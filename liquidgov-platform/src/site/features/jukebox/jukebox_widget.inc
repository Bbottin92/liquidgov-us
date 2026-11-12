// The site-wide station (id "") plus every community this account belongs
// to, as a JS array-literal source -- not strict JSON, embedded directly
// into jukebox_script's <script> via string concatenation, so each field
// goes through js_string_literal the same way every other server string
// gets safely embedded into inline JS on this site.
std::string jukebox_stations_js(const std::string& user) {
    std::ostringstream os;
    os << "[{id:" << js_string_literal("") << ",name:" << js_string_literal("Site-wide") << "}";
    if (!user.empty()) {
        for (const auto& c : g_store.communities_of(user)) {
            os << ",{id:" << js_string_literal(c.slug) << ",name:" << js_string_literal(c.name) << "}";
        }
        // Personal channels this account has already subscribed to (see
        // FileStore::subscribe_jukebox) -- embedded here so a returning
        // visit shows the right name in the station list without the
        // widget needing to ask the server about each id one at a time.
        // Anyone can still *add* a new personal channel from the widget by
        // typing a username, which is what actually calls /subscribe.
        for (const auto& owner : g_store.jukebox_subscriptions_of(user)) {
            os << ",{id:" << js_string_literal("u:" + owner) << ",name:" << js_string_literal(owner + "\xE2\x80\x99s Juketube") << "}";
        }
    }
    os << "]";
    return os.str();
}

// Floating embedded widget: jukebox-icon toggle + collapsible panel,
// injected on every logged-in page via wrap(). "Pop out" opens /listen in a real
// separate window — see jukebox_script's comment for why that, and not
// requestPictureInPicture, is what actually survives navigating this
// (deliberately multi-page) site.
std::string global_jukebox_widget(const std::string& yt_key, const std::string& user) {
    (void)yt_key; (void)user;
    return "<link rel=\"stylesheet\" href=\""+asset_url("juketube.css")+"\">"
           "<div id=\"juketube-root\"></div>"
           "<script src=\""+asset_url("juketube.js")+"\" defer></script><script src=\""+asset_url("push.js")+"\" defer></script>";
}

std::string jukebox_standalone_page(const std::string& yt_key, const std::string& user) {
    return "<div id=\"jbroot\" style=\"max-width:400px;margin:0 auto\">"
           + jukebox_panel_body(true) + "</div>"
           + jukebox_script(yt_key, true, jukebox_stations_js(user), user);
}

