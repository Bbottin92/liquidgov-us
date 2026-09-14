#!/usr/bin/env python3
"""Accept friend requests in the notices panel; keep the bell badged until handled."""
from pathlib import Path
import sys

p = Path(sys.argv[1] if len(sys.argv) > 1 else "/opt/liquidgov/src/http_server.cpp")
s = p.read_text()
orig = s

# 1. Friend-request rows: Accept button + unread styling.
old_list = '''        if (n.type=="friend_request") { line = html_escape(n.from)+" sent a friend request."; href="/friends"; }
        else if (n.type=="friend_accept") { line = html_escape(n.from)+" accepted your request."; href="/u/"+html_escape(n.from); }
        else if (n.type=="message") { line = html_escape(n.from)+" sent mail."; href="/mail?with="+html_escape(n.from); }
        else if (n.type=="group_invite") { line = html_escape(n.from)+" added you to /"+html_escape(n.extra)+"."; href="/c/"+html_escape(n.extra); }
        else line = html_escape(n.type);
        html += "<div class=\\"person\\">";
        if (!href.empty()) html += "<a href=\\""+href+"\\" style=\\"flex:1;min-width:0;color:inherit;text-decoration:none\\">";
        html += "<div>"+line+"<div class=\\"muted\\">"+html_escape(fmt_time(n.ts))+"</div></div>";
        if (!href.empty()) html += "</a>";
        html += "</div>";'''

new_list = r'''        const bool pending_fr = n.type=="friend_request" &&
            ([&]{ auto inc=g_store.incoming_requests(user); return std::find(inc.begin(),inc.end(),n.from)!=inc.end(); })();
        if (n.type=="friend_request") { line = html_escape(n.from)+" sent a friend request."; href="/u/"+html_escape(n.from); }
        else if (n.type=="friend_accept") { line = html_escape(n.from)+" accepted your request."; href="/u/"+html_escape(n.from); }
        else if (n.type=="message") { line = html_escape(n.from)+" sent mail."; href="/mail?with="+html_escape(n.from); }
        else if (n.type=="group_invite") { line = html_escape(n.from)+" added you to /"+html_escape(n.extra)+"."; href="/c/"+html_escape(n.extra); }
        else line = html_escape(n.type);
        html += "<div class=\"person" + std::string(n.read?"":" notice-unread") + "\">";
        if (!n.from.empty()) html += portrait_tag(n.from, "sm");
        if (!href.empty()) html += "<a href=\""+href+"\" style=\"flex:1;min-width:0;color:inherit;text-decoration:none\">";
        else html += "<div style=\"flex:1;min-width:0\">";
        html += "<div>"+line+"<div class=\"muted\">"+html_escape(fmt_time(n.ts))+"</div></div>";
        if (!href.empty()) html += "</a>"; else html += "</div>";
        if (pending_fr) {
            html += "<form class=\"inline notice-accept\" method=\"POST\" action=\"/friends/accept\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(n.from)+"\">"
                    "<button type=\"submit\">Accept</button></form>";
        }
        html += "</div>";'''

if old_list not in s:
    print("STOP list", s.count("sent a friend request."))
    raise SystemExit(1)
s = s.replace(old_list, new_list, 1)
print("ok_list 1")

# Unread count on the fragment so the widget can keep/clear the badge.
old_head = '''    std::string html = "<div class=\\"card\\">";
    auto notes = g_store.notices_for(user, 50);
    if (notes.empty()) html += "<p class=\\"empty\\">Nothing waiting.</p>";'''
new_head = '''    auto notes = g_store.notices_for(user, 50);
    int fresh = 0;
    for (const auto& n : notes) if (!n.read) ++fresh;
    std::string html = "<div class=\\"card\\" data-unread=\\"" + std::to_string(fresh) + "\\">";
    if (fresh > 0) html += "<p class=\\"notif-banner\\">" + std::to_string(fresh) + (fresh==1?" new":" new") + "</p>";
    if (notes.empty()) html += "<p class=\\"empty\\">Nothing waiting.</p>";'''
if old_head not in s:
    print("STOP head")
    raise SystemExit(1)
s = s.replace(old_head, new_head, 1)
print("ok_head 1")

# Opening the panel must not swallow pending friend-request badges.
old_mark = '        g_store.mark_notices_read_except_type(user, "message");'
new_mark = '''        g_store.mark_notices_read_of_type(user, "friend_accept");
        g_store.mark_notices_read_of_type(user, "group_invite");'''
nmark = s.count(old_mark)
print("mark_sites", nmark)
if nmark != 2:
    print("STOP mark")
    raise SystemExit(1)
s = s.replace(old_mark, new_mark)
print("ok_mark", nmark)

# After accepting the last pending request, clear leftover friend_request notices.
old_acc = '''        g_store.accept_friend_request(who, user);
        return http_redir("/friends");'''
