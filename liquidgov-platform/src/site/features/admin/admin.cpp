#include "site/features/admin/admin.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/text.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/features/mail/mail_attachments.hpp"
#include "site/features/preview/preview.hpp"

namespace liquidgov::site {
// Genesis operator tools -- /admin and friends. Not Phase 2, not CitizenId,
// not an election: this is whoever is_operator() names keeping the
// *website* running (locking a spam account, deleting a post, closing a
// community that's gone bad), not a second office. Same login as everyone
// else -- is_operator() gates by username, not a second credential -- no
// shell, no direct file access, no impersonation. Every route below checks
// is_operator() on both GET and POST; a signed-in non-operator gets a
// plain 404 (see handle()'s admin_404 lambda), so /admin isn't advertised
// to anyone it isn't for, and it never appears in the public nav -- only
// in the operator's own account menu (see nav()).
std::string admin_nav(const char* active) {
    auto tab = [&](const char* href, const char* key, const char* label) {
        const bool on = std::string(active) == key;
        return "<a class=\"btn"+std::string(on?"":" ghost")+"\" href=\""+href+"\">"+label+"</a>";
    };
    return "<div class=\"row\" style=\"margin:.5rem 0 1.2rem\">"
        +tab("/admin","overview","Overview")
        +tab("/admin/accounts","accounts","Accounts")
        +tab("/admin/posts","posts","Posts")
        +tab("/admin/proposals","proposals","Proposals")
        +tab("/admin/comments","comments","Comments")
        +tab("/admin/departments","departments","Departments")
        +tab("/admin/styles","styles","Styles")
        +tab("/admin/preview","preview","Preview")
        +tab("/admin/communities","communities","Groups")
        +tab("/admin/log","log","Log")
        +"</div>";
}

std::string admin_header() {
    return "<h1>Operator tools</h1>"
           "<p class=\"muted\">Temporary. Not a seat. See <a href=\"/control\">who holds control</a> "
           "for the public disclosure this backs.</p>";
}

std::string fmt_bytes(std::size_t n) {
    if (n < 1024) return std::to_string(n)+" B";
    if (n < 1024*1024) return std::to_string(n/1024)+" KiB";
    return std::to_string(n/(1024*1024))+" MiB";
}

std::string admin_overview_page() {
    std::size_t store_bytes = 0;
    {
        std::ifstream in(store_path_from_env(), std::ios::binary | std::ios::ate);
        if (in) store_bytes = static_cast<std::size_t>(in.tellg());
    }
    return admin_header() + admin_nav("overview") +
        "<div class=\"stattiles\">"
        "<div class=\"stattile\"><strong>"+std::to_string(g_store.account_count())+"</strong><span>Accounts</span></div>"
        "<div class=\"stattile\"><strong>"+std::to_string(g_store.all_communities().size())+"</strong><span>Groups</span></div>"
        "<div class=\"stattile\"><strong>"+std::to_string(g_store.post_count())+"</strong><span>Posts</span></div>"
        "<div class=\"stattile\"><strong>"+std::to_string(g_store.all_proposals().size())+"</strong><span>Open proposals</span></div>"
        "</div>"
        "<p class=\"muted\" style=\"margin-top:.8rem\">Store file: "+fmt_bytes(store_bytes)+"</p>"
        "<p class=\"muted\">You cannot restart the process from here &mdash; that's still SSH and systemd.</p>";
}

// The "Programs in mail" cell: what this person may send, and a form to
// choose the kinds and the size (one person at a time -- each gets their
// own list and limit). Operators always have every kind up to the ceiling.
std::string admin_programs_cell(const std::string& username) {
    const std::uint64_t ceiling_mb = program_ceiling_bytes() / (1024 * 1024);
    if (is_operator(username))
        return "<span class=\"muted\">Every kind, up to " + fmt_size(program_ceiling_bytes()) + " (admin)</span>";
    auto g = g_store.program_grant(username);
    std::string html = g ? html_escape(program_kinds_text(g->kinds)) + "<br><span class=\"muted\">up to " +
                               fmt_size(std::min<std::uint64_t>(g->max_bytes, program_ceiling_bytes())) + "</span>"
                         : std::string("<span class=\"muted\">None</span>");
    const std::uint64_t cur_mb = g ? std::max<std::uint64_t>(1, std::min<std::uint64_t>(g->max_bytes / (1024 * 1024), ceiling_mb)) : 25;
    html += "<details><summary class=\"muted\" style=\"cursor:pointer\">" + std::string(g ? "Change&hellip;" : "Allow&hellip;") +
            "</summary><form method=\"POST\" action=\"/admin/accounts/programs\" style=\"margin-top:.3rem\">"
            "<input type=\"hidden\" name=\"username\" value=\"" + html_escape(username) + "\">";
    for (const auto& k : program_kinds()) {
        const bool on = g && (g->kinds & static_cast<std::uint32_t>(k.kind));
        html += std::string("<label style=\"display:block\"><input type=\"checkbox\" name=\"k_") + k.slug + "\"" +
                (on ? " checked" : "") + "> " + html_escape(k.label) + "</label>";
    }
    html += "<label style=\"display:block;margin-top:.3rem\">Largest file (MB) "
            "<input type=\"number\" name=\"max_mb\" min=\"1\" max=\"" + std::to_string(ceiling_mb) +
            "\" step=\"1\" value=\"" + std::to_string(cur_mb) + "\" style=\"width:5rem\"></label>"
            "<p class=\"muted\" style=\"margin:.2rem 0\">Server limit " + fmt_size(program_ceiling_bytes()) +
            ". Untick everything to stop them sending programs. Programs are not virus-checked: allow only people "
            "you trust, and only the kinds they need.</p>"
            "<button class=\"ghost\" type=\"submit\">Save</button></form></details>";
    return html;
}

std::string admin_accounts_page(const std::string& viewer, const std::string& q) {
    std::string html = admin_header() + admin_nav("accounts") +
        "<form method=\"GET\" action=\"/admin/accounts\" class=\"row\" style=\"margin-bottom:.8rem\">"
        "<input name=\"q\" value=\""+html_escape(q)+"\" placeholder=\"Filter by username\" style=\"flex:1\">"
        "<button type=\"submit\" class=\"ghost\">Filter</button></form>";
    auto accounts = g_store.all_accounts();
    std::vector<PlatformAccount> shown;
    for (const auto& a : accounts) {
        if (q.empty() || a.username.compare(0, q.size(), q) == 0) shown.push_back(a);
    }
    if (shown.empty()) {
        html += "<p class=\"empty\">No accounts match.</p>";
        return html;
    }
    html += "<div style=\"overflow-x:auto\"><table class=\"register\"><tr>"
            "<th>Username</th><th>Created</th><th>Locked</th><th>Sessions</th><th>Programs in mail</th><th>Actions</th></tr>";
    for (const auto& a : shown) {
        const bool locked = g_store.is_locked(a.username);
        const bool self = a.username == viewer;
        html += "<tr><td><a href=\"/u/"+html_escape(a.username)+"\">"+html_escape(a.username)+"</a></td>"
                "<td>"+html_escape(fmt_time(a.created_at))+"</td>"
                "<td>"+std::string(locked?"Yes":"No")+"</td>"
                "<td>"+std::to_string(g_store.sessions_for(a.id).size())+"</td>"
                "<td>"+admin_programs_cell(a.username)+"</td><td>";
        if (self) {
            html += "<span class=\"muted\">(you)</span>";
        } else {
            html += "<form class=\"inline\" method=\"POST\" action=\"/admin/accounts/"+
                    std::string(locked?"unlock":"lock")+"\">"
                    "<input type=\"hidden\" name=\"username\" value=\""+html_escape(a.username)+"\">"
                    "<button class=\"ghost\" type=\"submit\">"+std::string(locked?"Unlock":"Lock")+"</button></form> "
                    "<form class=\"inline\" method=\"POST\" action=\"/admin/accounts/logout-all\">"
                    "<input type=\"hidden\" name=\"username\" value=\""+html_escape(a.username)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Sign out all devices</button></form> "
                    "<details style=\"display:inline-block\"><summary class=\"muted\" style=\"cursor:pointer;display:inline\">Delete&hellip;</summary>"
                    "<form method=\"POST\" action=\"/admin/accounts/delete\" style=\"margin-top:.3rem\">"
                    "<input type=\"hidden\" name=\"username\" value=\""+html_escape(a.username)+"\">"
                    "<input name=\"confirm\" placeholder=\"type &ldquo;delete&rdquo;\" autocomplete=\"off\">"
                    "<button class=\"danger\" type=\"submit\">Delete account</button></form></details>";
        }
        html += "</td></tr>";
    }
    html += "</table></div>";
    return html;
}

// Case-insensitive "does hay contain needle" for operator searches.
bool admin_matches(const std::string& hay, const std::string& needle) {
    auto low = [](std::string s) { for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
    return low(hay).find(low(needle)) != std::string::npos;
}

std::string admin_search_form(const char* action, const std::string& q, const char* hint = "Author or words") {
    return "<form method=\"GET\" action=\"" + std::string(action) + "\" class=\"row\" style=\"margin:0 0 1rem\">"
           "<input name=\"q\" placeholder=\"" + std::string(hint) + "\" value=\"" + html_escape(q) + "\" style=\"flex:1;font-size:16px\">"
           "<button type=\"submit\">Search</button></form>";
}

std::string admin_posts_page(const std::string& q) {
    std::string html = admin_header() + admin_nav("posts") + admin_search_form("/admin/posts", q);
    // Without a search: the 80 newest. With one: every post whose author is q or whose text contains q, newest first
    // (Brandon 2026-11-08: older abusive posts could not be reached from the 80-newest list).
    std::vector<Post> posts;
    if (q.empty()) {
        posts = g_store.recent_posts(80);
    } else {
        auto all = g_store.all_posts();
        for (auto it = all.rbegin(); it != all.rend() && posts.size() < 500; ++it)
            if (admin_matches(it->author, q) || admin_matches(it->text, q)) posts.push_back(*it);
        html += "<p class=\"muted\">" + std::to_string(posts.size()) + (posts.size() == 500 ? "+" : "") + " matching.</p>";
    }
    if (posts.empty()) {
        html += "<p class=\"empty\">" + std::string(q.empty() ? "No posts yet." : "No post matches.") + "</p>";
        return html;
    }
    html += "<div style=\"overflow-x:auto\"><table class=\"register\"><tr>"
            "<th>Id</th><th>Author</th><th>When</th><th>Text</th><th></th></tr>";
    for (const auto& p : posts) {
        std::string excerpt = p.text.substr(0, 80);
        if (p.text.size() > 80) excerpt += "\xE2\x80\xA6";
        html += "<tr><td>"+std::to_string(p.id)+"</td>"
                "<td><a href=\"/u/"+html_escape(p.author)+"\">"+html_escape(p.author)+"</a></td>"
                "<td>"+html_escape(fmt_time(p.ts))+"</td>"
                "<td>"+html_escape(excerpt)+"</td>"
                "<td><form method=\"POST\" action=\"/admin/posts/delete\" "
                "onsubmit=\"return confirm('Delete this post?');\">"
                "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                "<button class=\"danger\" type=\"submit\">Delete</button></form></td></tr>";
    }
    html += "</table></div>";
    return html;
}

// Operator moderation of proposals (Brandon 2026-11-08: "make it so i can delete and edit proposals for now? people
// are abusing it"). Before a delete, the proposal and its comments are appended to an archive next to the store
// (<store>.moderation.jsonl; its name matches the deploy sync's '*.lgp.*' exclusion, so no deploy can touch it), so
// the evidence survives and a hand restore is possible. If the archive can't be written, the delete is refused.
bool archive_deleted_proposal(const Proposal& p, const std::string& by) {
    std::string line = "{\"deleted_by\":\"" + json_escape(by) + "\",\"deleted_at\":" + std::to_string(std::time(nullptr)) +
        ",\"id\":" + std::to_string(p.id) + ",\"author\":\"" + json_escape(p.author) + "\",\"ts\":" + std::to_string(p.ts) +
        ",\"city\":\"" + json_escape(p.city) + "\",\"title\":\"" + json_escape(p.title) + "\",\"body\":\"" + json_escape(p.body) +
        "\",\"reactions\":{";
    bool first = true;
    for (const auto& rc : g_store.proposal_reaction_counts(p.id)) {
        line += std::string(first ? "" : ",") + "\"" + json_escape(rc.first) + "\":" + std::to_string(rc.second);
        first = false;
    }
    line += "},\"comments\":[";
    first = true;
    for (const auto& r : g_store.proposal_replies_of(p.id)) {
        line += std::string(first ? "" : ",") + "{\"author\":\"" + json_escape(r.author) + "\",\"ts\":" + std::to_string(r.ts) +
                ",\"text\":\"" + json_escape(r.text) + "\"}";
        first = false;
    }
    line += "]}\n";
    const std::string path = std::string(store_path_from_env()) + ".moderation.jsonl";
    std::ofstream out(path, std::ios::app | std::ios::binary);
    if (!out) {
        std::cerr << "moderation archive not writable: " << path << "\n";
        return false;
    }
    out << line;
    out.flush();
    return static_cast<bool>(out);
}

std::string admin_proposals_page(const std::string& msg) {
    std::string html = admin_header() + admin_nav("proposals");
    if (!msg.empty()) html += "<p class=\"muted\" role=\"status\">" + html_escape(msg) + "</p>";
    auto props = g_store.all_proposals();
    if (props.empty()) {
        html += "<p class=\"empty\">No proposals yet.</p>";
        return html;
    }
    html += "<p class=\"muted\">Newest first. Delete keeps a copy (with its comments) in the moderation archive on the "
            "server; it removes the proposal, its comments, reactions and votes from the site.</p>"
            "<div style=\"overflow-x:auto\"><table class=\"register\"><tr>"
            "<th>Id</th><th>Author</th><th>When</th><th>Proposal</th><th></th></tr>";
    for (auto it = props.rbegin(); it != props.rend(); ++it) {
        const auto& p = *it;
        const std::string id = std::to_string(p.id);
        std::string excerpt = p.body.substr(0, 120);
        if (p.body.size() > 120) excerpt += "\xE2\x80\xA6";
        html += "<tr><td><a href=\"/proposals?id=" + id + "\">" + id + "</a></td>"
                "<td><a href=\"/u/" + html_escape(p.author) + "\">" + html_escape(p.author) + "</a></td>"
                "<td>" + html_escape(fmt_time(p.ts)) + "</td>"
                // runs for office (/officials) are proposals too; label them so the operator knows where they show
                "<td>" + std::string(p.is_office ? "<span class=\"chip\">Officials: " + html_escape(p.jurisdiction) + "</span> " : "") +
                "<strong>" + html_escape(p.title) + "</strong><br><span class=\"muted\">" + html_escape(excerpt) + "</span>"
                "<details><summary>Edit</summary>"
                "<form method=\"POST\" action=\"/admin/proposals/edit\">"
                "<input type=\"hidden\" name=\"id\" value=\"" + id + "\">"
                "<label>Title <input name=\"title\" required maxlength=\"200\" value=\"" + html_escape(p.title) + "\"></label>"
                "<label>Text <textarea name=\"body\" rows=\"5\" maxlength=\"10000\">" + html_escape(p.body) + "</textarea></label>"
                "<button type=\"submit\">Save</button></form></details></td>"
                "<td><form method=\"POST\" action=\"/admin/proposals/delete\" "
                "onsubmit=\"return confirm('Delete proposal " + id + " with its comments and votes? A copy is kept in the moderation archive.');\">"
                "<input type=\"hidden\" name=\"id\" value=\"" + id + "\">"
                "<button class=\"danger\" type=\"submit\">Delete</button></form></td></tr>";
    }
    html += "</table></div>";
    return html;
}

// One profile style taken down by an operator (Brandon 2026-11-12, style library), archived like a deleted proposal:
// same <store>.moderation.jsonl, with its CSS, use count and reports; the take-down is refused if the archive can't
// be written. Copies people made stay in their own Profile CSS.
bool archive_deleted_style(const ProfileStyle& s, const std::string& by) {
    std::string line = "{\"deleted_by\":\"" + json_escape(by) + "\",\"deleted_at\":" + std::to_string(std::time(nullptr)) +
        ",\"kind\":\"style\",\"id\":" + std::to_string(s.id) + ",\"author\":\"" + json_escape(s.author) +
        "\",\"ts\":" + std::to_string(s.created_at) + ",\"name\":\"" + json_escape(s.name) + "\",\"css\":\"" +
        json_escape(s.css) + "\",\"uses\":" + std::to_string(g_store.style_use_count(s.id)) + ",\"reports\":[";
    bool first = true;
    for (const auto& r : g_store.style_reports(s.id)) {
        line += std::string(first ? "" : ",") + "{\"by\":\"" + json_escape(r.reporter) + "\",\"ts\":" +
                std::to_string(r.at) + ",\"reason\":\"" + json_escape(r.reason) + "\"}";
        first = false;
    }
    line += "]}\n";
    const std::string path = std::string(store_path_from_env()) + ".moderation.jsonl";
    std::ofstream out(path, std::ios::app | std::ios::binary);
    if (!out) {
        std::cerr << "moderation archive not writable: " << path << "\n";
        return false;
    }
    out << line;
    out.flush();
    return static_cast<bool>(out);
}

// Every style in the library, reported ones first (most reports first), then newest. The preview is the same
// sandboxed iframe /styles shows, so a hostile style can't reach this page either.
// The link preview card (Brandon 2026-11-16: "i just want to be able to change the preview label thing myself from the
// admin controls page"): what Facebook, X, Slack and others draw when someone shares a liquidgov.us link.
std::string admin_preview_page(const std::string& done) {
    static const std::pair<const char*, const char*> kMsgs[] = {
        {"saved", "Saved."}, {"image", "Picture saved."}, {"imagegone", "Picture removed: the built-in picture is back."},
        {"reset", "Reset: the built-in preview is back."}, {"type", "That file is not a JPEG or PNG picture."},
        {"size", "That picture is over 2 MB."}, {"noimage", "Choose a picture first."}, {"write", "Could not save (the server could not write the file)."}};
    std::string html = admin_header() + admin_nav("preview");
    for (const auto& m : kMsgs)
        if (done == m.first) html += "<p class=\"muted\" role=\"status\">" + std::string(m.second) + "</p>";
    const SitePreview p = site_preview();
    const std::string title = p.title.empty() ? "(the page's own title) \xE2\x80\x94 LiquidGov.US" : p.title;
    const std::string desc = p.description.empty() ? preview_builtin_description() : p.description;
    const std::string img = p.image_mime.empty() ? "/assets/og-image-v4.jpg" : "/preview-image?v=" + p.image_version;
    html += "<div class=\"card\"><h2>Link preview</h2>"
            "<p class=\"muted\">What Facebook, X, Slack and the like show when someone shares a liquidgov.us link. "
            "Leave a field blank to keep the built-in one. Those sites keep what they have already seen for a while; "
            "after a change, their own debugger tools can fetch it again.</p>"
            "<h3>How it looks now</h3>"
            "<div style=\"max-width:30rem;border:1px solid var(--rule,#ccc);border-radius:10px;overflow:hidden\">"
            "<img src=\"" + html_escape(img) + "\" alt=\"\" style=\"display:block;width:100%;aspect-ratio:1200/630;object-fit:cover\">"
            "<div style=\"padding:.6rem .8rem\"><div class=\"muted\" style=\"font-size:.75rem\">LIQUIDGOV.US</div>"
            "<strong>" + html_escape(title) + "</strong>"
            "<div class=\"muted\" style=\"font-size:.9rem;margin-top:.2rem\">" + html_escape(desc) + "</div></div></div></div>"
            "<div class=\"card\"><h3>Words</h3>"
            "<form method=\"POST\" action=\"/admin/preview\">"
            "<label>Title (up to 120 characters; blank = each page's own title)</label>"
            "<input name=\"title\" maxlength=\"120\" value=\"" + html_escape(p.title) + "\">"
            "<label>Description (up to 300 characters; blank = the built-in line)</label>"
            "<textarea name=\"description\" rows=\"3\" maxlength=\"300\">" + html_escape(p.description) + "</textarea>"
            "<div class=\"row\" style=\"margin-top:.5rem\"><button>Save the words</button></div></form></div>"
            "<div class=\"card\"><h3>Picture</h3>"
            "<p class=\"muted\">1200 \xC3\x97 630 pixels works best. JPEG or PNG, up to 2 MB.</p>"
            "<form method=\"POST\" action=\"/admin/preview/image\" enctype=\"multipart/form-data\">"
            "<input type=\"file\" name=\"image\" accept=\"image/jpeg,image/png\">"
            "<div class=\"row\" style=\"margin-top:.5rem\"><button>Upload the picture</button></div></form>"
            + (p.image_mime.empty() ? std::string() :
               "<form method=\"POST\" action=\"/admin/preview/image-remove\" style=\"margin-top:.5rem\">"
               "<button class=\"ghost\">Remove the picture (use the built-in one)</button></form>") +
            "</div>"
            "<div class=\"card\"><form method=\"POST\" action=\"/admin/preview/reset\">"
            "<button class=\"ghost\">Reset everything to the built-in preview</button></form></div>";
    return html;
}

std::string admin_styles_page(const std::string& msg) {
    std::string html = admin_header() + admin_nav("styles");
    if (!msg.empty()) html += "<p class=\"muted\" role=\"status\">" + html_escape(msg) + "</p>";
    auto all = g_store.styles();
    if (all.empty()) {
        html += "<p class=\"empty\">No styles in the library.</p>";
        return html;
    }
    std::stable_sort(all.begin(), all.end(), [](const ProfileStyle& a, const ProfileStyle& b) {
        return g_store.style_reports(a.id).size() > g_store.style_reports(b.id).size();
    });
    html += "<p class=\"muted\">Reported first. Take down keeps a copy (CSS and reports) in the moderation archive "
            "on the server and removes the style from the library; copies people already made stay theirs. Dismiss "
            "clears the reports and keeps the style.</p><div class=\"lg-style-grid\">";
    for (const auto& s : all) {
        const std::string id = std::to_string(s.id);
        auto reps = g_store.style_reports(s.id);
        html += "<article class=\"card lg-style-card\" id=\"style-" + id + "\">"
                "<iframe class=\"lg-style-preview\" sandbox src=\"/styles/" + id + "/preview\" loading=\"lazy\" "
                "referrerpolicy=\"no-referrer\" tabindex=\"-1\" title=\"Preview of " + html_escape(s.name) + "\"></iframe>"
                "<h3 class=\"lg-style-name\">" + html_escape(s.name) + "</h3>"
                "<p class=\"muted\">#" + id + " by <a href=\"/u/" + html_escape(s.author) + "\">" + html_escape(s.author) +
                "</a> &middot; " + html_escape(fmt_time(s.created_at)) + " &middot; " +
                std::to_string(g_store.style_use_count(s.id)) + (g_store.style_use_count(s.id) == 1 ? " use" : " uses") + "</p>";
        if (!reps.empty()) {
            html += "<p><strong>" + std::to_string(reps.size()) + (reps.size() == 1 ? " report" : " reports") +
                    "</strong></p><ul class=\"plain\">";
            for (const auto& r : reps)
                html += "<li><a href=\"/u/" + html_escape(r.reporter) + "\">" + html_escape(r.reporter) + "</a>: " +
                        (r.reason.empty() ? "<span class=\"muted\">(no reason given)</span>" : html_escape(r.reason)) + "</li>";
            html += "</ul>";
        }
        html += "<details><summary>CSS</summary><pre style=\"white-space:pre-wrap;overflow-wrap:anywhere\">" +
                html_escape(s.css) + "</pre></details><div class=\"row\">"
                "<form class=\"inline\" method=\"POST\" action=\"/admin/styles/takedown\" "
                "onsubmit=\"return confirm('Take this style down? A copy is kept in the moderation archive.');\">"
                "<input type=\"hidden\" name=\"id\" value=\"" + id + "\"><button class=\"danger\" type=\"submit\">Take down</button></form>";
        if (!reps.empty())
            html += "<form class=\"inline\" method=\"POST\" action=\"/admin/styles/dismiss\">"
                    "<input type=\"hidden\" name=\"id\" value=\"" + id + "\"><button class=\"ghost\" type=\"submit\">Dismiss reports</button></form>";
        html += "</div></article>";
    }
    html += "</div>";
    return html;
}

// One deleted comment, archived like a deleted proposal (same <store>.moderation.jsonl; refused if unwritable).
bool archive_deleted_comment(const char* on, std::uint64_t id, std::uint64_t parent, const std::string& author,
                             std::int64_t ts, const std::string& text, const std::string& by) {
    const std::string line = "{\"deleted_by\":\"" + json_escape(by) + "\",\"deleted_at\":" + std::to_string(std::time(nullptr)) +
        ",\"comment_on\":\"" + on + "\",\"comment_id\":" + std::to_string(id) + ",\"parent_id\":" + std::to_string(parent) +
        ",\"author\":\"" + json_escape(author) + "\",\"ts\":" + std::to_string(ts) + ",\"text\":\"" + json_escape(text) + "\"}\n";
    const std::string path = std::string(store_path_from_env()) + ".moderation.jsonl";
    std::ofstream out(path, std::ios::app | std::ios::binary);
    if (!out) {
        std::cerr << "moderation archive not writable: " << path << "\n";
        return false;
    }
    out << line;
    out.flush();
    return static_cast<bool>(out);
}

// Comments on posts and on proposals, newest first, each deletable on its own (Brandon 2026-11-08). q searches
// author and text across all of them.
std::string admin_comments_page(const std::string& q, const std::string& msg) {
    std::string html = admin_header() + admin_nav("comments") + admin_search_form("/admin/comments", q);
    if (!msg.empty()) html += "<p class=\"muted\" role=\"status\">" + html_escape(msg) + "</p>";
    const std::size_t limit = q.empty() ? 100 : 100000;
    struct Row { std::string on, parent_href, author, text; std::uint64_t id; std::int64_t ts; };
    std::vector<Row> rows;
    for (const auto& r : g_store.recent_post_replies(limit))
        rows.push_back({"post", "/post/" + std::to_string(r.post_id), r.author, r.text, r.id, r.ts});
    for (const auto& r : g_store.recent_proposal_replies(limit))
        rows.push_back({"proposal", "/proposals?id=" + std::to_string(r.proposal_id), r.author, r.text, r.id, r.ts});
    if (!q.empty())
        rows.erase(std::remove_if(rows.begin(), rows.end(), [&](const Row& r) {
            return !admin_matches(r.author, q) && !admin_matches(r.text, q); }), rows.end());
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.ts > b.ts; });
    if (rows.size() > 300) rows.resize(300);
    if (rows.empty()) {
        html += "<p class=\"empty\">" + std::string(q.empty() ? "No comments yet." : "No comment matches.") + "</p>";
        return html;
    }
    html += "<p class=\"muted\">Delete removes only that comment; a copy goes to the moderation archive first.</p>"
            "<div style=\"overflow-x:auto\"><table class=\"register\"><tr><th>On</th><th>Author</th><th>When</th><th>Comment</th><th></th></tr>";
    for (const auto& r : rows) {
        std::string excerpt = r.text.substr(0, 160);
        if (r.text.size() > 160) excerpt += "\xE2\x80\xA6";
        html += "<tr><td><a href=\"" + r.parent_href + "\">" + r.on + "</a></td>"
                "<td><a href=\"/u/" + html_escape(r.author) + "\">" + html_escape(r.author) + "</a></td>"
                "<td>" + html_escape(fmt_time(r.ts)) + "</td><td>" + html_escape(excerpt) + "</td>"
                "<td><form method=\"POST\" action=\"/admin/comments/delete\" onsubmit=\"return confirm('Delete this comment?');\">"
                "<input type=\"hidden\" name=\"on\" value=\"" + r.on + "\"><input type=\"hidden\" name=\"id\" value=\"" + std::to_string(r.id) + "\">"
                "<button class=\"danger\" type=\"submit\">Delete</button></form></td></tr>";
    }
    html += "</table></div>";
    return html;
}

