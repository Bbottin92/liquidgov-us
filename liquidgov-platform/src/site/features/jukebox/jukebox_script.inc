// Floating jukebox widget: a phone-icon button, present on every logged-in
// page (injected from wrap()), that expands into a panel with two modes —
// "Community" (synced to the site-wide queue over /community/ws, same
// real-time pattern as /world/ws) and "My search" (private, local-only
// playback, never touches the server). One YT.Player instance is reused
// across both modes so play/pause/mute/volume always act on whatever's
// currently loaded.
//
// Honest limitation, not hidden: this site is deliberately a multi-page
// app (LIQUIDGOV_CONTEXT_HANDOFF.md's Jukebox section — no site-wide SPA).
// Navigating to a different page resets local playback; the widget
// reconnects and re-syncs to the live community track next time it's
// opened. True across-navigation persistence would need the browser's
// Picture-in-Picture API, deliberately not added here.
// Shared panel guts (player, tabs, controls, community/search sections) —
// identical whether embedded in the floating widget or filling the whole
// /listen popout window.
std::string jukebox_panel_body(bool standalone) {
    return
        // First child of #jbpanel/#jbroot either way this gets embedded
        // (see global_jukebox_widget and jukebox_standalone_page, both of
        // which wrap this same body) -- one shared change covers the
        // floating widget and the /listen popout at once.
        "<div class=\"juketube-plate\">Juketube</div>"
        "<div class=\"row\" style=\"justify-content:space-between;align-items:center\">"
        "<div class=\"row\" style=\"gap:.3rem\">"
        "<button type=\"button\" class=\"ghost\" id=\"jbtab-community\">Stations</button>"
        "<button type=\"button\" class=\"ghost\" id=\"jbtab-search\">Search</button>"
        "</div>"
        "<div class=\"row\" style=\"gap:.3rem\">"
        // Embedded (the phone widget on every page): collapse/expand is
        // the round phone button alone, so neither Pop out nor a second
        // close control belongs here. Standalone (/listen, its own
        // window) keeps both -- Float above to go always-on-top, and its
        // own Close to actually close that window.
        + std::string(standalone
            ? "<button type=\"button\" class=\"ghost\" id=\"jbfloat\" hidden>Float above</button>"
              "<button type=\"button\" class=\"ghost\" id=\"jbclose\" aria-label=\"Close\">\xE2\x9C\x95</button>"
            : "")
        + "</div>"
        "</div>"
        "<div id=\"jbplayer\" style=\"margin-top:.6rem;aspect-ratio:16/9;background:#000;"
        "border-radius:var(--rsm)\"></div>"
        "<div class=\"row\" style=\"margin-top:.5rem;align-items:center;gap:.4rem\">"
        "<button type=\"button\" class=\"ghost\" id=\"jbplaypause\">Play</button>"
        "<button type=\"button\" class=\"ghost\" id=\"jbmute\">Mute</button>"
        "<input type=\"range\" id=\"jbvolume\" min=\"0\" max=\"100\" value=\"70\" style=\"flex:1\">"
        "</div>"
        "<div id=\"jbcommunity-ui\">"
        // Priority-ordered, drag-to-reorder list of stations tuned in on
        // *this* widget -- see jukebox_script's stations block for the
        // reorder/click-to-temporarily-prioritize logic. Deliberately not
        // an account preference: "on the phone" is this device's own
        // widget config, in localStorage, same as it staying collapsed
        // across a soft-nav is this tab's own state.
        "<p class=\"muted\" style=\"margin:.5rem 0 .15rem;font-size:.72rem;letter-spacing:.08em;"
        "text-transform:uppercase\">Stations &mdash; drag to reorder, tap to tune in now</p>"
        "<div id=\"jbstationlist\" style=\"display:flex;flex-direction:column;gap:.2rem\"></div>"
        "<select id=\"jbstationadd\" style=\"margin-top:.35rem\"><option value=\"\">Add a station&hellip;</option></select>"
        "<div class=\"row\" style=\"margin-top:.3rem;align-items:flex-start\">"
        "<input id=\"jbfollow\" placeholder=\"Follow someone's Juketube\" autocomplete=\"off\" style=\"flex:1\">"
        "<button type=\"button\" class=\"ghost\" id=\"jbfollowbtn\">Follow</button>"
        "</div>"
        "<p class=\"muted\" id=\"jbfollowerr\" style=\"margin:.2rem 0 0\" hidden></p>"
        "<p class=\"muted\" id=\"jbnow\" style=\"margin:.6rem 0 0\">Connecting&hellip;</p>"
        "<p class=\"muted\" id=\"jbupcoming\" style=\"margin:.1rem 0 0\"></p>"
        "<input id=\"jbadd\" placeholder=\"Add to the queue\" autocomplete=\"off\" "
        "style=\"margin-top:.4rem;width:100%\">"
        "<div id=\"jbaddresults\" style=\"margin-top:.3rem;display:flex;flex-direction:column;gap:.15rem\"></div>"
        "<p class=\"muted\" id=\"jbadderr\" style=\"margin:.2rem 0 0\" hidden></p>"
        "</div>"
        "<div id=\"jbsearch-ui\" hidden>"
        "<select id=\"jbsearchcat\" style=\"width:100%\">"
        "<option value=\"all\">All</option><option value=\"group\">Group</option>"
        "<option value=\"friends\">Friends</option><option value=\"others\">Others</option>"
        "<option value=\"self\">Self</option></select>"
        "<input id=\"jbsearch\" placeholder=\"Search people and groups\" autocomplete=\"off\" "
        "style=\"margin-top:.4rem;width:100%\">"
        "<div id=\"jbsearchresults\" style=\"margin-top:.3rem;display:flex;flex-direction:column;gap:.15rem\"></div>"
        "<label class=\"muted\" style=\"display:block;margin-top:.7rem;font-size:.72rem;letter-spacing:.08em;"
        "text-transform:uppercase\">History</label>"
        "<select id=\"jbhistorysel\" style=\"width:100%;margin-top:.2rem\">"
        "<option value=\"\">Something you&rsquo;ve played before&hellip;</option></select>"
        "</div>";
}