// The "Add a track" input + live search box, shared between the owner's own
// queue and a guest's one-shot request -- identical UI, different POST
// target and different note underneath it.
std::string jukebox_add_form(const std::string& post_target, const std::string& note) {
    return "<div><label>Add a track</label>"
            "<div class=\"row\" style=\"align-items:flex-start\">"
            "<input id=\"jinput\" placeholder=\"Paste a link or type to search\" autocomplete=\"off\" style=\"flex:1\">"
            "<button type=\"button\" id=\"jaddbtn\">Add</button>"
            "</div>"
            "<div id=\"jsresults\" style=\"margin-top:.4rem;display:flex;flex-direction:column;gap:.2rem\"></div>"
            "<p class=\"warn\" id=\"jaerr\" style=\"margin:.3rem 0 0\" hidden></p>"
            "</div>"
            "<p class=\"muted\">"+note+"</p>"
            "<script>(function(){"
            // Restores the scroll position saved by queueUrl()'s reload
            // path below, on the very next load of this same page.
            "var savedY=sessionStorage.getItem('lg_scroll');"
            "if(savedY!==null){sessionStorage.removeItem('lg_scroll');window.scrollTo(0,parseInt(savedY,10)||0);}"
            "var target="+js_string_literal(post_target)+";"
            "var input=document.getElementById('jinput'),out=document.getElementById('jsresults');"
            "var errEl=document.getElementById('jaerr');"
            "var lastTopId=null,timer=null,gen=0;"
            "function esc(s){"
            "var ta=document.createElement('textarea');ta.innerHTML=s;"
            "var d=document.createElement('div');d.textContent=ta.value;return d.innerHTML;"
            "}"
            "function looksLikeUrl(s){return /^https?:\\/\\//i.test(s)||/youtu\\.?be/i.test(s);}"
            // A picked/pasted track used to always window.location=... ,
            // a full navigation that scrolls the page back to the top
            // even though nothing about the layout actually moved -- so
            // this used to try updating #jbplayer's src in place instead
            // of reloading. But #jbplayer on THIS page is the floating
            // widget's own player -- always a <div> here, now that the
            // profile's own embedded iframe is gone -- and setting .src
            // on a div does nothing. Only take that shortcut when
            // #jbplayer is genuinely an <iframe> (the /listen standalone
            // page still embeds one); otherwise a plain reload (with
            // scroll restored, not reset to the top) is the only way the
            // new track actually shows up in this list.
            "function queueUrl(url){"
            "errEl.hidden=true;"
            "fetch(target,{method:'POST',credentials:'same-origin',"
            "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
            "body:'url='+encodeURIComponent(url)}).then(function(r){"
            "var dest=r.url||location.href;"
            "var failed=/[?&]err=/.test(dest);"
            "var player=document.getElementById('jbplayer');"
            "var m=url.match(/[?&]v=([A-Za-z0-9_-]{11})/)||url.match(/youtu\\.be\\/([A-Za-z0-9_-]{11})/);"
            "if(!failed&&player&&player.tagName==='IFRAME'&&m){"
            "player.src='https://www.youtube-nocookie.com/embed/'+m[1]+'?rel=0&enablejsapi=1';"
            "return;"
            "}"
            "sessionStorage.setItem('lg_scroll',String(window.scrollY));"
            "location.reload();"
            "}).catch(function(){"
            "errEl.textContent='Could not reach the server. Try again.';errEl.hidden=false;"
            "});"
            "}"
            "function render(items){"
            "out.innerHTML='';lastTopId=null;"
            "if(!items||!items.length)return;"
            "items.forEach(function(it,idx){"
            "var id=it.id.videoId,title=it.snippet.title,thumb=it.snippet.thumbnails.default.url;"
            "var row=document.createElement('div');row.className='row';"
            "row.style.cssText='align-items:center;cursor:pointer;padding:.3rem;border-radius:var(--rsm)';"
            "row.innerHTML='<img src=\"'+thumb+'\" width=\"60\" height=\"45\" alt=\"\" style=\"border-radius:4px\">"
            "<span style=\"flex:1\">'+esc(title)+'</span>';"
            "row.title='Click to queue this track';"
            "row.addEventListener('click',function(){"
            "queueUrl('https://www.youtube.com/watch?v='+id);"
            "});"
            "out.appendChild(row);"
            "if(idx===0){lastTopId=id;row.style.background='var(--parchment)';}"
            "});"
            "}"
            "function search(term){"
            "if(!term){out.textContent='';return;}"
            "var my=++gen;"
            "out.textContent='Searching...';"
            "fetch('/jukebox/search?q='+encodeURIComponent(term),{credentials:'same-origin'})"
            ".then(function(r){if(!r.ok)throw new Error('http');return r.json();})"
            ".then(function(data){if(my!==gen)return;render(data.items||[]);})"
            ".catch(function(){if(my!==gen)return;out.textContent='Search failed.';});"
            "}"
            "input.addEventListener('input',function(){"
            "var val=input.value.trim();"
            "clearTimeout(timer);"
            "if(!val||looksLikeUrl(val)){out.innerHTML='';lastTopId=null;return;}"
            "timer=setTimeout(function(){search(val);},300);"
            "});"
            "function doQueue(){"
            "var val=input.value.trim();if(!val)return;"
            "if(looksLikeUrl(val))queueUrl(val);"
            "else if(lastTopId)queueUrl('https://www.youtube.com/watch?v='+lastTopId);"
            "else search(val);"
            "}"
            "input.addEventListener('keydown',function(e){if(e.key==='Enter'){e.preventDefault();doQueue();}});"
            "var addBtn=document.getElementById('jaddbtn');"
            "if(addBtn)addBtn.addEventListener('click',doQueue);"
            "})();</script>";
}

