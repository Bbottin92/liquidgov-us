#include "site/features/mail/mail_pages.hpp"
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
    auto checks = [&](const std::vector<std::string>& people) {
        std::string out;
        for (auto& other : people) {
            out += "<label class=\"grouppickrow\" style=\"display:flex;align-items:center;gap:.5rem;min-height:44px;padding:.3rem .4rem;cursor:pointer\">";
            out += portrait_tag(other, "sm") + "<span style=\"flex:1\">" + html_escape(other) + "</span>";
            out += "<input type=\"checkbox\" data-who=\"" + html_escape(other) + "\"></label>";
        }
        return out;
    };
    std::string html = "<div class=\"mailsection\">";
    html += "<button type=\"button\" class=\"ghost mailgroupbtn\">Group</button>";
    html += "<div class=\"mailgroupbox\" hidden>";
    html += "<form method=\"POST\" action=\"/mail/group/new\" class=\"mailnewgroup mailgroupform\">";
    html += "<input name=\"title\" maxlength=\"80\" placeholder=\"Group name (optional)\">";
    html += "<input type=\"hidden\" name=\"who\" class=\"mailgroupwho\" value=\"\">";
    html += "<p class=\"muted\">Pick who to invite</p><div class=\"grouppick\" style=\"max-height:min(40vh,320px);overflow-y:auto\">";
    html += checks(top) + checks(rest);
    if (top.empty() && rest.empty()) html += "<p class=\"muted\">Add friends first.</p>";
    html += "</div><button type=\"submit\">Start group</button></form>";
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
    html += "</div>";
    if (top.empty() && rest.empty()) {
        html += "<p class=\"muted\">No friends yet. <a href=\"/friends\">Find people</a></p>";
    } else {
        if (!top.empty()) html += "<p class=\"eyebrow\">Inner circle</p><div class=\"inboxlist\">" + rows_for(top) + "</div>";
        if (!rest.empty()) html += "<p class=\"eyebrow\">Friends</p><div class=\"inboxlist\">" + rows_for(rest) + "</div>";
    }
    html += "<p class=\"maildisclose\">Messages on this copy are stored so the operator can read them. "
            "Direct mail is friends-only until age assurance exists. Unfriending leaves shared groups.</p></div>";
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
        "<p class=\"muted\" style=\"margin:.3rem 0 0\">Enter sends. Shift+Enter is a new line.</p>"
        + mail_program_note_html(user) + "</form></div>"
        "<script>(function(){"
        "var list=document.getElementById('msglist');if(list)list.scrollTop=list.scrollHeight;"
        // the full Messages page scrolls as a whole (Brief 4 item 8): open at the newest message, composer in view
        "if(list&&list.closest('.mailpage')){var mc=document.getElementById('mailcomposer');if(mc)mc.scrollIntoView({block:'end'});}"
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
    return
        "<script>(function(){"
        "var notifToggle=document.getElementById('notiftoggle'),notifPanel=document.getElementById('notifpanel'),"
        "notifRoot=document.getElementById('notifroot');"
        "var mailToggle=document.getElementById('mailtoggle'),mailPanel=document.getElementById('mailpanel'),"
        "mailRoot=document.getElementById('mailroot');"
        "var navc=document.getElementById('navc');"
        "if(!notifToggle||!mailToggle)return;"
        "var lastN=-1,lastM=-1;"
        "document.addEventListener('pointerdown',function(){try{if(!window.__lgAC)window.__lgAC=new(window.AudioContext||window.webkitAudioContext)();if(window.__lgAC.resume)window.__lgAC.resume();}catch(err){}},{once:true});"
        "function lgDing(){try{var ac=window.__lgAC;if(!ac)return;if(ac.resume)ac.resume();var o=ac.createOscillator(),g=ac.createGain();o.frequency.value=880;g.gain.value=0.07;o.connect(g);g.connect(ac.destination);o.start();g.gain.exponentialRampToValueAtTime(0.001,ac.currentTime+0.2);o.stop(ac.currentTime+0.22);}catch(err){}}"
        "function lgSetBadge(btn,v){if(!btn)return;var b=btn.querySelector('.badge');if(v<=0){if(b)b.remove();return;}if(!b){b=document.createElement('span');b.className='badge';btn.appendChild(b);}b.textContent=String(v);}"
        "function lgApplyUnread(n,m){n=n|0;m=m|0;var t=n+m;try{if(navigator.setAppBadge){if(t)navigator.setAppBadge(t);else if(navigator.clearAppBadge)navigator.clearAppBadge();}}catch(err){}lgSetBadge(notifToggle,n);lgSetBadge(mailToggle,m);if(lastN>=0&&(n>lastN||m>lastM)){lgDing();if('Notification' in window&&Notification.permission==='granted'){try{new Notification('LiquidGov',{body:m>lastM?'New message':'New notice',tag:'lg-unread',icon:'/favicon.svg'});}catch(err){}}}lastN=n;lastM=m;}"
        "function lgPollUnread(){fetch('/me/unread',{credentials:'same-origin'}).then(function(r){return r.ok?r.json():null;}).then(function(d){if(d)lgApplyUnread(d.n,d.m);}).catch(function(){});}"
        "lgPollUnread();setInterval(lgPollUnread,20000);"
        "function lgAskNotify(){if(!('Notification' in window))return;if(Notification.permission==='default'){try{Notification.requestPermission();}catch(err){}}}"
        "var activeWith=null,mailFetching=false,pollTimer=null;"
        "function nearBottom(el){return el.scrollHeight-el.scrollTop-el.clientHeight<60;}"
        "function stopPoll(){if(pollTimer){clearInterval(pollTimer);pollTimer=null;}}"
        "function hideNotif(){notifPanel.hidden=true;}"
        "function hideMail(){mailPanel.hidden=true;stopPoll();activeWith=null;}"
        "function closeMenu(){if(navc)navc.checked=false;}"
        "function loadMail(withUser,forceScroll){"
        "var ae=document.activeElement;"
        "if(!forceScroll&&ae&&mailRoot.contains(ae)&&(ae.tagName==='TEXTAREA'||ae.tagName==='INPUT'||(ae.closest&&ae.closest('.mailcomposer'))))return;"
        "var t0=mailRoot.querySelector('#mailtext');"
        "var f0=mailRoot.querySelector('.mailcomposer input[type=file],#mailcomposer input[type=file]');"
        "if(!forceScroll&&f0&&f0.files&&f0.files.length)return;"
        "var gbox=mailRoot.querySelector('.mailgroupbox');"
        "if(!forceScroll&&gbox&&!gbox.hidden)return;"
        "var draft=t0?t0.value:'',ss=t0?t0.selectionStart:0,se=t0?t0.selectionEnd:0;"
        "if(mailFetching)return;"
        "mailFetching=true;"
        "var url='/mail/panel'+(withUser?('?with='+encodeURIComponent(withUser)):'');"
        "fetch(url).then(function(r){return r.text();}).then(function(html){"
        "mailFetching=false;"
        "var list=document.getElementById('msglist');"
        "var wasNear=list?nearBottom(list):true;"
        "mailRoot.innerHTML=html;"
        "var t1=mailRoot.querySelector('#mailtext');"
        "if(t1&&draft){t1.value=draft;try{t1.setSelectionRange(ss,se);}catch(err){}}"
        "activeWith=withUser||null;"
        "var newList=document.getElementById('msglist');"
        "if(newList&&(forceScroll||wasNear))newList.scrollTop=newList.scrollHeight;"
        "if(activeWith&&!pollTimer)pollTimer=setInterval(function(){loadMail(activeWith,false);},3000);"
        "if(!activeWith)stopPoll();"
        "}).catch(function(){mailFetching=false;});"
        "}"
        "notifToggle.addEventListener('click',function(){"
        "lgAskNotify();"
        "if(!notifPanel.hidden){hideNotif();return;}"
        "hideMail();closeMenu();"
        "notifPanel.hidden=false;"
        "fetch('/notices/panel').then(function(r){return r.text();}).then(function(html){"
        "notifRoot.innerHTML=html;"
        "var b=notifToggle.querySelector('.badge');if(b)b.remove();"
        "});"
        "});"
        "document.addEventListener('click',function(e){"
        "var btn=e.target.closest&&e.target.closest('.mailgroupbtn');"
        "if(!btn)return;"
        "e.preventDefault();e.stopPropagation();"
        "var box=btn.parentNode.querySelector('.mailgroupbox');"
        "if(box)box.hidden=!box.hidden;"
        "});"
        "document.addEventListener('submit',function(e){"
        "var form=e.target&&e.target.closest&&e.target.closest('.mailgroupform');"
        "if(!form)return;"
        "var names=[];"
        "form.querySelectorAll('input[data-who]:checked').forEach(function(c){names.push(c.getAttribute('data-who'));});"
        "var h=form.querySelector('.mailgroupwho');"
        "if(h)h.value=names.join(' ');"
        "if(!names.length)e.preventDefault();"
        "});"
        "mailToggle.addEventListener('click',function(){"
        "lgAskNotify();"
        "if(!mailPanel.hidden){hideMail();return;}"
        "hideNotif();closeMenu();"
        "mailPanel.hidden=false;"
        "var b=mailToggle.querySelector('.badge');if(b)b.remove();"
        "loadMail(null,false);"
        "});"
        "if(navc)navc.addEventListener('change',function(){if(navc.checked){hideNotif();hideMail();}});"
        // The account menu's own links (Feed, Settings, Log out, ...) go
        // through a real or soft navigation either way -- nothing about
        // that unchecks navc on its own, so the dropdown was left looking
        // stuck open on top of whatever page you landed on. Delegated on
        // the <nav> itself, not its links: soft-navigation only ever
        // replaces this nav's *contents* (see jukebox_script's swap),
        // never the <nav> element itself, so one listener here survives
        // every page the account menu's own links take you to.
        "var accountMenu=document.querySelector('nav.accountmenu');"
        "if(accountMenu)accountMenu.addEventListener('click',function(e){"
        "if(e.target.closest('a'))closeMenu();"
        "});"
        "function clickInsideChrome(t){"
        "if(!t||!t.closest)return false;"
        "return !!(t.closest('nav.accountmenu')||t.closest('#notifpanel')||t.closest('#mailpanel')"
        "||t.closest('.navavatar')||t.closest('#notiftoggle')||t.closest('#mailtoggle')"
        "||t.closest('label.navbtn')||t.closest('#navc'));"
        "}"
        "document.addEventListener('click',function(e){"
        "if(clickInsideChrome(e.target))return;"
        "closeMenu();hideNotif();hideMail();"
        "});"
        "document.addEventListener('keydown',function(e){"
        "if(e.key==='Escape'){closeMenu();hideNotif();hideMail();}"
        "});"
        // Delegated, not per-link: mailRoot's own contents are replaced
        // wholesale on every load, so listeners bound to specific elements
        // would be gone after the very first fetch.
        "mailRoot.addEventListener('click',function(e){"
        "var a=e.target.closest('a.mailwith,a.mailback,a.inboxrow.mailwith');"
        "if(a){e.preventDefault();loadMail(a.classList.contains('mailback')?null:a.dataset.with,true);return;}"
        // A group link (unlike mailwith/mailback) is a real navigation,
        // not fetched into the panel -- same "left looking stuck open"
        // problem as the account menu, fixed the same way. Attachment
        // downloads are deliberately not matched here: those carry
        // Content-Disposition:attachment, so the browser never actually
        // navigates away and the panel closing on one would just be wrong.
        "if(e.target.closest('a.mailgroup'))hideMail();"
        "});"
        "mailRoot.addEventListener('submit',function(e){"
        "var form=e.target.closest('form.mailcomposer');"
        "if(!form)return;"
        "e.preventDefault();"
        "var fd=new FormData(form);"
        // XHR, not fetch, for the upload's progress: the Send button reads "Sending 37%" while a file goes up
        // (2026-10-27: a 17 MB program looked like nothing was happening, and leaving the panel cancelled it)
        "var btn=form.querySelector('button[type=submit]');"
        "if(form.dataset.sending)return;form.dataset.sending='1';if(btn)btn.textContent='Sending...';"
        "var x=new XMLHttpRequest();"
        // the form's own action (/mail/big for a program sender); a refusal (?err=, /mail/too-large) is shown as a
        // page, since the panel reload would drop its message
        "x.open('POST',form.getAttribute('action')||'/mail');"
        "x.upload.onprogress=function(p){if(btn&&p.lengthComputable)btn.textContent='Sending '+Math.floor(p.loaded*100/p.total)+'%';};"
        "x.onload=function(){var u=x.responseURL||'';"
        "if(/[?&]err=|\\/mail\\/too-large/.test(u)){location.href=u;return;}"
        // nginx's own refusals of an upload (too large 413, too many at once 429) never reach the app
        "if(x.status<200||x.status>=300){location.href='/mail/too-large?status='+x.status;return;}loadMail(activeWith,true);};"
        "x.onerror=function(){location.href='/mail/too-large?status=0';};"
        "x.send(fd);"
        "});"
        "mailRoot.addEventListener('keydown',function(e){"
        "if(e.key!=='Enter'||e.shiftKey)return;"
        "var ta=e.target.closest('textarea[name=text]');"
        "if(!ta)return;"
        "var form=ta.closest('form.mailcomposer');"
        "if(!form)return;"
        "e.preventDefault();"
        "form.requestSubmit();"
        "});"
        "})();</script>";
}

}  // namespace liquidgov::site
