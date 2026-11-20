#include "site/features/feed/feed_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/multipart.hpp"
#include "site/features/feed/composer.hpp"
#include "site/features/feed/post_block.hpp"
#include "site/features/map/map_rules.hpp"
#include "site/features/pains/pains.hpp"
#include "site/ui/wrap.hpp"

#include <algorithm>
#include <cstdint>
#include <map>

namespace liquidgov::site {

std::optional<std::string> feed_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    // Share to LiquidGov (2026-11-15): the installed app is a share target (manifest.webmanifest share_target), so a
    // phone's Share menu offers it from TikTok, X, YouTube and the rest; what was shared opens in the feed's post box,
    // ready to post or change. Title, text and link go in as one text (a link already in the text is not repeated).
    if(method=="GET"&&path=="/share") {
        if(!acc) return need_login();
        const std::string title = qs.count("title") ? qs.at("title") : "", text = qs.count("text") ? qs.at("text") : "",
                          url = qs.count("url") ? qs.at("url") : "";
        std::string out;
        auto add = [&](const std::string& part) { if (part.empty()) return; if (!out.empty()) out += "\n"; out += part; };
        if (text.find(title) == std::string::npos) add(title);
        add(text);
        if (url.empty() || text.find(url) == std::string::npos) add(url);
        if (out.size() > 2000) out.resize(2000);
        return http_redir("/feed#share=" + url_encode(out));
    }
    if(method=="GET"&&path=="/feed") {
        if(!acc) return need_login();
        // The choice lives only in the address; nothing new is stored (feed filters slice 1, wip/160). The two
        // orderings take the same posts as All and only change their order: Most discussed by the number of
        // replies each post shows (a tie keeps newest first), Friends first puts my friends' and my own posts
        // before the group posts (newest first in each part).
        std::string filter_key = qs.count("filter") ? qs.at("filter") : "all";
        FeedFilter filter = FeedFilter::All;
        if (filter_key == "friends") filter = FeedFilter::Friends;
        else if (filter_key == "committees") filter = FeedFilter::Committees;
        else if (filter_key != "discussed" && filter_key != "friendsfirst")
            filter_key = "all"; // anything else, an old ?filter=drawings link included, is All (no Drawings, Brandon 2026-11-10)
        auto tab = [&](const char* key, const char* label) {
            const bool active = filter_key == key;
            return "<a class=\"btn"+std::string(active?"\" aria-current=\"page":" ghost")+"\" href=\"/feed?filter="+key+"\">"+label+"</a>";
        };
        std::string html = "<h1>Feed</h1><p class=\"muted eyebrow\">"+html_escape(fmt_time(now()))+
            " &middot; "+std::to_string(g_store.account_count())+" members</p>"
            "<div class=\"feedtabs\"><div class=\"segtabs\">"+
            tab("all","All")+tab("friends","Friends")+tab("committees","Groups")+
            tab("discussed","Most discussed")+tab("friendsfirst","Friends first")+
            "</div></div>"+ // .feedtabs: on a phone the five tabs wrap instead of scrolling the chosen one out of sight
            flash_error(qs)+
            feed_composer_card(user);
        auto posts = g_store.feed_view(user, 80, filter); // newest first
        if (filter_key == "discussed") {
            std::map<std::uint64_t, std::size_t> replies;
            for (const auto& p : posts) replies[p.id] = g_store.replies_of(p.id).size();
            std::stable_sort(posts.begin(), posts.end(),
                [&](const Post& a, const Post& b) { return replies[a.id] > replies[b.id]; });
        } else if (filter_key == "friendsfirst") {
            std::stable_partition(posts.begin(), posts.end(),
                [&](const Post& p) { return p.author == user || g_store.are_friends(user, p.author); });
        }
        if (posts.empty()) html += "<div class=\"card\"><p class=\"empty\">Nothing in this view yet.</p>"
            "<p class=\"muted\">Write one above, or <a href=\"/friends\">add a friend</a> so theirs show up.</p></div>";
        for (const auto& p : posts) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
        return pg("Feed", html);
    }

