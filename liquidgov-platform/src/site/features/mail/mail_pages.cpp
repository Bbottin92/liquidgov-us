#include "site/features/mail/mail_pages.hpp"
#include "site/ui/assets.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/names_categories.hpp"
#include "site/features/mail/mail_attachments.hpp"
#include "site/features/profile/portrait.hpp"

namespace liquidgov::site {
// The friends section of the mail widget/page: real hrefs (/mail?with=X)
// so it still works with JS off or as a direct link, plus a data-with the
// widget's own delegated click handler uses to fetch the thread in place
// instead of navigating -- same progressive-enhancement shape as the
// site's own soft-navigation (see jukebox_script's click interceptor).
// One chronological inbox: 1:1 threads that actually have mail, plus groups.
// Friends with no thread live under "Start a conversation", not the list.
std::string mail_inbox_html(const std::string& user, AccountId acc_id) {
    auto pals = g_store.friends_of(user);
    auto inner = g_store.inner_of(user);
    std::set<std::string> inset(inner.begin(), inner.end());
    std::vector<std::string> top, rest;
    for (auto& p : inner) {
        if (g_store.are_friends(user, p) && !g_store.is_blocked(user, p) && !g_store.is_blocked(p, user))
            top.push_back(p);
    }
    for (auto& p : pals) {
        if (inset.count(p)) continue;
        if (g_store.is_blocked(user, p) || g_store.is_blocked(p, user)) continue;
        rest.push_back(p);
    }
    std::sort(rest.begin(), rest.end());
    auto rows_for = [&](const std::vector<std::string>& people) {
        std::string out;
        for (auto& other : people) {
            auto ts = g_store.last_mail_ts(user, other);
            auto prev = g_store.last_mail_preview(user, other);
            if (prev.size() > 80) prev = prev.substr(0, 77) + "...";
            bool unread = ts > g_store.thread_last_read(user, "d:" + other);
            out += "<a class=\"inboxrow mailwith";
            if (unread) out += " unread";
            out += "\" href=\"/mail?with=" + html_escape(other) + "\" data-with=\"" + html_escape(other) + "\">";
            out += portrait_tag(other, "sm");
            out += "<span style=\"min-width:0;flex:1\"><span class=\"inboxtitle\">" + html_escape(other) + "</span>";
            out += "<span class=\"inboxpreview\">" + html_escape(prev) + "</span></span>";
            if (ts) out += "<span class=\"inboxmeta\">" + html_escape(fmt_time(ts)) + "</span>";
            out += "</a>";
        }
        return out;
    };
    std::string html = "<div class=\"mailsection\">";
    // The Group button and its pick-friends form are gone (Brandon 2026-11-15: "the group button at the top of messenger
    // is annoying... that needs to go away"). Group chats you are already in stay listed, and open as before.
    std::string groups;
    for (auto& conv : g_store.conversations_for(acc_id)) {
        if (!conv.is_group) continue;
        bool pending = false;
        for (auto& m : conv.members) if (m.account == acc_id) pending = m.state == MemberState::Pending;
        auto ts = g_store.last_group_ts(conv.id);
        auto prev = g_store.last_group_preview(conv.id);
        if (prev.size() > 80) prev = prev.substr(0, 77) + "...";
        bool unread = pending || ts > g_store.thread_last_read(user, "g:" + std::to_string(conv.id));
        groups += "<a class=\"inboxrow mailgroup";
        if (unread) groups += " unread";
        groups += "\" href=\"/mail/g/" + std::to_string(conv.id) + "\">";
        groups += "<span class=\"portrait sm ph\" aria-hidden=\"true\"></span>";
        groups += "<span style=\"min-width:0;flex:1\"><span class=\"inboxtitle\">" + html_escape(group_label(conv, acc_id)) + "</span>";
        groups += "<span class=\"inboxpreview\">" + html_escape(prev);
        if (pending) groups += " · invite";
        groups += "</span></span></a>";
    }
    if (!groups.empty()) html += "<p class=\"eyebrow\">Your groups</p><div class=\"inboxlist\">" + groups + "</div>";
    if (top.empty() && rest.empty()) {
        html += "<p class=\"muted\">No friends yet. <a href=\"/friends\">Find people</a></p>";
    } else {
        // one list (Brandon 2026-11-15: remove "Inner circle"); your inner circle still comes first
        html += "<p class=\"eyebrow\">Friends</p><div class=\"inboxlist\">" + rows_for(top) + rows_for(rest) + "</div>";
    }
    // The storage note under the list is gone (Brandon 2026-11-15); /privacy says how messages are kept.
    html += "</div>";
    return html;
}
// Kept for the widget's "start a group" only — inbox is the unified list.
std::string mail_groups_list_html(const std::string& user, AccountId acc_id) {
    return mail_inbox_html(user, acc_id);
}

std::string mail_friends_list_html(const std::string& user) {
    auto acc = g_store.find_by_username(user);
    if (!acc) return "";
    return mail_inbox_html(user, acc->id);
}

// The open thread: message bubbles (mine/theirs) plus the composer. Shared
// by the full /mail page and GET /mail/panel?with=<friend> -- same
// friends-only/not-blocked gate either way, empty string means "not
// viewable" (unknown other, not actually friends, or blocked either
// direction), which the caller renders as simply no thread shown.
std::string mail_thread_html(const std::string& user, const std::string& other,
                              const std::map<std::string,std::string>& qs) {
    if (other.empty() || !g_store.are_friends(user, other) || g_store.is_blocked(user, other)) return "";
    g_store.mark_thread_read(user, "d:" + other);
    std::string html = "<div class=\"mailthread\">"
        "<div class=\"mailthreadhead\"><a class=\"mailback\" data-with=\"\" href=\"/mail\" aria-label=\"Back to messages\">\xE2\x86\x90</a>"
        + portrait_tag(other, "sm") +
        "<strong><a href=\"/u/"+html_escape(other)+"\">"+html_escape(other)+"</a></strong></div>";
    if (qs.count("err")) {
        const std::string& e = qs.at("err");
        if (e=="notfriends") html += "<p class=\"warn\">Mail is friends-only until they accept.</p>";
        else html += mail_attachment_error_html(e, user);
    }
    auto thread = g_store.mail_with(user, other);
    html += "<div class=\"msglist\" id=\"msglist\">";
    if (thread.empty()) html += "<p class=\"empty\">No messages yet.</p>";
    for (auto& m : thread) {
        const bool mine = m.from == user;
        html += "<div class=\"msg "+std::string(mine ? "mine" : "theirs")+"\"><div class=\"bubble\">";
        if (!m.text.empty()) html += html_escape(m.text);
        if (m.attachment_blob_id) {
            html += mail_attachment_html(*m.attachment_blob_id, m.attachment_mime.value_or(""), attachment_name_key(m), m.from, mine);
        }
        html += "<span class=\"bubtime\">"+html_escape(fmt_time(m.ts))+"</span></div></div>";
    }
    html += "</div>"
        "<form method=\"POST\" action=\"/mail\" enctype=\"multipart/form-data\" class=\"mailcomposer\" id=\"mailcomposer\""
        + std::string(kComposerOnSubmit) + ">"
        "<input type=\"hidden\" name=\"to\" value=\""+html_escape(other)+"\">"
        "<div class=\"row\" style=\"align-items:flex-end;gap:.4rem;flex-wrap:nowrap\">"
        "<textarea name=\"text\" id=\"mailtext\" maxlength=\"2000\" rows=\"2\" placeholder=\"Message\" "
        "autocomplete=\"off\" style=\"flex:1;min-width:0;min-height:44px;resize:vertical;background:#fff;color:#14110c;caret-color:#14110c\"></textarea>"
        "<label class=\"mailattbtn\" title=\"Attach a file\">"
        + mail_file_input_html(user, "/mail/big") +
        "<span aria-hidden=\"true\">\xF0\x9F\x93\x8E</span></label>"
        "<button type=\"submit\">Send</button></div>"
        + std::string(kComposerFileLine) +
        // no instructions under the box (Brandon 2026-11-15: "it shouldnt say that you can send programs or have
        // instructions. everyone gets how it works already ... plus its half untrue")
        "</form></div>"
        "<script>(function(){"
        "var list=document.getElementById('msglist');if(list)list.scrollTop=list.scrollHeight;"
        // the full Messages page scrolls as a whole (Brief 4 item 8): open at the newest message, composer in view.
        // Not composer.scrollIntoView: the composer is sticky at the bottom, so the browser counted it
        // as in view and did not scroll, and every send reopened the page at its top (Brandon 2026-11-15). Again once
        // pictures have loaded, since they push the newest message down.
        // Scrolled so the newest message sits just above the composer and the pad (--lg-pad-room, walker.js), not to the
        // page's very end, which is the footer.
        "if(list&&list.closest('.mailpage')){var end=function(){var mc=document.getElementById('mailcomposer'),last=list.lastElementChild;"
        "if(!mc||!last)return;var room=parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--lg-pad-room'))||0;"
        "window.scrollTo(0,Math.max(0,last.getBoundingClientRect().bottom+window.scrollY+mc.offsetHeight+room+12-window.innerHeight));};"
        "end();window.addEventListener('load',end);}"
        "var form=document.getElementById('mailcomposer'),ta=document.getElementById('mailtext');"
        "if(!form||!ta)return;"
        "ta.addEventListener('keydown',function(e){"
        "if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();form.requestSubmit();}"
        "});"
        "})();</script>";
    return html;
}

// Drives the Notifications/Messages header widgets: open/close, fetching
// each panel's fragment (/notices/panel, /mail/panel), switching between
// the friends list and an open thread inside the mail panel without a
// real navigation, sending a message via fetch, and polling the open
// thread every few seconds so it behaves like an actual instant messenger
// instead of a static page you have to manually refresh.
//
// Lives in .topinner as a sibling of <nav class="accountmenu">, same as
// the buttons and panels it drives -- the site's own soft-navigation (see
// jukebox_script) only ever replaces the *inside* of that <nav> element,
// never this script tag or the panels themselves, so this widget's state
// (an open thread, the poll timer) survives browsing to another page the
// same way the floating jukebox widget already does.
std::string mail_widget_script() {
    // The script is assets/mail_widget.js (was inlined here; moved 2026-11-12, same code). No defer: it runs at this
    // point of the page, as the inline one did.
    return "<script src=\"" + asset_url("mail_widget.js") + "\"></script>";
}

}  // namespace liquidgov::site
