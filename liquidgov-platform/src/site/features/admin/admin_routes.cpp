#include "site/features/admin/admin_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/http.hpp"
#include "site/features/admin/admin.hpp"
#include "site/features/mail/mail_attachments.hpp"
#include "site/core/multipart.hpp"
#include "site/features/preview/preview.hpp"

namespace liquidgov::site {

std::optional<std::string> admin_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/admin") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_overview_page());
    }
    if(method=="GET"&&path=="/admin/accounts") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_accounts_page(user, trim_ws(qs["q"])));
    }
    if(method=="GET"&&path=="/admin/posts") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_posts_page(qs.count("q") ? trim_ws(qs.at("q")) : ""));
    }
    if(method=="GET"&&path=="/admin/communities") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_communities_page());
    }
    if(method=="GET"&&path=="/admin/proposals") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        const std::string msg = qs.count("done") ? (qs.at("done") == "deleted" ? "Deleted; a copy is in the moderation archive."
                              : qs.at("done") == "edited" ? "Saved." : qs.at("done") == "archive-failed"
                              ? "Not deleted: the moderation archive could not be written (see the server log)." : "Nothing changed.") : "";
        return pg("Operator tools", admin_proposals_page(msg));
    }
    if(method=="POST"&&path=="/admin/proposals/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        std::uint64_t pid = 0;
        try { pid = std::stoull(parse_form(body)["id"]); } catch (...) { return http_redir("/admin/proposals?done=none"); }
        auto target = g_store.proposal_by_id(pid);
        if (!target) return http_redir("/admin/proposals?done=none");
        if (!archive_deleted_proposal(*target, user)) return http_redir("/admin/proposals?done=archive-failed");
        g_store.delete_proposal(pid);
        g_store.add_audit_log(user, "delete_proposal", std::to_string(pid) + " by " + target->author + ": " + target->title.substr(0, 60));
        return http_redir("/admin/proposals?done=deleted");
    }
    if(method=="GET"&&path=="/admin/comments") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        const std::string done = qs.count("done") ? qs.at("done") : "";
        const std::string msg = done == "deleted" ? "Deleted; a copy is in the moderation archive."
                              : done == "archive-failed" ? "Not deleted: the moderation archive could not be written (see the server log)."
                              : done.empty() ? "" : "Nothing changed.";
        return pg("Operator tools", admin_comments_page(qs.count("q") ? trim_ws(qs.at("q")) : "", msg));
    }
    if(method=="POST"&&path=="/admin/comments/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        std::uint64_t cid = 0;
        try { cid = std::stoull(f["id"]); } catch (...) { return http_redir("/admin/comments?done=none"); }
        if (f["on"] == "post") {
            auto r = g_store.post_reply_by_id(cid);
            if (!r) return http_redir("/admin/comments?done=none");
            if (!archive_deleted_comment("post", r->id, r->post_id, r->author, r->ts, r->text, user))
                return http_redir("/admin/comments?done=archive-failed");
            g_store.delete_post_reply(cid);
            g_store.add_audit_log(user, "delete_comment", "post comment " + std::to_string(cid) + " by " + r->author);
        } else if (f["on"] == "proposal") {
            auto r = g_store.proposal_reply_by_id(cid);
            if (!r) return http_redir("/admin/comments?done=none");
            if (!archive_deleted_comment("proposal", r->id, r->proposal_id, r->author, r->ts, r->text, user))
                return http_redir("/admin/comments?done=archive-failed");
            g_store.delete_proposal_reply(cid);
            g_store.add_audit_log(user, "delete_comment", "proposal comment " + std::to_string(cid) + " by " + r->author);
        } else {
            return http_redir("/admin/comments?done=none");
        }
        return http_redir("/admin/comments?done=deleted");
    }
    if(method=="GET"&&path=="/admin/departments") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        const std::string done = qs.count("done") ? qs.at("done") : "";
        const std::string msg = done == "deleted" ? "Deleted; a copy is in the moderation archive."
                              : done == "archive-failed" ? "Not deleted: the moderation archive could not be written (see the server log)."
                              : done.empty() ? "" : "Nothing changed.";
        return pg("Operator tools", admin_departments_page(qs.count("q") ? trim_ws(qs.at("q")) : "", msg));
    }
    if(method=="POST"&&path=="/admin/departments/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        const std::string key = f["city"], tag = f["tag"];
        bool known = false;
        for (const auto& t : g_store.city_tags(key)) if (t == tag) { known = true; break; }
        if (key.empty() || !known) return http_redir("/admin/departments?done=none");
        const auto props = g_store.proposals_with_city_tag(key, tag);
        if (!archive_deleted_city_tag(key, tag, props, user)) return http_redir("/admin/departments?done=archive-failed");
        if (!g_store.delete_city_tag(key, tag)) return http_redir("/admin/departments?done=none");
        g_store.add_audit_log(user, "delete_city_tag", key + ": " + tag.substr(0, 80) + " (" + std::to_string(props.size()) + " proposal(s))");
        return http_redir("/admin/departments?done=deleted");
    }
    if(method=="POST"&&path=="/admin/proposals/edit") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        std::uint64_t pid = 0;
        try { pid = std::stoull(f["id"]); } catch (...) { return http_redir("/admin/proposals?done=none"); }
        const std::string title = trim_ws(f["title"]).substr(0, 200);
        const std::string text = f["body"].substr(0, 10000);
        if (!g_store.edit_proposal(pid, title, text)) return http_redir("/admin/proposals?done=none");
        g_store.add_audit_log(user, "edit_proposal", std::to_string(pid));
        return http_redir("/admin/proposals?done=edited");
    }
    if(method=="GET"&&path=="/admin/styles") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        const std::string done = qs.count("done") ? qs.at("done") : "";
        const std::string msg = done == "deleted" ? "Taken down; a copy is in the moderation archive."
                              : done == "dismissed" ? "Reports dismissed."
                              : done == "archive-failed" ? "Not taken down: the moderation archive could not be written (see the server log)."
                              : done.empty() ? "" : "Nothing changed.";
        return pg("Operator tools", admin_styles_page(msg));
    }
    if(method=="POST"&&(path=="/admin/styles/takedown"||path=="/admin/styles/dismiss")) {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        std::uint64_t sid = 0;
        const std::string raw = parse_form(body)["id"];
        if (raw.empty() || raw.size() > 19 || !std::all_of(raw.begin(), raw.end(), [](char c) { return c >= '0' && c <= '9'; }))
            return http_redir("/admin/styles?done=none");
        sid = std::stoull(raw);
        auto s = g_store.style(sid);
        if (!s) return http_redir("/admin/styles?done=none");
        if (path == "/admin/styles/dismiss") {
            g_store.dismiss_style_reports(sid);
            g_store.add_audit_log(user, "dismiss_style_reports", std::to_string(sid) + " by " + s->author + ": " + s->name.substr(0, 60));
            return http_redir("/admin/styles?done=dismissed");
        }
        if (!archive_deleted_style(*s, user)) return http_redir("/admin/styles?done=archive-failed");
        g_store.remove_style(sid);
        g_store.add_audit_log(user, "delete_style", std::to_string(sid) + " by " + s->author + ": " + s->name.substr(0, 60));
        return http_redir("/admin/styles?done=deleted");
    }
    // the site's link preview card (preview.hpp): the words, the picture, and a way back to the built-in one
    if(method=="GET"&&path=="/admin/preview") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_preview_page(qs.count("done") ? qs.at("done") : std::string()));
    }
    if(method=="POST"&&path=="/admin/preview") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        return http_redir(std::string("/admin/preview?done=") + (site_preview_save_text(f["title"], f["description"]) ? "saved" : "write"));
    }
    if(method=="POST"&&path=="/admin/preview/image") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto bytes = multipart_file(header_value(req, "Content-Type"), body, "image");
        if (!bytes || bytes->empty()) return http_redir("/admin/preview?done=noimage");
        const std::string why = site_preview_set_image(*bytes);
        return http_redir("/admin/preview?done=" + (why.empty() ? std::string("image") : why));
    }
    if(method=="POST"&&path=="/admin/preview/image-remove") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        site_preview_remove_image();
        return http_redir("/admin/preview?done=imagegone");
    }
    if(method=="POST"&&path=="/admin/preview/reset") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        site_preview_reset();
        return http_redir("/admin/preview?done=reset");
    }
    if(method=="GET"&&path=="/admin/log") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_log_page());
    }
    if(method=="POST"&&path=="/admin/accounts/lock") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto target = trim_ws(parse_form(body)["username"]);
        if (!target.empty() && target != user && g_store.find_by_username(target)) {
            g_store.set_locked(target, true);
            g_store.add_audit_log(user, "lock", target);
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/programs") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto form = parse_form(body);
        auto target = trim_ws(form["username"]);
        if (!target.empty() && g_store.find_by_username(target) && !is_operator(target)) {
            ProgramGrant g;
            g.username = target;
            for (const auto& k : program_kinds())
                if (!form[std::string("k_") + k.slug].empty()) g.kinds |= static_cast<std::uint32_t>(k.kind);
            std::uint64_t mb = 0;
            try { mb = std::stoull(trim_ws(form["max_mb"])); } catch (...) { mb = 0; }
            const std::uint64_t ceiling_mb = program_ceiling_bytes() / (1024 * 1024);
            mb = std::max<std::uint64_t>(1, std::min<std::uint64_t>(mb, ceiling_mb));
            g.max_bytes = mb * 1024 * 1024;
            g.granted_by = user;
            g.granted_at = now();
            const bool had = g_store.program_grant(target).has_value();
            g_store.set_program_grant(g);
            if (g.kinds) g_store.add_audit_log(user, "programs_grant", target);
            else if (had) g_store.add_audit_log(user, "programs_revoke", target);
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/unlock") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto target = trim_ws(parse_form(body)["username"]);
        if (!target.empty() && g_store.find_by_username(target)) {
            g_store.set_locked(target, false);
            g_store.add_audit_log(user, "unlock", target);
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/logout-all") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto target = trim_ws(parse_form(body)["username"]);
        if (!target.empty() && target != user) {
            if (auto ta = g_store.find_by_username(target)) {
                logout_all_devices(g_store, ta->id, "");
                g_store.add_audit_log(user, "logout_all", target);
            }
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        auto target = trim_ws(f["username"]);
        auto confirm = trim_ws(f["confirm"]);
        if (!target.empty() && target != user && confirm_says_delete(confirm)) {
            if (auto ta = g_store.find_by_username(target)) {
                clear_avatar(g_blobs, g_store, ta->id);
                g_store.delete_account(ta->id);
                g_store.add_audit_log(user, "delete_account", target);
            }
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/posts/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        std::uint64_t pid = 0;
        try { pid = std::stoull(parse_form(body)["id"]); } catch (...) { return http_redir("/admin/posts"); }
        if (auto target = g_store.post_by_id(pid)) {
            g_store.delete_post(pid, target->author);
            g_store.add_audit_log(user, "delete_post", std::to_string(pid));
        }
        return http_redir("/admin/posts");
    }
    if(method=="POST"&&path=="/admin/communities/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        auto slug = trim_ws(f["slug"]);
        auto confirm = trim_ws(f["confirm"]);
        if (!slug.empty() && confirm_says_delete(confirm)) {
            if (auto c = g_store.community(slug)) {
                g_store.delete_community(slug, c->owner);
                g_store.add_audit_log(user, "delete_community", slug);
            }
        }
        return http_redir("/admin/communities");
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
