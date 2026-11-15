#include "site/features/mail/mail_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/multipart.hpp"
#include "site/core/names_categories.hpp"
#include "site/features/mail/mail_attachments.hpp"
#include "site/features/mail/mail_pages.hpp"
#include "site/features/pains/pains.hpp"
#include "site/ui/wrap.hpp"

namespace liquidgov::site {

std::optional<std::string> mail_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/mail/group/new") {
        if(!acc) return need_login();
        return http_redir("/mail");
    }
    if(method=="GET"&&path=="/me/unread") {
        if(!acc) return http_raw("401 Unauthorized","application/json","{\"n\":0,\"m\":0}");
        const int mcnt = g_store.unread_message_count(user);
        const int ncnt = unread>mcnt ? unread-mcnt : 0;
        return http_raw("200 OK","application/json",
            "{\"n\":"+std::to_string(ncnt)+",\"m\":"+std::to_string(mcnt)+"}");
    }

    // Where nginx sends a /mail/big upload it refused before reading it.
    if(method=="GET"&&path=="/mail/too-large") {
        std::string why = "That file is bigger than you can send.";
        if (!acc || !program_policy_for(user)) why = "Only an admin, or someone an admin allowed, can send programs and large files.";
        if (qs["status"] == "429") why = "Too many uploads at once. Wait a minute, then send it again.";
        else if (!qs["status"].empty() && qs["status"] != "413") why = "It wasn't sent (the site didn't answer). Try again in a moment.";
        return pg("Not sent", "<div class=\"card\"><h1>Not sent</h1><p>" + why + "</p>"
                  "<p><a href=\"/mail\">Back to mail</a></p></div>");
    }

    if(method=="GET"&&path=="/mail") {
        if(!acc) return need_login();
        g_store.mark_notices_read_of_type(user, "message");
        std::string thread = mail_thread_html(user, qs["with"], qs);
        if (!qs["with"].empty() && thread.empty()) {
            thread = "<div class=\"card\"><p class=\"warn\">Mail is friends-only until they accept. "
                     "<a href=\"/search?q="+html_escape(qs["with"])+"\">Find "+html_escape(qs["with"])+"</a></p></div>";
        }
        // Brief 4 item 8 (Brandon 2026-11-11 4:37 PM): the heading matches the nav ("Messages", not "Mail"), and the
        // age-assurance note is one line plus "more" so the conversation starts higher up. .mailpage scopes the layout
        // (list beside the thread on wide screens, one scroll region, composer pinned; see assets/site.css).
        std::string html = "<div class=\"mailpage\"><h1>Messages</h1>"
            "<div class=\"muted mailage\">Direct messages are friends-only for now. "
            "<details class=\"mailmore\"><summary>more</summary>"
            "Open messaging waits on age assurance so a stranger cannot reach a minor. We will not treat a checkbox as age."
            "</details></div>";
        if (qs["skipped"].size()) html += "<p class=\"warn\">Couldn’t include: "+html_escape(qs["skipped"])+
            ". Unknown, blocked, and contact-settings conflicts are skipped — start a new thread with just them if you still want to talk.</p>";
        html += "<div class=\"card mailwidget"+std::string(thread.empty() ? "" : " hasthread")+"\">"+thread+mail_inbox_html(user, acc->id)+"</div></div>";
        // No separate "Messages" app any more (one app), so no "add Messages
        // to your Home Screen" nudge here; mail_install_hint() is unused.
        return pg("Messages", html);
    }
    if(method=="GET"&&path=="/mail/panel") {
        if(!acc) return http_code("401 Unauthorized", "");
        g_store.mark_notices_read_of_type(user, "message");
        std::string thread = mail_thread_html(user, qs["with"], qs);
        std::string html = thread+mail_inbox_html(user, acc->id);
        return http_ok(html);
    }

    if(method=="GET"&&path.rfind("/mail/attachment/",0)==0) {
        if(!acc) return need_login();
        const std::string blob_id = path.substr(17);
        auto found = g_store.find_mail_attachment(user, blob_id);
        if (!found) {
            auto gfound = g_store.find_group_attachment(acc->id, blob_id);
            if (!gfound) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
            return mail_attachment_response(blob_id,
                gfound->attachment_mime.empty() ? "application/octet-stream" : gfound->attachment_mime,
                attachment_name_key(*gfound));
        }
        const std::string other = (found->from == user) ? found->to : found->from;
        // Same gate as the thread view: attachments do not outlive the
        // friendship they were sent inside of. Do not open stranger DMs.
        if (!g_store.are_friends(user, other) || g_store.is_blocked(user, other) || g_store.is_blocked(other, user))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        // (the same bytes sent twice under two names: the download is named after the message the store finds)
        return mail_attachment_response(blob_id, found->attachment_mime.value_or("application/octet-stream"),
                                        attachment_name_key(*found));
    }

    if(method=="GET"&&path.rfind("/mail/g/",0)==0) {
        if(!acc) return need_login();
        ConversationId conv_id = 0;
        try { conv_id = std::stoull(path.substr(8)); } catch(...) { conv_id = 0; }
        auto conv = conv_id ? g_store.get_conversation(conv_id) : std::nullopt;
        const ConversationMember* self_member = nullptr;
        if (conv) for (auto& m : conv->members) if (m.account == acc->id) self_member = &m;
        if (!conv || !conv->is_group || !self_member)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such group.</p></div>"));
        g_store.mark_thread_read(user, "g:" + std::to_string(conv_id));
        const std::string base = "/mail/g/" + std::to_string(conv_id);
        const bool active = self_member->state == MemberState::Active;

        std::string html = "<p><a href=\"/mail\">\xE2\x86\x90 Mail</a></p>"
            "<h1>"+html_escape(group_label(*conv, acc->id))+"</h1>"
            "<p class=\"muted\">Platform group conversation. Not an AVND locality.</p>";
        if (qs["err"]=="invite") html += "<p class=\"warn\">Couldn’t add them — conflicts with someone’s contact settings. "
            "Start a new thread with just them if you still want to talk.</p>";
        if (!qs["skipped"].empty()) html += "<p class=\"warn\">Couldn’t include: "+html_escape(qs["skipped"])+". "
            "Unknown, blocked, and contact-settings conflicts are skipped.</p>";
        if (qs["err"]!="invite") html += mail_attachment_error_html(qs["err"], user);

        if (!active) {
            html += "<div class=\"mailpending\"><p><strong>Waiting for you to join.</strong> "
                    "You can’t send until you accept. Declining is never announced to the room.</p>"
                    "<form method=\"POST\" action=\""+base+"/accept\"><button>Accept</button></form>"
                    "<form method=\"POST\" action=\""+base+"/leave\"><button class=\"ghost\">Decline</button></form></div>";
        }

        html += "<div class=\"card\">";
        auto msgs = g_store.messages_in(conv_id);
        html += "<div class=\"msglist\" id=\"msglist\" style=\"max-height:none\">";
        bool any = false;
        for (auto& m : msgs) {
            if (m.deletion==DeletionState::DeletedForSender && m.sender==acc->id) continue;
            any = true;
            const bool mine = m.sender==acc->id;
            html += "<div class=\"msg "+std::string(mine?"mine":"theirs")+"\"><div class=\"bubble\">";
            if (!mine) html += "<strong>"+html_escape(display_name_for(m.sender))+"</strong> ";
            if (m.deletion==DeletionState::DeletedForEveryone) {
                html += "<em class=\"muted\">Message deleted.</em>";
            } else {
                if (!m.text.empty()) html += html_escape(m.text);
                if (!m.attachment_blob_id.empty())
                    html += mail_attachment_html(m.attachment_blob_id, m.attachment_mime, attachment_name_key(m),
                                                 display_name_for(m.sender), mine);
                if (m.edited_at>0) html += "<span class=\"muted\"> (edited)</span>";
            }
            html += "<span class=\"bubtime\">"+html_escape(fmt_time(m.sent_at))+"</span></div></div>";
        }
        if (!any) html += "<p class=\"empty\">No messages yet.</p>";
        html += "</div>";
        if (active) {
            html += "<form method=\"POST\" action=\""+base+"/send\" enctype=\"multipart/form-data\" class=\"mailcomposer\" id=\"mailcomposer\""
                    + std::string(kComposerOnSubmit) + ">"
                    "<div class=\"row\" style=\"align-items:flex-end;gap:.4rem;flex-wrap:nowrap\">"
                    "<textarea name=\"text\" id=\"mailtext\" maxlength=\"2000\" rows=\"2\" placeholder=\"Message\" "
                    "style=\"flex:1;min-width:0;min-height:44px;resize:vertical\"></textarea>"
                    "<label class=\"mailattbtn\" title=\"Attach a file\">"
                    + mail_file_input_html(user, "/mail/big/g/" + std::to_string(conv_id) + "/send") +
                    "<span aria-hidden=\"true\">\xF0\x9F\x93\x8E</span></label>"
                    "<button type=\"submit\">Send</button></div>"
                    + std::string(kComposerFileLine) +
                    // no instructions under the box (Brandon 2026-11-15: "it shouldnt say that you can send programs or
                    // have instructions. everyone gets how it works already ... plus its half untrue")
                    "</form>";
        }
        html += "</div>";

        html += "<details class=\"card\"><summary>Members</summary><ul class=\"plain\">";
        for (auto& m : conv->members) {
            html += "<li>"+html_escape(display_name_for(m.account))+
                    (m.state==MemberState::Pending?" <span class=\"muted\">invited \xE2\x80\x94 not in yet</span>":"")+"</li>";
        }
        html += "</ul>";
        if (active) {
            html += "<form method=\"POST\" action=\""+base+"/invite\"><label>Invite</label>"
                    "<div class=\"row\"><input name=\"who\" required autocomplete=\"off\"><button>Invite</button></div></form>"
                    "<form method=\"POST\" action=\""+base+"/title\" class=\"mailnewgroup\">"
                    "<label>Group name</label><input name=\"title\" maxlength=\"80\" value=\""+html_escape(conv->title)+"\">"
                    "<button class=\"ghost\">Save name</button></form>"
                    "<form method=\"POST\" action=\""+base+"/leave\" onsubmit=\"return confirm('Leave this group? You will not be told if others stay.');\">"
                    "<button class=\"ghost\">Leave group</button></form>";
        }
        html += "</details>"
            "<script>(function(){"
            "var list=document.getElementById('msglist');if(list)list.scrollTop=list.scrollHeight;"
            "var form=document.getElementById('mailcomposer'),ta=document.getElementById('mailtext');"
            "if(!form||!ta)return;"
            "ta.addEventListener('keydown',function(e){"
            "if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();form.requestSubmit();}"
            "});"
            "})();</script>";
        return pg("Group", html);
    }
    if(method=="POST"&&path=="/mail") {
        if(!acc) return need_login();
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        std::string ctype = header_value(hdrs, "Content-Type");
        std::string to, text;
        if (ctype.find("multipart/form-data") != std::string::npos) {
            to = multipart_field(ctype, body, "to");
            text = multipart_field(ctype, body, "text");
        } else {
            auto f = parse_form(body);
            to = f["to"]; text = f["text"];
        }
        const MailUpload up = take_mail_upload(ctype, body); // releases the body
        if(text.size()>2000) text.resize(2000);
        // Do not open stranger DMs: friends-only, both directions unblocked,
        // exactly as before attachments existed.
        if(to.empty() || !g_store.are_friends(user,to) || g_store.is_blocked(user,to) || g_store.is_blocked(to,user))
            return http_redir("/mail?with="+to+"&err=notfriends");

        std::optional<std::string> blob_id, mime;
        std::string download_name;
        if (up.file && !up.file->empty()) {
            const std::string code = ingest_mail_attachment(user, up, blob_id, mime, download_name);
            if (!code.empty()) return http_redir("/mail?with="+to+"&err="+code);
        }
        if (!text.empty() || blob_id) {
            g_store.add_mail(user,to,text,blob_id,mime);
            if (!text.empty()) {
                const auto thread = g_store.mail_with(user, to);
                for (auto it = thread.rbegin(); it != thread.rend(); ++it) {
                    if (it->from == user && it->text == text) {
                        notice_pains(user, text, "mail:" + std::to_string(it->ts) + ":" + to);
                        break;
                    }
                }
            }
            if (!download_name.empty()) { // named once the message exists (review 2026-10-27)
                const auto thread = g_store.mail_with(user, to);
                for (auto it = thread.rbegin(); it != thread.rend(); ++it)
                    if (it->from == user && it->attachment_blob_id == blob_id) {
                        g_store.set_attachment_name(attachment_name_key(*it), download_name);
                        break;
                    }
            }
        }
        return http_redir("/mail?with="+to);
    }

    if(method=="POST"&&path=="/mail/group/new") {
        if(!acc) return need_login();
        auto form = parse_form(body);
        const auto who = form["who"];
        auto title = trim_ws(form["title"]);
        if (title.size() > 80) title.resize(80);
        std::vector<AccountId> invitees;
        std::vector<std::string> skipped;
        std::string cur;
        auto flush_tok = [&]() {
            const auto name = trim_ws(cur);
            cur.clear();
            if (name.empty() || name == user) return;
            auto found = g_store.find_by_username(name);
            if (!found || g_store.is_blocked(user, name) || g_store.is_blocked(name, user)) {
                skipped.push_back(name);
                return;
            }
            for (auto id : invitees) if (id == found->id) return;
            invitees.push_back(found->id);
        };
        for (char c : who) {
            if (c==','||c==' '||c=='\t'||c=='\n'||c=='\r') flush_tok();
            else cur += c;
        }
        flush_tok();
        auto result = create_conversation(g_store, g_social_graph, acc->id, invitees, /*is_group=*/true);
        auto conv = g_store.get_conversation(*result.conversation_id);
        if (conv && !title.empty()) {
            conv->title = title;
            g_store.save_conversation(*conv);
        }
        if (conv) {
            std::set<AccountId> kept;
            for (auto& m : conv->members) kept.insert(m.account);
            for (auto id : invitees) {
                if (!kept.count(id)) {
                    if (auto a = g_store.find_by_id(id)) skipped.push_back(a->username);
                } else if (auto a = g_store.find_by_id(id)) {
                    auto pr = g_store.prefs_of(a->username);
                    if (pr.notify_message) g_store.add_notice(a->username, "mail_group", user, std::to_string(conv->id));
                }
            }
        }
        std::string loc = "/mail/g/"+std::to_string(*result.conversation_id);
        if (!skipped.empty()) {
            loc += "?skipped=";
            for (size_t i=0;i<skipped.size();++i) {
                if (i) loc += ",";
                loc += url_encode(skipped[i]);
            }
        }
        return http_redir(loc);
    }

    if(method=="POST"&&path.rfind("/mail/g/",0)==0) {
        if(!acc) return need_login();
        std::string rest = path.substr(8);
        auto slash = rest.find('/');
        const std::string id_str = (slash==std::string::npos) ? rest : rest.substr(0,slash);
        const std::string action = (slash==std::string::npos) ? std::string() : rest.substr(slash+1);
        ConversationId conv_id = 0;
        try { conv_id = std::stoull(id_str); } catch(...) { return http_redir("/mail"); }
        auto conv = g_store.get_conversation(conv_id);
        if (!conv || !conv->is_group) return http_redir("/mail");
        const ConversationMember* self_member = nullptr;
        for (auto& m : conv->members) if (m.account == acc->id) self_member = &m;
        const std::string base = "/mail/g/"+id_str;

        if (action=="accept") {
            if (!self_member) return http_redir("/mail");
            accept_invite(g_store, acc->id, conv_id);
            return http_redir(base);
        }
        if (action=="leave") {
            if (!self_member) return http_redir("/mail");
            leave_conversation(g_store, acc->id, conv_id); // Pending "decline" and Active "leave" alike; never notified
            return http_redir("/mail");
        }
        // Everything below requires being an Active member.
        if (!self_member || self_member->state != MemberState::Active) return http_redir("/mail");

        if (action=="invite") {
            const auto who = trim_ws(parse_form(body)["who"]);
            auto found = g_store.find_by_username(who);
            bool refuse = !found || g_store.is_blocked(user, who) || g_store.is_blocked(who, user);
            if (!refuse) {
                auto r = add_member(g_store, g_social_graph, acc->id, found->id, conv_id);
                refuse = r.error != MessengerError::None;
                if (!refuse) {
                    auto pr = g_store.prefs_of(found->username);
                    if (pr.notify_message) g_store.add_notice(found->username, "mail_group", user, id_str);
                }
            }
            return http_redir(refuse ? (base+"?err=invite") : base);
        }
        if (action=="title") {
            auto title = trim_ws(parse_form(body)["title"]);
            if (title.size() > 80) title.resize(80);
            conv->title = title;
            g_store.save_conversation(*conv);
            return http_redir(base);
        }
        if (action=="send") {
            auto hdr_end = req.find("\r\n\r\n");
            std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
            std::string ctype = header_value(hdrs, "Content-Type");
            std::string text;
            if (ctype.find("multipart/form-data") != std::string::npos) {
                text = multipart_field(ctype, body, "text");
            } else {
                text = parse_form(body)["text"];
            }
            const MailUpload up = take_mail_upload(ctype, body); // releases the body
            text = trim_ws(text);
            if (text.size()>2000) text.resize(2000);
            std::optional<std::string> blob_id, mime;
            std::string download_name;
            if (up.file && !up.file->empty()) {
                const std::string code = ingest_mail_attachment(user, up, blob_id, mime, download_name);
                if (!code.empty()) return http_redir(base+"?err="+code);
            }
            if (!text.empty() || blob_id) {
                auto sent = send_message(g_store, acc->id, conv_id, text);
                if (sent.message_id && !text.empty())
                    notice_pains(user, text, "gmsg:" + std::to_string(*sent.message_id));
                if (sent.message_id && blob_id) {
                    auto msg = g_store.get_message(*sent.message_id);
                    if (msg) {
                        msg->attachment_blob_id = *blob_id;
                        msg->attachment_mime = mime.value_or("");
                        g_store.update_message(*msg);
                        if (!download_name.empty()) // named once the message exists (review 2026-10-27)
                            g_store.set_attachment_name(attachment_name_key(*msg), download_name);
                    }
                } else if (blob_id) {
                    g_blobs.release(*blob_id); // not sent (left the group, blocked...): don't keep up to 100 MB for nothing
                }
            }
            return http_redir(base);
        }
        if (action.rfind("message/",0)==0) {
            const std::string mrest = action.substr(8);
            const auto mslash = mrest.find('/');
            if (mslash==std::string::npos) return http_redir(base);
            MessageId msg_id = 0;
            try { msg_id = std::stoull(mrest.substr(0,mslash)); } catch(...) { return http_redir(base); }
            const std::string mact = mrest.substr(mslash+1);
            if (mact=="edit") {
                auto text = trim_ws(parse_form(body)["text"]);
                if (!text.empty()) {
                    if (text.size()>2000) text.resize(2000);
                    edit_message(g_store, acc->id, msg_id, text);
                }
            } else if (mact=="delete") {
                delete_message(g_store, acc->id, msg_id, /*for_everyone=*/true);
            }
            return http_redir(base);
        }
        return http_redir(base);
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