// Page-local jukebox widget (navigation option A: MPA, own instance per
// page, stops on navigate away — no site-wide SPA, no WebSocket sync).
// The owner always curates their own queue -- persistent, Prev/Next
// browsing, no cap beyond kJukeboxQueueCap. Depending on the owner's
// JukeboxPolicy, a signed-in friend or anyone else may also queue one
// track at a time: a one-shot guest request that's removed and logged to
// history the moment the embedded player reports it ended (see
// complete_jukebox_track). `viewer` is the signed-in username, or empty
// for an anonymous visitor.
// A CoverFlow-style browser for the owner's own queue on /me: the focused
// track's thumbnail faces the viewer, the rest recede at an angle to
// either side like records on a shelf. Arrow clicks and swipes (a harder
// swipe skips further, scaled by release velocity) move the focus;
// title/creator for the focused track comes from GET /jukebox/oembed (no
// API key needed, unlike the Data API v3 search this file also proxies)
// fetched on demand and cached client-side per session -- JukeboxTrack
// itself still only ever stores a video id, exactly as before.
std::string jukebox_coverflow_html(const std::vector<JukeboxTrack>& tracks, const std::string& owner) {
    std::string tracks_json = "[";
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        if (i) tracks_json += ",";
        tracks_json += "{\"id\":\""+json_escape(tracks[i].video_id)+"\",\"by\":\""+
                        json_escape(tracks[i].queued_by)+"\"}";
    }
    tracks_json += "]";
    return
        "<style>"
        "#jbcoverflow{position:relative;display:flex;align-items:center;justify-content:center;"
        "height:160px;margin:.8rem 0;touch-action:pan-y;user-select:none}"
        "#jbcftrack{position:relative;width:100%;height:100%;perspective:900px}"
        ".jbcf-item{position:absolute;top:50%;left:50%;width:120px;height:120px;margin:-60px 0 0 -60px;"
        "border-radius:var(--rsm);overflow:hidden;cursor:pointer;background:#000;"
        "transition:transform .35s ease,opacity .35s ease;box-shadow:0 4px 16px rgba(0,0,0,.3)}"
        ".jbcf-item img{width:100%;height:100%;object-fit:cover;display:block}"
        ".jbcf-arrow{background:var(--panel);border:1px solid var(--rule);border-radius:50%;"
        "width:36px;height:36px;font-size:1.3rem;line-height:1;cursor:pointer;z-index:20;flex-shrink:0;"
        "color:var(--ink)}"
        "#jbcfcaption{text-align:center;margin:.2rem 0 .5rem;min-height:2.6em}"
        "#jbcfcaption strong{display:block}"
        "</style>"
        "<div id=\"jbcoverflow\">"
        "<button type=\"button\" class=\"jbcf-arrow\" id=\"jbcfprev\" aria-label=\"Previous track\">&#8249;</button>"
        "<div id=\"jbcftrack\"></div>"
        "<button type=\"button\" class=\"jbcf-arrow\" id=\"jbcfnext\" aria-label=\"Next track\">&#8250;</button>"
        "</div>"
        "<p id=\"jbcfcaption\"><strong id=\"jbcftitle\"></strong><span class=\"muted\" id=\"jbcfmeta\"></span></p>"
        "<p class=\"row\"><button type=\"button\" class=\"ghost\" id=\"jbcfremove\">Remove this track</button></p>"
        "<script>(function(){"
        "var TRACKS="+tracks_json+";"
        "var OWNER="+js_string_literal(owner)+";"
        "var focus=0;"
        "var track=document.getElementById('jbcftrack');"
        "var titleEl=document.getElementById('jbcftitle'),metaEl=document.getElementById('jbcfmeta');"
        "var wrap=document.getElementById('jbcoverflow'),cap=document.getElementById('jbcfcaption');"
        "var removeBtn=document.getElementById('jbcfremove');"
        "var oembedCache={};"
        "function render(){"
        "track.innerHTML='';"
        "TRACKS.forEach(function(t,i){"
        "var off=i-focus;"
        "if(Math.abs(off)>3)return;"
        "var el=document.createElement('div');el.className='jbcf-item';"
        "var img=document.createElement('img');"
        "img.src='https://img.youtube.com/vi/'+t.id+'/hqdefault.jpg';img.alt='';"
        "el.appendChild(img);"
        "var angle=off*32,tx=off*72,tz=-Math.abs(off)*70,scale=off===0?1:.82;"
        "el.style.transform='translateX('+tx+'px) translateZ('+tz+'px) rotateY('+(-angle)+'deg) scale('+scale+')';"
        "el.style.zIndex=String(100-Math.abs(off));"
        "el.style.opacity=String(Math.max(0,1-Math.abs(off)*.3));"
        "el.addEventListener('click',function(){if(i!==focus){focus=i;update();}});"
        "track.appendChild(el);"
        "});"
        "}"
        "function loadCaption(){"
        "var t=TRACKS[focus];"
        "if(!t){titleEl.textContent='';metaEl.textContent='';return;}"
        // "queued by <you>" on your own curated pick is just noise here --
        // only worth saying when a guest actually requested it.
        "var pos='Track '+(focus+1)+' of '+TRACKS.length;"
        "metaEl.textContent=(t.by&&t.by!==OWNER)?(pos+' \\u00b7 queued by '+t.by):pos;"
        "if(oembedCache[t.id]){titleEl.textContent=oembedCache[t.id];return;}"
        "titleEl.textContent='Loading\\u2026';"
        "fetch('/jukebox/oembed?id='+encodeURIComponent(t.id)).then(function(r){return r.json();})"
        ".then(function(d){"
        "var txt=d.title?(d.title+(d.author_name?' \\u2014 '+d.author_name:'')):t.id;"
        "oembedCache[t.id]=txt;"
        "if(TRACKS[focus]===t)titleEl.textContent=txt;"
        "}).catch(function(){if(TRACKS[focus]===t)titleEl.textContent=t.id;});"
        "}"
        "function update(){"
        "if(!TRACKS.length){wrap.hidden=true;cap.hidden=true;removeBtn.parentNode.hidden=true;return;}"
        "render();loadCaption();"
        "}"
        "function move(delta){"
        "focus=Math.max(0,Math.min(TRACKS.length-1,focus+delta));"
        "update();"
        "}"
        "document.getElementById('jbcfprev').addEventListener('click',function(){move(-1);});"
        "document.getElementById('jbcfnext').addEventListener('click',function(){move(1);});"
        "removeBtn.addEventListener('click',function(){"
        "var t=TRACKS[focus];if(!t)return;"
        "fetch('/me/jukebox/remove',{method:'POST',credentials:'same-origin',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'video_id='+encodeURIComponent(t.id)}).then(function(){"
        "TRACKS.splice(focus,1);"
        "if(focus>=TRACKS.length)focus=TRACKS.length-1;"
        "update();"
        "});"
        "});"
        // Swipe: drag distance decides direction, release velocity decides
        // how many tracks to skip -- a light flick moves one, a hard flick
        // moves several.
        "var startX=null,startT=0;"
        "wrap.addEventListener('pointerdown',function(e){startX=e.clientX;startT=Date.now();});"
        "wrap.addEventListener('pointerup',function(e){"
        "if(startX===null)return;"
        "var dx=e.clientX-startX,dt=Math.max(1,Date.now()-startT);"
        "startX=null;"
        "if(Math.abs(dx)<20)return;"
        "var velocity=Math.abs(dx)/dt;"
        "var skip=Math.max(1,Math.round(velocity*3.5));"
        "move(dx<0?skip:-skip);"
        "});"
        "wrap.addEventListener('pointercancel',function(){startX=null;});"
        "update();"
        "})();</script>";
}