    // /post-media/<blob_id> -- an image or video attached to a post.
    // Public and unauthenticated, same posture as /portrait/ and
    // /canvas-frame/: a post's own visibility is already decided by
    // wherever it renders (feed, group, profile), and the blob id itself
    // is a content hash, not a secret. blob_id comes straight from the
    // URL, though, so it's validated as a bare hex sha256 digest before
    // ever reaching BlobStore -- FilesystemBlobStore::path_for()
    // concatenates the id directly into a filesystem path with no
    // sanitization of its own, so anything looser here would be a real
    // path-traversal hole, not just a style nit.
    if(method=="GET"&&path.rfind("/post-media/",0)==0) {
        const std::string blob_id = path.substr(12);
        bool valid_hex = blob_id.size() == 64;
        if (valid_hex) {
            for (unsigned char c : blob_id) {
                if (!std::isxdigit(c)) { valid_hex = false; break; }
            }
        }
        if (!valid_hex) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto meta = g_blobs.meta(blob_id);
        auto bytes = g_blobs.get(blob_id);
        if (!meta || !bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string data(bytes->begin(), bytes->end());
        return http_raw("200 OK", meta->mime_type.c_str(), data,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline\r\n");
    }
    // One post on its own (the map's right-hand field opens this). Same
    // audience rules as the author's profile, via map_post_ok.
    if(method=="GET"&&path.rfind("/post/",0)==0&&path.size()>6&&path.find('/',6)==std::string::npos) {
        if(!acc) return need_login();
        std::uint64_t pid = 0;
        const std::string digits = path.substr(6);
        if (digits.size() <= 19 && digits.find_first_not_of("0123456789") == std::string::npos)
            try { pid = std::stoull(digits); } catch (...) { pid = 0; }
        auto p = pid ? g_store.post_by_id(pid) : std::nullopt;
        if (!p || !map_post_ok(*p, user))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such post.</p></div>"));
        return pg("Post", "<div class=\"card\">" + post_block(*p, user) + "</div>");
    }
    if(method=="POST"&&path=="/me/repost") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t pid = 0;
        try { pid = std::stoull(f["post_id"]); } catch (...) { return http_redir("/me"); }
        auto target = g_store.post_by_id(pid);
        if (!target) return http_redir("/me");
        // A repost of a repost still credits the true original author, not
        // the intermediate reposter -- see the repost_of_author comment on
        // Post.
        const std::string& origAuthor = target->repost_of_author.empty() ? target->author : target->repost_of_author;
        std::uint64_t origId = target->repost_of_author.empty() ? target->id : target->repost_of_id;
        if (g_store.is_blocked(user, origAuthor)) return http_redir("/me");
        std::string blob_id, mime;
        if (!target->attachment_blob_id.empty()) {
            // put() is content-addressed and idempotent -- re-putting the
            // same bytes lands on the exact same blob id while correctly
            // incrementing its refcount for this new post referencing it.
            auto bytes = g_blobs.get(target->attachment_blob_id);
            if (bytes) {
                blob_id = g_blobs.put(*bytes, target->attachment_mime);
                mime = target->attachment_mime;
            }
        }
        g_store.add_post(user, target->text, "", origAuthor, origId, blob_id, mime);
        return http_redir(back_to_post(header_value(req, "Referer"), pid)); // the page it was tapped on (was /me)
    }
    // Reply and second both need to land back on whatever page the post was
    // actually being viewed on (feed, a profile, a community) -- post_block
    // renders in all of those, so unlike most POST handlers here a fixed
    // redirect target would be wrong more often than not. Referer, same
    // pattern the theme toggle already uses, falling back to /feed.
    if(method=="POST"&&path.rfind("/post/",0)==0&&path.size()>12&&
       path.compare(path.size()-6,6,"/reply")==0) {
        if(!acc) return need_login();
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/feed";
        std::uint64_t pid = 0;
        try { pid = std::stoull(path.substr(6, path.size()-6-6)); } catch (...) { return http_redir(back); }
        if (!g_store.post_by_id(pid)) return http_redir(back);
        auto text = trim_ws(parse_form(body)["text"]);
        if (!text.empty()) g_store.add_reply(pid, user, text);
        return http_redir(back_to_post(back, pid));
    }
    if(method=="POST"&&path.rfind("/post/",0)==0&&path.size()>13&&
       path.compare(path.size()-7,7,"/second")==0) {
        if(!acc) return need_login();
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/feed";
        std::uint64_t pid = 0;
        try { pid = std::stoull(path.substr(6, path.size()-6-7)); } catch (...) { return http_redir(back); }
        if (!g_store.post_by_id(pid)) return http_redir(back);
        g_store.toggle_second(pid, user);
        return http_redir(back_to_post(back, pid));
    }
    if(method=="POST"&&path.rfind("/post/",0)==0&&path.size()>13&&
       path.compare(path.size()-7,7,"/delete")==0) {
        if(!acc) return need_login();
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/feed";
        std::uint64_t pid = 0;
        try { pid = std::stoull(path.substr(6, path.size()-6-7)); } catch (...) { return http_redir(back); }
        auto target = g_store.post_by_id(pid);
        if (!target) return http_redir(back); // already gone
        if (target->author != user) return http_code("403 Forbidden", "not your post");
        g_store.delete_post(pid, user);
        return http_redir(back);
    }
    if(method=="POST"&&path=="/feed") {
        if(!acc) return need_login();
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto c = read_composer_post(hdrs, body);
        if (!c.ok) return http_redir(std::string("/feed?err=")+c.err);
        auto text = trim_ws(c.text);
        if (!text.empty() || !c.attachment_blob_id.empty()) {
            // anybody/friends/groups and the except/friend/group lists come
            // from feed_composer_card's own hidden fields and checkbox
            // groups -- absent (JS-off, or an older cached page) means the
            // same static defaults that markup ships with: Friends, all
            // friends, no group, exactly today's one and only behavior
            // before this existed.
            std::string ctype = header_value(hdrs, "Content-Type");
            bool anybody, friends_on, groups_on;
            std::vector<std::string> except_raw, friend_raw, group_raw;
            if (ctype.find("multipart/form-data") != std::string::npos) {
                anybody = multipart_field(ctype, body, "anybody") == "1";
                friends_on = multipart_field(ctype, body, "friends") == "1";
                groups_on = multipart_field(ctype, body, "groups") == "1";
                except_raw = multipart_field_all(ctype, body, "except");
                friend_raw = multipart_field_all(ctype, body, "friend");
                group_raw = multipart_field_all(ctype, body, "group");
            } else {
                auto f = parse_form(body);
                anybody = f["anybody"] == "1";
                friends_on = f["friends"] == "1";
                groups_on = f["groups"] == "1";
                except_raw = parse_form_multi(body, "except");
                friend_raw = parse_form_multi(body, "friend");
                group_raw = parse_form_multi(body, "group");
            }
            // Anybody is exclusive; "nothing chosen" falls back to today's
            // only behavior (Friends, all friends) -- never trust the
            // client's own idea of these three past this normalization.
            if (anybody) { friends_on = false; groups_on = false; }
            if (!anybody && !friends_on && !groups_on) friends_on = true;

            const auto my_friends = g_store.friends_of(user);
            auto is_a_friend = [&](const std::string& n) {
                return std::find(my_friends.begin(), my_friends.end(), n) != my_friends.end();
            };
            std::vector<std::string> exceptions, friend_allowlist, groups;
            if (anybody) {
                for (const auto& n : except_raw) if (is_a_friend(trim_ws(n))) exceptions.push_back(trim_ws(n));
            }
            if (friends_on) {
                for (const auto& n : friend_raw) if (is_a_friend(trim_ws(n))) friend_allowlist.push_back(trim_ws(n));
            }
            // A group needs an actual, current membership -- same
            // join-to-post rule as posting from the community's own page
            // (see POST /c/<slug>). A checked slug that isn't (anymore) a
            // real membership is just dropped, not trusted from the client.
            if (groups_on) {
                for (const auto& s : group_raw) {
                    auto slug = trim_ws(s);
                    if (!slug.empty() && g_store.is_member(slug, user)) groups.push_back(slug);
                }
                if (groups.empty()) return http_redir("/feed?err=nogroup");
            }

            std::string community = groups.empty() ? "" : groups.front();
            std::vector<std::string> extra_groups(groups.empty() ? groups : std::vector<std::string>(groups.begin()+1, groups.end()));
            const PostAudience audience = anybody ? PostAudience::Anybody
                : (groups_on && !friends_on) ? PostAudience::GroupsOnly
                : PostAudience::Friends;
            const auto new_post = g_store.add_post(user, text, community, "", 0, c.attachment_blob_id, c.attachment_mime,
                              audience, exceptions, friend_allowlist, extra_groups);
            // Reposts are someone else's words. Anything else this person
            // wrote is noticed: Anybody can be Flowed; the rest stay private.
            if (new_post) {
                Post noted;
                noted.id = new_post;
                noted.audience = audience;
                noted.community = community;
                noted.extra_groups = extra_groups;
                notice_pains(user, text, post_pain_source(noted));
            }
        }
        return http_redir("/feed");
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