new_acc = '''        g_store.accept_friend_request(who, user);
        if (g_store.incoming_requests(user).empty())
            g_store.mark_notices_read_of_type(user, "friend_request");
        return http_redir("/friends");'''
if old_acc not in s:
    print("STOP accept")
    raise SystemExit(1)
s = s.replace(old_acc, new_acc, 1)
print("ok_accept 1")

# Widget: keep badge in sync; Accept stays in the panel (no navigation).
old_js = '''        "fetch('/notices/panel').then(function(r){return r.text();}).then(function(html){"
        "notifRoot.innerHTML=html;"
        "var b=notifToggle.querySelector('.badge');if(b)b.remove();"
        "});"'''
new_js = '''        "fetch('/notices/panel').then(function(r){return r.text();}).then(function(html){"
        "notifRoot.innerHTML=html;syncNotifBadge();"
        "});"'''
if old_js not in s:
    print("STOP js_open")
    raise SystemExit(1)
s = s.replace(old_js, new_js, 1)
print("ok_js_open 1")

old_end = '''        "mailRoot.addEventListener('submit',function(e){"
        "var form=e.target.closest('form.mailcomposer');"
        "if(!form)return;"
        "e.preventDefault();"
        "var fd=new FormData(form);"
        "var errEl=form.querySelector('.mailsenderr');if(errEl)errEl.hidden=true;"
        "fetch('/mail',{method:'POST',body:fd}).then(function(r){"
        "if(!r.ok)throw new Error('send failed');"
        "loadMail(activeWith,true);"
        "}).catch(function(){"
        "if(errEl){errEl.textContent='Could not send. Try again.';errEl.hidden=false;}"
        "});"
        "});"
        "})();</script>";'''
new_end = '''        "function syncNotifBadge(){"
        "var card=notifRoot.querySelector('[data-unread]');"
        "var c=card?Number(card.getAttribute('data-unread')||0):0;"
        "var b=notifToggle.querySelector('.badge');"
        "if(c>0){if(!b){b=document.createElement('span');b.className='badge';notifToggle.appendChild(b);}b.textContent=String(c);}"
        "else if(b)b.remove();"
        "}"
        "notifRoot.addEventListener('submit',function(e){"
        "var form=e.target.closest('form.notice-accept');"
        "if(!form)return;"
        "e.preventDefault();"
        "var who=(form.querySelector('[name=who]')||{}).value||'';"
        "fetch('/friends/accept',{method:'POST',credentials:'same-origin',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'who='+encodeURIComponent(who)}).then(function(){"
        "return fetch('/notices/panel');"
        "}).then(function(r){return r.text();}).then(function(html){"
        "notifRoot.innerHTML=html;syncNotifBadge();"
        "});"
        "});"
        "mailRoot.addEventListener('submit',function(e){"
        "var form=e.target.closest('form.mailcomposer');"
        "if(!form)return;"
        "e.preventDefault();"
        "var fd=new FormData(form);"
        "var errEl=form.querySelector('.mailsenderr');if(errEl)errEl.hidden=true;"
        "fetch('/mail',{method:'POST',body:fd}).then(function(r){"
        "if(!r.ok)throw new Error('send failed');"
        "loadMail(activeWith,true);"
        "}).catch(function(){"
        "if(errEl){errEl.textContent='Could not send. Try again.';errEl.hidden=false;}"
        "});"
        "});"
        "})();</script>";'''
if old_end not in s:
    print("STOP js_end")
    raise SystemExit(1)
s = s.replace(old_end, new_end, 1)
print("ok_js_end 1")

# CSS: unread row + louder badge + panel banner.
old_css = '''        ".iconbtn .badge{position:absolute;top:-4px;right:-4px;margin:0;font-size:.62rem;"
        "min-width:1.1rem;height:1.1rem;padding:0 .25rem}"'''
new_css = '''        ".iconbtn .badge{position:absolute;top:-5px;right:-5px;margin:0;font-size:.7rem;"
        "min-width:1.25rem;height:1.25rem;padding:0 .28rem;box-shadow:0 0 0 2px var(--parchment)}"
        ".notif-banner{margin:0 0 .6rem;padding:.35rem .6rem;border-radius:var(--rsm);"
        "background:var(--accent);color:var(--paper);font-size:.8rem;font-weight:600}"
        ".notice-unread{background:rgba(196,165,116,.14);margin:0 -.4rem;padding:.7rem .4rem;"
        "border-radius:var(--rsm)}"
        ".person form.notice-accept{flex-shrink:0;margin:0 0 0 .4rem}"
        ".person form.notice-accept button{min-height:36px;width:auto;padding:.3rem .75rem}"'''
if old_css not in s:
    print("STOP css")
    raise SystemExit(1)
s = s.replace(old_css, new_css, 1)
print("ok_css 1")

if s == orig:
    print("NO CHANGE")
    raise SystemExit(1)
p.write_text(s)
print("done", p)