// One deleted city department/council, archived like a deleted comment (same <store>.moderation.jsonl; refused if
// unwritable), with the proposals that named it so a hand restore can re-tag them.
bool archive_deleted_city_tag(const std::string& key, const std::string& tag, const std::vector<std::uint64_t>& props,
                              const std::string& by) {
    std::string ids;
    for (auto id : props) ids += (ids.empty() ? "" : ",") + std::to_string(id);
    const std::string line = "{\"deleted_by\":\"" + json_escape(by) + "\",\"deleted_at\":" + std::to_string(std::time(nullptr)) +
        ",\"city_tag_city\":\"" + json_escape(key) + "\",\"city_tag\":\"" + json_escape(tag) + "\",\"proposals\":[" + ids + "]}\n";
    const std::string path = std::string(store_path_from_env()) + ".moderation.jsonl";
    std::ofstream out(path, std::ios::app | std::ios::binary);
    if (!out) {
        std::cerr << "moderation archive not writable: " << path << "\n";
        return false;
    }
    out << line;
    out.flush();
    return static_cast<bool>(out);
}

// Departments and councils named on city pages, each deletable (open item "moderation gap left: delete city
// departments/councils", Brandon OK'd 2026-11-08). q searches the city and the name.
std::string admin_departments_page(const std::string& q, const std::string& msg) {
    std::string html = admin_header() + admin_nav("departments") + admin_search_form("/admin/departments", q, "City or name");
    if (!msg.empty()) html += "<p class=\"muted\" role=\"status\">" + html_escape(msg) + "</p>";
    // the city's name ("Fresno, CA"), not its key ("fresno-ca"), as the city page shows it (sweep 2026-11-09)
    std::map<std::string, std::string> city_names;
    for (const auto& cu : g_store.cities_in_use()) city_names[cu.key] = cu.name;
    auto city_name = [&](const std::string& key) { auto it = city_names.find(key); return it == city_names.end() ? key : it->second; };
    auto rows = g_store.all_city_tags();
    if (!q.empty())
        rows.erase(std::remove_if(rows.begin(), rows.end(), [&](const std::pair<std::string, std::string>& r) {
            return !admin_matches(r.first, q) && !admin_matches(city_name(r.first), q) && !admin_matches(r.second, q); }),
            rows.end());
    if (rows.empty()) {
        html += "<p class=\"empty\">" + std::string(q.empty() ? "No departments or councils yet." : "Nothing matches.") + "</p>";
        return html;
    }
    html += "<p class=\"muted\">Delete removes it from the city page and from every proposal that names it; a copy goes "
            "to the moderation archive first.</p>"
            "<div style=\"overflow-x:auto\"><table class=\"register\"><tr><th>City</th><th>Department or council</th>"
            "<th>Proposals</th><th></th></tr>";
    for (const auto& [key, tag] : rows) {
        const auto n = g_store.proposals_with_city_tag(key, tag).size();
        html += "<tr><td><a href=\"/city/" + url_encode(key) + "\">" + html_escape(city_name(key)) + "</a></td>"
                "<td>" + html_escape(tag) + "</td><td>" + std::to_string(n) + "</td>"
                "<td><form method=\"POST\" action=\"/admin/departments/delete\" "
                "onsubmit=\"return confirm('Delete this department or council?');\">"
                "<input type=\"hidden\" name=\"city\" value=\"" + html_escape(key) + "\">"
                "<input type=\"hidden\" name=\"tag\" value=\"" + html_escape(tag) + "\">"
                "<button class=\"danger\" type=\"submit\">Delete</button></form></td></tr>";
    }
    html += "</table></div>";
    return html;
}