std::string jukebox_widget(const std::string& owner, bool owner_view, const std::string& viewer,
                            const std::map<std::string,std::string>& qs) {
    auto tracks = g_store.jukebox_of(owner);
    auto owner_prefs = g_store.prefs_of(owner);
    const char* policy_badge = owner_prefs.jukebox_policy==JukeboxPolicy::Anyone ? "anyone may queue"
        : owner_prefs.jukebox_policy==JukeboxPolicy::Friends ? "friends may queue" : "owner curated";
    std::string html = "<div class=\"card jukebox\"><h2>Jukebox</h2>"
                        "<p class=\"muted eyebrow\">"+std::string(policy_badge);
    if (owner_view) {
        const auto n = g_store.jukebox_subscriber_count(owner);
        html += " &middot; "+std::to_string(n)+(n==1?" subscriber":" subscribers");
    }
    html += "</p>";
    html += flash_error(qs);

    const bool is_guest = !owner_view && !viewer.empty();
    const bool is_friend = is_guest && g_store.are_friends(viewer, owner);
    const bool blocked = is_guest && g_store.is_blocked(owner, viewer);
    bool has_pending = false;
    if (is_guest) for (const auto& t : tracks) if (t.queued_by == viewer) { has_pending = true; break; }
    bool policy_allows = false;
    if (is_guest && !blocked) {
        if (owner_prefs.jukebox_policy == JukeboxPolicy::Anyone) policy_allows = true;
        else if (owner_prefs.jukebox_policy == JukeboxPolicy::Friends) policy_allows = is_friend;
    }
    const bool guest_can_add = is_guest && policy_allows && !has_pending;

    // What's actually playing here can be a relay (see "Play this Juketube
    // instead" on /me) -- resolved at most one hop, never chased further.
    // Two players on one page was the actual bug this replaces: no more
    // embedded iframe, no Prev/Next-as-page-nav, no second YT.Player. This
    // card is just text + a thumbnail; the real playback -- for owner and
    // visitor alike -- happens once, in the floating widget, via "Play in
    // Juketube" below.
    const std::string playback_owner = g_store.jukebox_effective_owner(owner);
    const bool relayed = playback_owner != owner;
    auto playing = g_store.jukebox_of(playback_owner);
    if (relayed) {
        html += "<p class=\"muted\">Playing <a href=\"/u/"+html_escape(playback_owner)+"\">"+
                html_escape(playback_owner)+"</a>'s Juketube.</p>";
    }
    if (playing.empty()) {
        html += "<p class=\"empty\">Nothing playing.</p>";
    } else {
        const auto& t = playing.front();
        html += "<div class=\"row\" style=\"align-items:center;gap:.6rem\">"
                "<img src=\"https://img.youtube.com/vi/"+html_escape(t.video_id)+"/hqdefault.jpg\" "
                "width=\"96\" height=\"72\" alt=\"\" style=\"border-radius:var(--rsm);flex-shrink:0\">"
                "<div><strong>Now playing</strong><br><span class=\"muted\">queued by "+
                html_escape(t.queued_by)+"</span></div></div>"
                "<p class=\"muted\">"+std::to_string(playing.size()-1)+
                (playing.size()==2?" track":" tracks")+" queued next</p>";
    }
    if (!viewer.empty()) {
        html += "<p class=\"row\"><button type=\"button\" class=\"btn\" "
                "onclick=\"window.__lgTuneStation&amp;&amp;window.__lgTuneStation('u:"+owner+"')\">"
                "Play in Juketube</button></p>";
    }
    if (owner_view) {
        html += "<div style=\"margin-top:.6rem\"><label>Your playlist</label>";
        if (tracks.empty()) {
            html += "<p class=\"empty\">Nothing queued yet.</p>";
        } else {
            html += jukebox_coverflow_html(tracks, owner);
        }
        html += "</div>";
    }

    if (owner_view) {
        // Only true of this in-page embed: a visitor who followed this
        // channel in their own jukebox widget instead (see jukebox_script's
        // Follow button, /jukebox/subscribe) keeps listening as they browse
        // elsewhere -- the widget polls /jukebox/now independently of this
        // page. Who-can-queue is an access-control setting, not profile
        // content -- moved to Settings > Privacy (Brandon: "its not
        // appropriate" on the profile page); the eyebrow badge above still
        // shows the current policy here, read-only.
        html += jukebox_add_form("/me/jukebox/add",
            "Visitors listen here as long as they stay on this page -- following it in the jukebox widget keeps it playing as they browse.");
    } else if (guest_can_add) {
        html += jukebox_add_form("/u/"+owner+"/jukebox/add",
            "One request at a time. It plays once, then it's gone. Follow this channel in the jukebox widget to keep listening as you browse elsewhere.");
    } else if (is_guest && has_pending) {
        html += "<p class=\"muted\">You already have a track queued here.</p>";
    } else if (is_guest && !blocked && owner_prefs.jukebox_policy == JukeboxPolicy::Friends) {
        html += "<p class=\"muted\">Only "+html_escape(owner)+"'s friends can queue a track here.</p>";
    }

    auto hist = g_store.jukebox_history_of(playback_owner);
    if (!hist.empty()) {
        // One row per track, most recently played first, with its play count and last played date; 5 show and the
        // rest sit under "Show more" (Brandon 2026-11-11 4:37 PM, Brief 4 item 6: nine of ten rows were one track,
        // each a bare "watch again"). The title comes from /jukebox/oembed in the page (signed-in viewers; nothing
        // stores titles); until then, and for visitors who are signed out, the link says "YouTube video".
        struct HistRow { std::string id; std::size_t plays = 0; std::int64_t last = 0; };
        std::vector<HistRow> rows; // history is oldest first
        for (auto it = hist.rbegin(); it != hist.rend(); ++it) {
            auto r = std::find_if(rows.begin(), rows.end(), [&](const HistRow& x) { return x.id == it->video_id; });
            if (r == rows.end()) rows.push_back(HistRow{it->video_id, 1, it->played_at});
            else ++r->plays;
        }
        auto row = [](const HistRow& r) {
            const std::string id = html_escape(r.id);
            return "<li class=\"jbhist-row muted\" data-yt=\""+id+"\" style=\"display:flex;align-items:center;gap:.4rem;margin:.3rem 0\">"
                   "<img src=\"https://img.youtube.com/vi/"+id+"/default.jpg\" "
                   "width=\"40\" height=\"30\" alt=\"\" style=\"border-radius:4px;flex:none\">"
                   "<span style=\"min-width:0;overflow-wrap:anywhere\"><a class=\"jbtitle\" data-yt=\""+id+"\" "
                   "href=\"https://www.youtube.com/watch?v="+id+"\" target=\"_blank\" rel=\"noopener\">YouTube video</a>"
                   " &middot; "+std::to_string(r.plays)+(r.plays==1 ? " play" : " plays")+
                   " &middot; last "+html_escape(fmt_time(r.last))+"</span></li>";
        };
        const std::string ul = "<ul style=\"list-style:none;padding:0;margin:0\">";
        html += "<div class=\"jbhist\" style=\"margin-top:1rem\"><label>Recently played</label>"+ul;
        for (std::size_t i = 0; i < rows.size() && i < 5; ++i) html += row(rows[i]);
        html += "</ul>";
        if (rows.size() > 5) {
            html += "<details class=\"jbhist-more\"><summary>Show more</summary>"+ul;
            for (std::size_t i = 5; i < rows.size(); ++i) html += row(rows[i]);
            html += "</ul></details>";
        }
        html += "</div>";
        if (!viewer.empty())
            html += "<script>(function(){"
                    "document.querySelectorAll('a.jbtitle[data-yt]').forEach(function(a){"
                    "fetch('/jukebox/oembed?id='+encodeURIComponent(a.getAttribute('data-yt')))"
                    ".then(function(r){return r.ok?r.json():{};})"
                    ".then(function(d){if(d&&d.title)a.textContent=d.title;})"
                    // err-ok: no title is fine, the link keeps saying YouTube video
                    ".catch(function(){});"
                    "});"
                    "})();</script>";
    }

    html += "</div>";
    return html;
}