// Shared controller JS. `standalone` selects between the two hosting
// contexts: embedded (toggle button + collapsible panel, Pop-out button
// opens /listen) vs the /listen window itself (always visible, Close button
// really closes the window, and — since *this* window's own document never
// navigates away as long as it stays open — a working "Float above" button
// using the real Document Picture-in-Picture API, moving the whole panel
// into an always-on-top OS-level window for as long as this tab stays open).
std::string jukebox_script(const std::string& yt_key, bool standalone, const std::string& stations_js,
                            const std::string& own_username) {
    (void)yt_key; // search goes through /jukebox/search so the key never ships in HTML
    std::string s =
        "<script>(function(){"
        "var STANDALONE="+std::string(standalone ? "true" : "false")+";"
        "var OWN_USERNAME="+js_string_literal(own_username)+";"
        "var tabC=document.getElementById('jbtab-community'),tabS=document.getElementById('jbtab-search');"
        "var playerDiv=document.getElementById('jbplayer');"
        "var playPauseBtn=document.getElementById('jbplaypause'),muteBtn=document.getElementById('jbmute');"
        "var volumeSlider=document.getElementById('jbvolume');"
        "var nowEl=document.getElementById('jbnow'),upcomingEl=document.getElementById('jbupcoming');"
        "var addInput=document.getElementById('jbadd'),addResults=document.getElementById('jbaddresults');"
        "var addErr=document.getElementById('jbadderr');"
        "var searchInput=document.getElementById('jbsearch'),searchResults=document.getElementById('jbsearchresults');"
        "var searchCat=document.getElementById('jbsearchcat'),historySel=document.getElementById('jbhistorysel');"
        "var communityUi=document.getElementById('jbcommunity-ui'),searchUi=document.getElementById('jbsearch-ui');"
        "var stationList=document.getElementById('jbstationlist'),stationAdd=document.getElementById('jbstationadd');"
        "var mode='community',ytPlayer=null,ws=null,muted=false;"
        "var pastEndReportedFor=null;" // see onStateChange's own comment -- one past-end self-heal report per video_id
        // ytPlayer itself goes non-null the instant `new YT.Player(...)`
        // is called -- its actual methods (loadVideoById etc.) aren't
        // guaranteed to exist until the async onReady event fires later.
        // playerReady/pendingPlayerCbs track real readiness separately so
        // a second ensurePlayer() call arriving in that gap queues behind
        // onReady instead of firing on a half-constructed player (caught
        // live: "ytPlayer.loadVideoById is not a function").
        "var playerReady=false,pendingPlayerCbs=[];"
        "var lastCommunityId=null,lastCommunityElapsed=0,lastKnownDuration=0,stationLoadedAt=0,ignoreEndedUntil=0;"
        "function saneStart(elapsed){"
        "elapsed=Number(elapsed);"
        "if(!isFinite(elapsed)||elapsed<0)return 0;"
        "if(elapsed>3*3600)return 0;"
        "if(lastKnownDuration>1&&elapsed>=lastKnownDuration-0.5)return 0;"
        "return elapsed;"
        "}"
        "function loadStationVideo(id,elapsed){"
        "lastKnownDuration=0;"
        "lastCommunityId=id;"
        "lastCommunityElapsed=saneStart(elapsed);"
        "stationLoadedAt=Date.now();"
        "ignoreEndedUntil=Date.now()+4000;ytPlayer.loadVideoById({videoId:id,startSeconds:0});"
        "}"
        "var pollTimer=null,activePersonalOwner=null;"
        "var followInput=document.getElementById('jbfollow'),followBtn=document.getElementById('jbfollowbtn'),"
        "followErr=document.getElementById('jbfollowerr');"
        // A personal channel's station id is "u:<username>" -- see
        // jukebox_stations_js. Everything else (site-wide "" or a
        // community slug) is a live WebSocket station.
        "function personalOwnerFromId(id){return id&&id.indexOf('u:')===0?id.slice(2):null;}"
        // Stations: which live jukeboxes (the site-wide one, id "", plus any
        // community this account belongs to) are tuned in on *this* widget,
        // in priority order (index 0 = plays). This is the device's own
        // widget config -- localStorage, not an account setting -- same
        // reasoning as the panel's own open/closed state never being saved
        // server-side. tempOverride is a click-to-listen-now override: set
        // by tapping a station's name, cleared by tapping it again or
        // choosing a different one, never persisted (it's explicitly
        // temporary -- see jukebox_panel_body's station row markup).
        "var STATIONS_AVAILABLE="+stations_js+";"
        "var STATIONS_KEY='liquidgov.jukebox.stations';"
        "var enabledOrder=[''];"
        "try{"
        "var saved=JSON.parse(localStorage.getItem(STATIONS_KEY)||'null');"
        "if(saved&&saved.length){"
        "var filtered=saved.filter(function(id){"
        "return STATIONS_AVAILABLE.some(function(s){return s.id===id;});"
        "});"
        "if(filtered.length)enabledOrder=filtered;"
        "}"
        "}catch(e){}"
        "var tempOverride=null,currentConnectedStation=undefined;"
        "function saveStations(){try{localStorage.setItem(STATIONS_KEY,JSON.stringify(enabledOrder));}catch(e){}}"
        "function stationName(id){"
        "var found=STATIONS_AVAILABLE.filter(function(s){return s.id===id;})[0];"
        "return found?found.name:id;"
        "}"
        "function activeStationId(){"
        "if(tempOverride!==null)return tempOverride;"
        "return enabledOrder.length?enabledOrder[0]:null;"
        "}"
        "function esc(s){var ta=document.createElement('textarea');ta.innerHTML=s;"
        "var d=document.createElement('div');d.textContent=ta.value;return d.innerHTML;}"
        "function looksLikeUrl(s){return /^https?:\\/\\//i.test(s)||/youtu\\.?be/i.test(s);}"
        "function updateTabs(){"
        "tabC.style.background=mode==='community'?'var(--accent)':'';tabC.style.color=mode==='community'?'var(--paper)':'';"
        "tabS.style.background=mode==='search'?'var(--accent)':'';tabS.style.color=mode==='search'?'var(--paper)':'';"
        "}"
        "function ensureYT(cb){"
        "if(window.YT&&window.YT.Player){cb();return;}"
        "if(!document.getElementById('jbytapi')){"
        "var s=document.createElement('script');s.id='jbytapi';s.src='https://www.youtube.com/iframe_api';"
        "document.head.appendChild(s);}"
        "var prev=window.onYouTubeIframeAPIReady;"
        "window.onYouTubeIframeAPIReady=function(){if(prev)prev();cb();};"
        "}"
        "function ensurePlayer(cb){"
        "if(playerReady){cb();return;}"
        "pendingPlayerCbs.push(cb);"
        "if(ytPlayer)return;" // construction already in flight -- onReady below drains the queue
        "ensureYT(function(){"
        "ytPlayer=new YT.Player('jbplayer',{height:'100%',width:'100%',"
        "playerVars:{rel:0,playsinline:1},events:{"
        "onReady:function(){"
        "playerReady=true;"
        "ytPlayer.setVolume(Number(volumeSlider.value));"
        "var cbs=pendingPlayerCbs;pendingPlayerCbs=[];"
        "cbs.forEach(function(fn){fn();});"
        "},"
        "onStateChange:function(e){"
        "if(e.data===YT.PlayerState.PLAYING){playPauseBtn.textContent='Pause';try{var dd=ytPlayer.getDuration();if(dd>1)lastKnownDuration=dd;}catch(err){}}"
        "else if(e.data===YT.PlayerState.PAUSED)playPauseBtn.textContent='Play';"
        "function reportEnded(){"
        "if(Date.now()<ignoreEndedUntil)return;"
        "var d=0,t=0;"
        "try{d=ytPlayer.getDuration()||0;t=ytPlayer.getCurrentTime()||0;}catch(err){}"
        "if(d>1)lastKnownDuration=d;"
        "if(d<1||lastCommunityElapsed>=d-0.5||(t<3&&(lastCommunityElapsed>5||Date.now()-stationLoadedAt<2500))){"
        "lastCommunityElapsed=0;"
        "try{ytPlayer.seekTo(0,true);ytPlayer.playVideo();}catch(err){}"
        "return;"
        "}"
        "if(ws&&ws.readyState===1)ws.send(JSON.stringify({ended:lastCommunityId||true}));"
        "else if(activePersonalOwner&&lastCommunityId)fetch("
        "'/u/'+encodeURIComponent(activePersonalOwner)+'/jukebox/complete',{method:'POST',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'video_id='+encodeURIComponent(lastCommunityId)});"
        "}"
        "if(e.data===YT.PlayerState.ENDED&&mode==='community')reportEnded();"
        // The shared elapsed clock only tracks real wall-clock time since
        // the last freeze/resume (see community_elapsed_locked's own
        // comment) -- it has no idea how long the actual video is, so
        // nothing stops it drifting past a short video's real end if
        // nobody's been actually watching (caught live: a 59s video
        // frozen at 734s, from an earlier round of WS testing that held
        // connections open without ever loading a real player). Loading
        // at or past the real duration would otherwise just sit there
        // glitching -- self-heal by reporting it finished immediately,
        // the same path a genuine ENDED event already takes, so the
        // queue advances instead of getting stuck.
        "if((e.data===YT.PlayerState.PLAYING||e.data===YT.PlayerState.CUED)&&mode==='community'&&"
        "typeof ytPlayer.getDuration==='function'){"
        "var dur=ytPlayer.getDuration();"
        "if(dur>0&&ytPlayer.getCurrentTime()>=dur-1&&pastEndReportedFor!==lastCommunityId){"
        "pastEndReportedFor=lastCommunityId;"
        "reportEnded();"
        "}"
        "}"
        // This closes, in order: the PLAYING/CUED-duration if-block above
        // (the one just before it), onStateChange's own function body,
        // and the events:{...} object -- three closes, not two. Missing
        // one of these left the *entire* jukebox_script IIFE with an
        // unbalanced brace, which is a parse-time SyntaxError: the whole
        // script tag silently fails to run, not just this block -- no
        // listener anywhere in the widget ever attaches. That's the real
        // explanation for "click never does anything, no console error":
        // there's no error to see because the script never started.
        "}}});"
        "});"
        "}"
        "function teardownWs(){"
        "if(ws){try{ws.close();}catch(e){}ws=null;}"
        "lastCommunityId=null;"
        "}"
        "function teardownPoll(){"
        "if(pollTimer){clearInterval(pollTimer);pollTimer=null;}"
        "activePersonalOwner=null;lastCommunityId=null;"
        "}"
        "function teardownActive(){teardownWs();teardownPoll();}"
        "function teardown(){"
        "teardownActive();"
        "if(ytPlayer){try{ytPlayer.destroy();}catch(e){}ytPlayer=null;}"
        "playerReady=false;pendingPlayerCbs=[];"
        "}"
        // Personal channels have no live sync -- a shared but ordinary HTTP
        // queue (see jukebox_widget), same one the profile page itself
        // shows at the front. Polling /jukebox/now every few seconds is
        // the widget's substitute for the WebSocket push the site-wide/
        // community stations get; good enough since a personal queue only
        // changes when someone queues or a track ends, not continuously.
        "function pollPersonal(owner){"
        "if(pollTimer)return;"
        "activePersonalOwner=owner;"
        "function tick(){"
        "fetch('/u/'+encodeURIComponent(owner)+'/jukebox/now').then(function(r){"
        "if(!r.ok)throw new Error('unavailable');return r.json();"
        "}).then(function(data){"
        "if(activePersonalOwner!==owner)return;"
        "if(data.now){"
        "nowEl.textContent='Now playing on '+esc(stationName('u:'+owner))+' \\u2014 queued by '+esc(data.now.queued_by);"
        "if(data.now.video_id!==lastCommunityId){"
        "lastCommunityId=data.now.video_id;lastCommunityElapsed=saneStart(data.now.elapsed||0);"
        "if(mode==='community')ensurePlayer(function(){"
        "loadStationVideo(lastCommunityId,lastCommunityElapsed);"
        "});"
        "}else{"
        "lastCommunityElapsed=saneStart(data.now.elapsed||0);"
        // Same shared-playhead drift correction the WS-backed stations
        // get from connectWs's onmessage -- the server clock is the one
        // that matters, this just catches a personal station's own
        // player up to it every few seconds instead of only on a track
        // change.
        "if(mode==='community'&&playerReady&&typeof ytPlayer.getCurrentTime==='function'"
        "&&ytPlayer.getPlayerState()===YT.PlayerState.PLAYING){"
        "var cur=ytPlayer.getCurrentTime();"
        "if(lastKnownDuration>1){var tgt=saneStart(lastCommunityElapsed);if(tgt<lastKnownDuration-0.5&&Math.abs(cur-tgt)>2)ytPlayer.seekTo(tgt,true);}"
        "}"
        "}"
        "}else{"
        "nowEl.textContent=esc(stationName('u:'+owner))+': nothing playing \\u2014 add a track below.';"
        "lastCommunityId=null;"
        "if(mode==='community'&&playerReady)ytPlayer.stopVideo();"
        "}"
        "upcomingEl.textContent=data.upcoming?(data.upcoming+' track'+(data.upcoming===1?'':'s')+' queued next'):'';"
        "}).catch(function(){"
        "if(activePersonalOwner!==owner)return;"
        "nowEl.textContent=esc(stationName('u:'+owner))+': can\\u2019t reach this channel right now.';"
        "});"
        "}"
        "tick();pollTimer=setInterval(tick,4000);"
        "}"
        // The polling widget's substitute for a WebSocket connect/
        // disconnect (see personal_heartbeat_locked/personal_freeze_locked
        // server-side) -- fires only while a personal station is both the
        // active one AND actually playing, not just open in the
        // background. video_id only, on purpose: the server clock is what
        // advances the shared position, never a client-reported
        // currentTime (two listeners drift differently; the server can't
        // referee that from a bare heartbeat, only from an explicit
        // freeze with its own reconciliation -- see personalFreezeBeacon).
        "function personalHeartbeatTick(){"
        "if(mode!=='community'||!activePersonalOwner||!playerReady||!lastCommunityId)return;"
        "if(ytPlayer.getPlayerState()!==YT.PlayerState.PLAYING)return;"
        "fetch('/u/'+encodeURIComponent(activePersonalOwner)+'/jukebox/pos',{method:'POST',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'video_id='+encodeURIComponent(lastCommunityId)});"
        "}"
        // Pause or pagehide on a personal station: stop right now instead
        // of waiting out the server's grace window, so the next listener
        // (or this same person, back later) resumes from here, not up to
        // 30s short of it. sendBeacon survives the page actually
        // unloading; a plain fetch might not finish in time.
        "function personalFreezeBeacon(){"
        "if(mode!=='community'||!activePersonalOwner||!playerReady||!lastCommunityId)return;"
        "var cur=0;try{cur=Math.round(ytPlayer.getCurrentTime());}catch(e){}"
        "var b='video_id='+encodeURIComponent(lastCommunityId)+'&freeze=1&offset='+cur;"
        "var url='/u/'+encodeURIComponent(activePersonalOwner)+'/jukebox/pos';"
        "if(navigator.sendBeacon){"
        "navigator.sendBeacon(url,new Blob([b],{type:'application/x-www-form-urlencoded'}));"
        "}else{"
        "fetch(url,{method:'POST',keepalive:true,"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});"
        "}"
        "}"
        "setInterval(personalHeartbeatTick,15000);"
        "window.addEventListener('pagehide',personalFreezeBeacon);"
        "function connectActive(id){"
        "if(id===null){nowEl.textContent='No station selected \\u2014 add one below.';upcomingEl.textContent='';"
        "if(playerReady)ytPlayer.stopVideo();return;}"
        "var owner=personalOwnerFromId(id);"
        "if(owner)pollPersonal(owner);else connectWs(id);"
        "}"
        "function connectWs(stationId){"
        "if(ws)return;"
        "var proto=location.protocol==='https:'?'wss:':'ws:';"
        "var path=stationId?('/c/'+encodeURIComponent(stationId)+'/jukebox/ws'):'/community/ws';"
        "ws=new WebSocket(proto+'//'+location.host+path);"
        "ws.onmessage=function(ev){"
        "var data;try{data=JSON.parse(ev.data);}catch(e){return;}"
        "if(data.now){"
        "nowEl.textContent='Now playing on '+esc(stationName(stationId))+' \\u2014 queued by '+esc(data.now.queued_by);"
        "if(data.now.video_id!==lastCommunityId){"
        "lastCommunityId=data.now.video_id;lastCommunityElapsed=saneStart(data.now.elapsed);"
        "if(mode==='community')ensurePlayer(function(){"
        "loadStationVideo(lastCommunityId,lastCommunityElapsed);"
        "});"
        "}else{"
        "lastCommunityElapsed=saneStart(data.now.elapsed);"
        // WS ticks every ~500ms, so drift here is normally tiny (a
        // reconnect or a stalled player are the real cases this catches);
        // same >2s threshold and reasoning as the personal/poll path.
        "if(mode==='community'&&playerReady&&typeof ytPlayer.getCurrentTime==='function'"
        "&&ytPlayer.getPlayerState()===YT.PlayerState.PLAYING){"
        "var cur=ytPlayer.getCurrentTime();"
        "if(lastKnownDuration>1){var tgt=saneStart(lastCommunityElapsed);if(tgt<lastKnownDuration-0.5&&Math.abs(cur-tgt)>2)ytPlayer.seekTo(tgt,true);}"
        "}"
        "}"
        "}else{"
        "nowEl.textContent=esc(stationName(stationId))+': nothing playing \\u2014 add a track below.';"
        "lastCommunityId=null;"
        "if(mode==='community'&&playerReady)ytPlayer.stopVideo();"
        "}"
        "upcomingEl.textContent=data.upcoming?(data.upcoming+' track'+(data.upcoming===1?'':'s')+' queued next'):'';"
        "};"
        "}"
        // Called after enabledOrder or tempOverride changes: re-renders the
        // station list (so the active one's highlight is right) and, only
        // when the *actual* active station changed, tears down and
        // reconnects the live socket to it -- a reorder that doesn't move
        // anything above the active one, or a temp-override toggle back to
        // the same station that was already playing, is a no-op here.
        "function onStationsChanged(persist){"
        "if(persist)saveStations();"
        "renderStations();renderStationAdd();"
        "var id=activeStationId();"
        "if(id===currentConnectedStation)return;"
        "currentConnectedStation=id;"
        "if(mode==='community'){"
        "teardownActive();"
        "connectActive(id);"
        "}"
        "}"
        "function renderStations(){"
        "var active=activeStationId();"
        "stationList.innerHTML='';"
        "enabledOrder.forEach(function(id){"
        "var row=document.createElement('div');"
        "row.className='jbstation-row'+(id===active?' active':'');"
        "row.dataset.id=id;"
        "var handle=document.createElement('span');handle.className='jbdraghandle';"
        "handle.textContent='\\u283F';handle.setAttribute('aria-hidden','true');"
        "var name=document.createElement('span');name.className='jbstation-name';"
        "name.textContent=stationName(id);name.title='Tap to listen to this now';"
        "name.addEventListener('click',function(){"
        "tempOverride=(tempOverride===id)?null:id;"
        "onStationsChanged(false);"
        "});"
        "var remove=document.createElement('button');remove.type='button';remove.className='jbstation-remove';"
        "remove.setAttribute('aria-label','Remove station');remove.textContent='\\u2715';"
        "remove.addEventListener('click',function(e){"
        "e.stopPropagation();"
        "enabledOrder=enabledOrder.filter(function(x){return x!==id;});"
        "if(tempOverride===id)tempOverride=null;"
        "onStationsChanged(true);"
        "var removedOwner=personalOwnerFromId(id);"
        "if(removedOwner)fetch('/u/'+encodeURIComponent(removedOwner)+'/jukebox/unsubscribe',{method:'POST'});"
        "});"
        "row.appendChild(handle);row.appendChild(name);row.appendChild(remove);"
        "stationList.appendChild(row);"
        "});"
        "}"
        "function renderStationAdd(){"
        "stationAdd.innerHTML='';"
        "var ph=document.createElement('option');ph.value='__placeholder__';ph.textContent='Add a station\\u2026';"
        "stationAdd.appendChild(ph);"
        "STATIONS_AVAILABLE.forEach(function(s){"
        "if(enabledOrder.indexOf(s.id)!==-1)return;"
        "var opt=document.createElement('option');opt.value=s.id;opt.textContent=s.name;"
        "stationAdd.appendChild(opt);"
        "});"
        "stationAdd.value='__placeholder__';"
        "}"
        "stationAdd.addEventListener('change',function(){"
        "var id=stationAdd.value;"
        "if(id==='__placeholder__')return;"
        "if(enabledOrder.indexOf(id)===-1){enabledOrder.push(id);onStationsChanged(true);}"
        "});"
        // Following someone's personal Juketube is a real, server-recorded
        // subscription (not just this device's local station list) so the
        // owner can see who's tuned in -- see FileStore::subscribe_jukebox.
        // Unlike the fixed dropdown above, any username can be typed here.
        "function doFollow(){"
        "var name=followInput.value.trim();"
        "followErr.hidden=true;"
        "if(!name)return;"
        "followBtn.disabled=true;"
        "fetch('/u/'+encodeURIComponent(name)+'/jukebox/subscribe',{method:'POST'}).then(function(r){"
        "followBtn.disabled=false;"
        "if(!r.ok){"
        "followErr.textContent=r.status===404?'No one by that name.':'Couldn\\u2019t follow that channel.';"
        "followErr.hidden=false;return;"
        "}"
        "var id='u:'+name;"
        "if(!STATIONS_AVAILABLE.some(function(s){return s.id===id;}))"
        "STATIONS_AVAILABLE.push({id:id,name:name+'\\u2019s Juketube'});"
        "followInput.value='';"
        "if(enabledOrder.indexOf(id)===-1){enabledOrder.push(id);onStationsChanged(true);}"
        "}).catch(function(){"
        "followBtn.disabled=false;followErr.textContent='Couldn\\u2019t reach the server.';followErr.hidden=false;"
        "});"
        "}"
        "followBtn.addEventListener('click',doFollow);"
        "followInput.addEventListener('keydown',function(e){if(e.key==='Enter'){e.preventDefault();doFollow();}});"
        // Pointer-based reorder (not native HTML5 drag, which doesn't work
        // on touch): drag the handle, the row under the pointer's midpoint
        // is where it lands, dropped position becomes the new saved order.
        "(function attachStationDrag(){"
        "var dragRow=null;"
        "function rowUnder(clientY){"
        "var rows=Array.prototype.slice.call(stationList.querySelectorAll('.jbstation-row'));"
        "var after=null;"
        "rows.forEach(function(r){"
        "if(r===dragRow)return;"
        "var box=r.getBoundingClientRect();"
        "if(clientY>box.top+box.height/2)after=r;"
        "});"
        "return after;"
        "}"
        "stationList.addEventListener('pointerdown',function(e){"
        "var row=e.target.closest&&e.target.closest('.jbstation-row');"
        "var handle=e.target.closest&&e.target.closest('.jbdraghandle');"
        "if(!row||!handle)return;"
        "dragRow=row;"
        "try{row.setPointerCapture(e.pointerId);}catch(err){}"
        "row.style.zIndex='5';row.style.opacity='.85';"
        "});"
        "stationList.addEventListener('pointermove',function(e){"
        "if(!dragRow)return;"
        "var after=rowUnder(e.clientY);"
        "var target=after?after.nextSibling:stationList.firstChild;"
        "if(target!==dragRow)stationList.insertBefore(dragRow,target);"
        "});"
        "function drop(){"
        "if(!dragRow)return;"
        "dragRow.style.zIndex='';dragRow.style.opacity='';"
        "dragRow=null;"
        "enabledOrder=Array.prototype.slice.call(stationList.querySelectorAll('.jbstation-row'))"
        ".map(function(r){return r.dataset.id;});"
        "onStationsChanged(true);"
        "}"
        "stationList.addEventListener('pointerup',drop);"
        "stationList.addEventListener('pointercancel',drop);"
        "})();"
        "function switchMode(m){"
        "mode=m;updateTabs();"
        "communityUi.hidden=(m!=='community');searchUi.hidden=(m!=='search');"
        "if(m==='community'){"
        "var id=activeStationId();"
        "if(id!==currentConnectedStation||(!ws&&!pollTimer)){"
        "currentConnectedStation=id;teardownActive();"
        "connectActive(id);"
        "}"
        "/* keep current video on Stations tab */"
        "}"
        "}"
        "function attachLiveSearch(input,resultsEl,onPick){"
        "var timer=null,gen=0;"
        "input.addEventListener('input',function(){"
        "var val=input.value.trim();clearTimeout(timer);"
        "if(!val||looksLikeUrl(val)){resultsEl.innerHTML='';return;}"
        "timer=setTimeout(function(){"
        "var my=++gen;"
        "resultsEl.textContent='Searching...';"
        "fetch('/jukebox/search?q='+encodeURIComponent(val),{credentials:'same-origin'})"
        ".then(function(r){if(!r.ok)throw new Error('http '+r.status);return r.json();})"
        ".then(function(data){"
        "if(my!==gen)return;"
        "resultsEl.innerHTML='';"
        "var items=data.items||[];"
        "if(!items.length){resultsEl.textContent='No matches.';return;}"
        "items.forEach(function(it){"
        "var id=(it.id&&it.id.videoId)||'';"
        "var title=(it.snippet&&it.snippet.title)||'untitled';"
        "var th=it.snippet&&it.snippet.thumbnails&&(it.snippet.thumbnails.default||it.snippet.thumbnails.medium);"
        "var thumb=(th&&th.url)||'';"
        "if(!id)return;"
        "var row=document.createElement('div');row.className='row';"
        "row.style.cssText='align-items:center;cursor:pointer;padding:.25rem;border-radius:var(--rsm)';"
        "row.innerHTML='<img src=\"'+thumb+'\" width=\"48\" height=\"36\" alt=\"\" style=\"border-radius:4px\">"
        "<span style=\"flex:1;font-size:.85rem\">'+esc(title)+'</span>';"
        "row.addEventListener('click',function(){"
        "input.value='';resultsEl.innerHTML='';onPick('https://www.youtube.com/watch?v='+id);"
        "});"
        "resultsEl.appendChild(row);"
        "});"
        "}).catch(function(){if(my===gen)resultsEl.textContent='Search failed.';});"
        "},300);"
        "});"
        "input.addEventListener('keydown',function(e){"
        "if(e.key==='Enter'){"
        "e.preventDefault();"
        "var val=input.value.trim();"
        "if(val&&looksLikeUrl(val)){input.value='';resultsEl.innerHTML='';onPick(val);}"
        "}"
        "});"
        "}"
        "attachLiveSearch(addInput,addResults,function(url){"
        "if(addErr)addErr.hidden=true;"
        // Every mutating fetch below either updates something real (the
        // widget re-tunes) or, on failure, tells the person right next to
        // the input -- a POST that just vanishes into an empty .catch is
        // how "add to queue" used to silently do nothing.
        "function fail(){if(addErr){addErr.textContent='Could not reach the server. Try again.';addErr.hidden=false;}}"
        "if(activePersonalOwner){"
        "fetch('/u/'+encodeURIComponent(activePersonalOwner)+'/jukebox/add',{method:'POST',credentials:'same-origin',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'url='+encodeURIComponent(url)}).then(function(r){if(!r.ok)fail();}).catch(fail);"
        "}else if(ws&&ws.readyState===1){"
        "ws.send(JSON.stringify({add:url}));"
        "}else{"
        // Neither a personal channel nor a live community socket is
        // connected -- tuning to nothing was a silent no-op click. Add to
        // the viewer's own queue instead and tune the widget there, so
        // the click both persists (jukebox_of(OWN_USERNAME) actually
        // gains the track) and plays immediately rather than waiting on
        // a poll tick that was never going to happen with no station
        // connected.
        "fetch('/me/jukebox/add',{method:'POST',credentials:'same-origin',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'url='+encodeURIComponent(url)}).then(function(r){"
        "if(!r.ok){fail();return;}"
        // tempOverride/onStationsChanged/switchMode are all defined
        // unconditionally further up this same script -- used directly
        // (not through window.__lgTuneStation, which only exists in the
        // embedded, non-standalone widget) so this fallback works the
        // same way on the /listen popout too.
        "if(!OWN_USERNAME)return;"
        "tempOverride='u:'+OWN_USERNAME;"
        "if(mode!=='community')switchMode('community');else onStationsChanged(false);"
        "}).catch(fail);"
        "}"
        "});"
        // Search tab: browse people/communities by category and tune the
        // widget straight to one -- not a YouTube search (that stays on
        // the Stations tab's "Add to the queue" input above, the existing
        // /jukebox/search proxy). Clicking a row calls the same
        // __lgTuneStation bridge a profile's own "Play in Juketube"
        // button uses, so there's still only ever one player on the page.
        "var searchTimer=null,searchGen=0;"
        "function runSearch(){"
        "var term=searchInput.value.trim();"
        "clearTimeout(searchTimer);"
        "searchTimer=setTimeout(function(){"
        "var my=++searchGen;"
        "fetch('/jukebox/browse?category='+encodeURIComponent(searchCat.value)+"
        "'&q='+encodeURIComponent(term))"
        ".then(function(r){return r.json();})"
        ".then(function(data){"
        "if(my!==searchGen)return;"
        "searchResults.innerHTML='';"
        "(data.items||[]).forEach(function(it){"
        "var row=document.createElement('div');row.className='row';"
        "row.style.cssText='align-items:center;cursor:pointer;padding:.3rem;border-radius:var(--rsm)';"
        "row.textContent=it.name;"
        "row.addEventListener('click',function(){window.__lgTuneStation(it.id);});"
        "searchResults.appendChild(row);"
        "});"
        "}).catch(function(){});"
        "},250);"
        "}"
        "searchCat.addEventListener('change',runSearch);"
        "searchInput.addEventListener('input',runSearch);"
        "runSearch();"
        // History: newest first. A personal channel's history is scoped by
        // owner (?for=); a community/site-wide station's own history (bug
        // fix: this used to always send ?for= with an empty owner, which
        // isn't a station lookup at all -- it silently fell back to the
        // VIEWER'S OWN plays, not the station's) is scoped by ?station=
        // instead, including "" for the site-wide station itself.
        "function loadHistory(){"
        "var sid=activeStationId();var owner=personalOwnerFromId(sid);"
        "var q=owner?('for='+encodeURIComponent(owner)):('station='+encodeURIComponent(sid||''));"
        "fetch('/jukebox/history?'+q)"
        ".then(function(r){return r.json();})"
        ".then(function(data){"
        "historySel.innerHTML='<option value=\"\">Something you\\u2019ve played before\\u2026</option>';"
        "(data.items||[]).forEach(function(it,idx){"
        "var opt=document.createElement('option');opt.value=String(idx);"
        "opt.textContent=it.queued_by;"
        "opt.dataset.videoId=it.video_id;"
        "historySel.appendChild(opt);"
        "});"
        "}).catch(function(){});"
        "}"
        "historySel.addEventListener('focus',loadHistory);"
        "historySel.addEventListener('change',function(){"
        "var opt=historySel.selectedOptions[0];"
        "if(!opt||!opt.dataset.videoId)return;"
        "var videoId=opt.dataset.videoId;"
        "if(ws&&ws.readyState===1)ws.send(JSON.stringify({play:'https://www.youtube.com/watch?v='+videoId}));"
        "switchMode('community');"
        "ensurePlayer(function(){ytPlayer.loadVideoById(videoId);});"
        "historySel.value='';"
        "});"
        "if(!STANDALONE){"
        "var toggle=document.getElementById('jbtoggle'),panel=document.getElementById('jbpanel');"
        "toggle.addEventListener('click',function(e){"
        "e.stopPropagation();"
        "var opening=panel.style.display==='none';"
        "panel.style.display=opening?'block':'none';"
        // Deferred a frame so the un-hide paints before ensurePlayer's
        // synchronous new YT.Player(...) iframe creation runs -- see
        // #jbpanel's own comment for why same-tick was the actual bug.
        "if(opening)requestAnimationFrame(function(){"
        "ensurePlayer(function(){});"
        "if(mode==='community'&&!ws&&!pollTimer){currentConnectedStation=activeStationId();connectActive(currentConnectedStation);}"
        "});"
        "});"
        // No Pop out or Close in the embedded widget anymore -- the round
        // phone button (jbtoggle, above) is the only collapse/expand
        // control. window.__lgTuneStation exposes a way for other scripts
        // on the page (a profile's "Play in Juketube" button, the
        // autoplay-on-visit script) to open this same panel, switch it to
        // Stations, and tune it at a station id -- "" for site-wide, a
        // community slug, or "u:<username>" for a personal channel --
        // without spinning up a second YT.Player anywhere on the page.
        "window.__lgTuneStation=function(id){"
        "panel.style.display='block';"
        "tempOverride=id;"
        "if(mode!=='community')switchMode('community');"
        "else onStationsChanged(false);"
        "};"
        // Soft navigation: intercept plain same-origin link clicks and swap
        // <main>'s content via fetch instead of a full page load, so the
        // widget (a sibling of <main>, see wrap()) and its live
        // WebSocket/YT player are simply never touched by an ordinary click
        // around the site. Forms/POSTs are untouched (this only hooks
        // anchor clicks) and still do a normal full-page round trip.
        "document.addEventListener('click',function(e){"
        "if(e.defaultPrevented||e.button!==0||e.metaKey||e.ctrlKey||e.shiftKey||e.altKey)return;"
        "var a=e.target.closest&&e.target.closest('a[href]');"
        "if(!a||a.target&&a.target!=='_self'||a.hasAttribute('download'))return;"
        "var url;try{url=new URL(a.href,location.href);}catch(err){return;}"
        "if(url.origin!==location.origin||url.pathname===location.pathname&&url.search===location.search&&url.hash)return;"
        // /theme/<mode> flips the data-theme attribute on <html>, which sits
        // outside both regions softNavigate ever touches (main.wrap and
        // header.top nav) -- a soft nav here sets the cookie and swaps
        // content that already assumed the OLD theme, so nothing visibly
        // changes until some later real page load re-renders <html> from
        // scratch. Reads as "the toggle doesn't work" (Brandon, live on
        // liquidgov.us) until you hit refresh, at which point it's already
        // correct. Letting this one path fall through to a real navigation
        // is simpler and safer than teaching softNavigate to also patch
        // <html>'s attribute on every navigation just for this one case.
        "if(url.pathname.indexOf('/theme/')===0)return;"
        "e.preventDefault();"
        "softNavigate(url.href,true);"
        "});"
        "window.addEventListener('popstate',function(){softNavigate(location.href,false);});"
        "function reExecuteScripts(container){"
        "Array.prototype.forEach.call(container.querySelectorAll('script'),function(old){"
        "var fresh=document.createElement('script');"
        "for(var i=0;i<old.attributes.length;i++)fresh.setAttribute(old.attributes[i].name,old.attributes[i].value);"
        "fresh.textContent=old.textContent;"
        "old.parentNode.replaceChild(fresh,old);"
        "});"
        "}"
        "function softNavigate(url,pushState){"
        "fetch(url,{headers:{'X-Soft-Nav':'1'}}).then(function(r){"
        "if(!r.ok)throw new Error('nav failed');"
        "return r.text().then(function(html){return{html:html,finalUrl:r.url};});"
        "}).then(function(res){"
        "var doc=new DOMParser().parseFromString(res.html,'text/html');"
        "var newMain=doc.querySelector('main.wrap');"
        "if(!newMain){location.href=url;return;}"
        "document.querySelector('main.wrap').innerHTML=newMain.innerHTML;"
        "reExecuteScripts(document.querySelector('main.wrap'));"
        "var newNav=doc.querySelector('header.top nav');"
        "var navEl=document.querySelector('header.top nav');"
        "if(navEl&&newNav)navEl.innerHTML=newNav.innerHTML;"
        "document.title=doc.title;"
        "if(pushState)history.pushState({},'',res.finalUrl);"
        "window.scrollTo(0,0);"
        "}).catch(function(){location.href=url;});"
        "}"
        "}else{"
        // This whole branch only runs when STANDALONE is true (see its
        // own var declaration above), which is exactly when jbclose
        // exists in the DOM -- but guarding the lookup anyway costs
        // nothing and survives this branch ever being restructured
        // later without silently reintroducing a null-deref crash.
        "var closeBtn=document.getElementById('jbclose');"
        "if(closeBtn)closeBtn.addEventListener('click',function(){window.close();});"
        "ensurePlayer(function(){});"
        "currentConnectedStation=activeStationId();connectActive(currentConnectedStation);"
        "var floatBtn=document.getElementById('jbfloat');"
        "if('documentPictureInPicture' in window){"
        "floatBtn.hidden=false;"
        "floatBtn.addEventListener('click',function(){"
        "documentPictureInPicture.requestWindow({width:340,height:300}).then(function(pipWin){"
        "Array.prototype.forEach.call(document.styleSheets,function(sheet){"
        "try{"
        "if(sheet.ownerNode&&sheet.ownerNode.tagName==='STYLE'){"
        "var style=document.createElement('style');"
        "style.textContent=sheet.ownerNode.textContent;"
        "pipWin.document.head.appendChild(style);"
        "}"
        "}catch(e){}"
        "});"
        "pipWin.document.body.style.margin='0';"
        "pipWin.document.body.appendChild(document.getElementById('jbroot'));"
        "pipWin.addEventListener('pagehide',function(){"
        "document.body.appendChild(document.getElementById('jbroot'));"
        "});"
        "}).catch(function(e){"
        "console.error('Float above failed:',e);"
        "floatBtn.textContent='Couldn\\u2019t float \\u2014 try again';"
        "setTimeout(function(){floatBtn.textContent='Float above';},2500);"
        "});"
        "});"
        "}"
        "}"
        "tabC.addEventListener('click',function(){switchMode('community');});"
        "tabS.addEventListener('click',function(){switchMode('search');});"
        "playPauseBtn.addEventListener('click',function(){"
        "if(!playerReady)return;"
        "if(ytPlayer.getPlayerState()===YT.PlayerState.PLAYING){ytPlayer.pauseVideo();personalFreezeBeacon();}"
        "else{if(mode==='community'&&lastCommunityId)ytPlayer.seekTo(saneStart(lastCommunityElapsed),true);ytPlayer.playVideo();}"
        "});"
        "muteBtn.addEventListener('click',function(){"
        "if(!ytPlayer)return;"
        "muted=!muted;"
        "if(muted){ytPlayer.mute();muteBtn.textContent='Unmute';}else{ytPlayer.unMute();muteBtn.textContent='Mute';}"
        "});"
        "volumeSlider.addEventListener('input',function(){"
        "if(ytPlayer)ytPlayer.setVolume(Number(volumeSlider.value));"
        "});"
        // YouTube's own embedded player frequently pauses itself when its
        // tab goes to the background (a documented IFrame Player quirk,
        // distinct from — and in addition to — generic browser tab
        // throttling). Catch back up to the live community position
        // instead of leaving it stalled whenever the tab is visible again.
        "document.addEventListener('visibilitychange',function(){"
        "if(!document.hidden&&mode==='community'&&playerReady&&lastCommunityId){"
        "if(ytPlayer.getPlayerState()!==YT.PlayerState.PLAYING){"
        "ytPlayer.seekTo(saneStart(lastCommunityElapsed),true);ytPlayer.playVideo();"
        "}"
        "}"
        "});"
        "updateTabs();renderStations();renderStationAdd();"
        "})();</script>";
    return s;
}