std::string admin_communities_page() {
    std::string html = admin_header() + admin_nav("communities");
    auto communities = g_store.all_communities();
    if (communities.empty()) {
        html += "<p class=\"empty\">No groups yet.</p>";
        return html;
    }
    html += "<div style=\"overflow-x:auto\"><table class=\"register\"><tr>"
            "<th>Name</th><th>Owner</th><th>Members</th><th>Actions</th></tr>";
    for (const auto& c : communities) {
        html += "<tr><td><a href=\"/c/"+html_escape(c.slug)+"\">"+html_escape(c.name)+"</a> "
                "<span class=\"muted\">/"+html_escape(c.slug)+"</span></td>"
                "<td><a href=\"/u/"+html_escape(c.owner)+"\">"+html_escape(c.owner)+"</a></td>"
                "<td>"+std::to_string(g_store.members_of(c.slug).size())+"</td>"
                "<td><details style=\"display:inline-block\"><summary class=\"muted\" style=\"cursor:pointer;display:inline\">Delete&hellip;</summary>"
                "<form method=\"POST\" action=\"/admin/communities/delete\" style=\"margin-top:.3rem\">"
                "<input type=\"hidden\" name=\"slug\" value=\""+html_escape(c.slug)+"\">"
                "<input name=\"confirm\" placeholder=\"type &ldquo;delete&rdquo;\" autocomplete=\"off\">"
                "<button class=\"danger\" type=\"submit\">Delete group</button></form></details>"
                "<p class=\"muted\" style=\"margin:.3rem 0 0\">Posts already made here keep their room "
                "name but stop being reachable -- they are not deleted.</p></td></tr>";
    }
    html += "</table></div>";
    return html;
}

std::string admin_log_page() {
    std::string html = admin_header() + admin_nav("log");
    auto entries = g_store.recent_audit_log(50);
    if (entries.empty()) {
        html += "<p class=\"empty\">No operator actions yet.</p>";
        return html;
    }
    html += "<div style=\"overflow-x:auto\"><table class=\"register\"><tr>"
            "<th>When</th><th>Operator</th><th>Action</th><th>Target</th></tr>";
    for (const auto& e : entries) {
        html += "<tr><td>"+html_escape(fmt_time(e.ts))+"</td>"
                "<td>"+html_escape(e.operator_name)+"</td>"
                "<td>"+html_escape(e.action)+"</td>"
                "<td>"+html_escape(e.target)+"</td></tr>";
    }
    html += "</table></div>";
    return html;
}

}  // namespace liquidgov::site
