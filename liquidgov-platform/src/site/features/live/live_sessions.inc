// World client page: a <canvas>, WASD/arrow movement, one WebSocket to
// /world/ws. Sends {"x":n,"y":n} on movement; server broadcasts the full
// player snapshot back on an interval regardless of whether this client
// moved, so remote players animate smoothly too. Matches the documented
// MVP: "One shared room, two people see each other move."
std::string world_page(const std::string& username) {
    std::string html =
        "<div class=\"card\"><h1>World</h1>"
        "<p class=\"muted\">One shared room. Arrow keys or WASD to move. Closing this page removes you.</p>"
        "<canvas id=\"wcanvas\" width=\"800\" height=\"500\" "
        "style=\"border:1px solid var(--rule);border-radius:var(--rsm);background:#eee7d6;"
        "max-width:100%;touch-action:none\"></canvas>"
        "<p class=\"muted\" id=\"wstatus\">Connecting&hellip;</p></div>"
        "<script>(function(){"
        "var me="+js_string_literal(username)+";"
        "var cv=document.getElementById('wcanvas'),ctx=cv.getContext('2d');"
        "var st=document.getElementById('wstatus');"
        "var players={};"
        "var proto=location.protocol==='https:'?'wss:':'ws:';"
        "var ws=new WebSocket(proto+'//'+location.host+'/world/ws');"
        "var keys={};"
        "var opened=false;"
        "ws.onopen=function(){opened=true;st.textContent='Connected. Arrow keys or WASD to move.';};"
        "ws.onclose=function(){"
        "st.textContent=opened?'Disconnected. Reload to rejoin.':'Could not reach the room. Reload to try again.';"
        "};"
        "ws.onerror=function(){"
        "if(!opened)st.textContent='Could not reach the room. Reload to try again.';"
        "};"
        "setTimeout(function(){"
        "if(!opened)st.textContent='Still connecting. If this hangs, reload.';"
        "},4000);"
        "ws.onmessage=function(ev){"
        "try{var data=JSON.parse(ev.data);players=data.players||{};}catch(e){}"
        "draw();"
        "};"
        "function draw(){"
        "ctx.clearRect(0,0,cv.width,cv.height);"
        "Object.keys(players).forEach(function(id){"
        "var p=players[id];"
        "ctx.beginPath();ctx.arc(p.x,p.y,14,0,Math.PI*2);"
        "ctx.fillStyle=p.color;ctx.fill();"
        "if(p.username===me){ctx.lineWidth=2;ctx.strokeStyle='#1a1814';ctx.stroke();}"
        "ctx.fillStyle='#1a1814';ctx.font='12px sans-serif';ctx.textAlign='center';"
        "ctx.fillText(p.username,p.x,p.y-20);"
        "});"
        "}"
        "document.addEventListener('keydown',function(e){keys[e.key.toLowerCase()]=true;});"
        "document.addEventListener('keyup',function(e){keys[e.key.toLowerCase()]=false;});"
        "var x=cv.width/2,y=cv.height/2;"
        "setInterval(function(){"
        "if(ws.readyState!==1)return;"
        "var dx=0,dy=0,speed=5;"
        "if(keys['arrowleft']||keys['a'])dx-=speed;"
        "if(keys['arrowright']||keys['d'])dx+=speed;"
        "if(keys['arrowup']||keys['w'])dy-=speed;"
        "if(keys['arrowdown']||keys['s'])dy+=speed;"
        "if(dx||dy){"
        "x=Math.max(14,Math.min(cv.width-14,x+dx));"
        "y=Math.max(14,Math.min(cv.height-14,y+dy));"
        "ws.send(JSON.stringify({x:x,y:y}));"
        "}"
        "},50);"
        "})();</script>";
    return html;
}

// One thread per connected /world/ws client. Runs the RFC 6455 handshake
// response, then loops: try to read this client's new position (short
// timeout so the loop can broadcast even when nobody moved), update the
// shared registry, and push everyone's current positions back to this
// client. Removes the client from the registry on disconnect.
void run_world_session(int fd, std::string username) {
    timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 100000; // 100ms
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    {
        std::lock_guard<std::mutex> lock(g_world_mu);
        WorldPlayer p;
        p.username = username;
        p.color = world_color_for(username);
        g_world_players[fd] = p;
    }

    for (;;) {
        auto result = ws_read_frame(fd);
        if (result.status == WsReadStatus::Closed) break;
        if (result.status == WsReadStatus::Ok) {
            if (result.frame.opcode == WsOpcode::Close) break;
            if (result.frame.opcode == WsOpcode::Text) {
                // Minimal ad-hoc parse of {"x":<num>,"y":<num>} — the only
                // shape this endpoint ever receives, so a general JSON
                // parser would be pure overhead here.
                const std::string& s = result.frame.payload;
                auto extract = [&](const char* key) -> std::optional<double> {
                    auto p = s.find(key);
                    if (p == std::string::npos) return std::nullopt;
                    p = s.find(':', p);
                    if (p == std::string::npos) return std::nullopt;
                    try { return std::stod(s.substr(p + 1)); } catch (...) { return std::nullopt; }
                };
                auto nx = extract("\"x\"");
                auto ny = extract("\"y\"");
                if (nx && ny) {
                    std::lock_guard<std::mutex> lock(g_world_mu);
                    auto it = g_world_players.find(fd);
                    if (it != g_world_players.end()) { it->second.x = *nx; it->second.y = *ny; }
                }
            }
        }
        // status == Timeout falls through here too — always re-broadcast.

        std::string snapshot;
        {
            std::lock_guard<std::mutex> lock(g_world_mu);
            if (g_world_players.find(fd) == g_world_players.end()) break;
            std::ostringstream os;
            os << "{\"players\":{";
            bool first = true;
            for (auto& [id, p] : g_world_players) {
                if (!first) os << ",";
                first = false;
                os << "\"" << id << "\":{\"x\":" << p.x << ",\"y\":" << p.y
                   << ",\"username\":\"" << p.username << "\",\"color\":\"" << p.color << "\"}";
            }
            os << "}}";
            snapshot = os.str();
        }
        if (!ws_write_frame(fd, WsOpcode::Text, snapshot)) break;
    }

    {
        std::lock_guard<std::mutex> lock(g_world_mu);
        g_world_players.erase(fd);
    }
    close(fd);
}

// Minimal ad-hoc extractor for a top-level {"key":"value"} string field —
// same "only shape this endpoint ever receives" reasoning as the world
// socket's x/y number parser. Not escape-aware: fine for URLs, which never
// contain a literal quote character.
std::optional<std::string> extract_json_string_field(const std::string& s, const char* key) {
    auto p = s.find(key);
    if (p == std::string::npos) return std::nullopt;
    p = s.find(':', p);
    if (p == std::string::npos) return std::nullopt;
    auto open = s.find('"', p);
    if (open == std::string::npos) return std::nullopt;
    auto close_pos = s.find('"', open + 1);
    if (close_pos == std::string::npos) return std::nullopt;
    return s.substr(open + 1, close_pos - open - 1);
}

// One thread per connected station client (/community/ws for the "" site-
// wide station, /c/<slug>/jukebox/ws for a community's own) — same shape
// as run_world_session, entirely separate shared state (g_community_mu).
// Membership (for a real station) was already checked before this thread
// was spawned, same as everything else here trusts the caller. Any
// connected member can add a track (open queue within the station, same
// posture the site-wide one always had); the queue auto-advances when any
// connected client's player reports the current track ended.
void run_community_session(int fd, const std::string& username, const std::string& station) {
    timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 500000; // 500ms — a shared queue doesn't need world's 100ms cadence
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Joining is a 0->1 transition on this station's listener count: if
    // nobody else is here either, resume from wherever the freeze left
    // off (durable jpos, so this survives a process restart too, not just
    // a run of the same process with everyone gone and back). If someone
    // else is already listening (count already >0), there's nothing to
    // do here -- community_elapsed_locked already gives this new
    // connection the same live position everyone else has.
    {
        std::lock_guard<std::mutex> lock(g_community_mu);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int& count = g_community_listeners_by_station[station];
        const bool was_zero = (count == 0);
        ++count;
        if (was_zero) {
            if (auto pos = g_store.jukebox_pos(station)) {
                if (pos->offset_sec > kJukeboxResumeMaxSec) {
                    g_store.clear_jukebox_pos(station);
                } else {
                    auto& np = g_community_now_by_station[station];
                    np.video_id = pos->video_id;
                    np.frozen_offset = pos->offset_sec;
                    np.resumed_at = now;
                    np.active = true;
                }
            } else if (g_community_now_by_station[station].active) {
                g_community_now_by_station[station].resumed_at = now;
            }
        }
    }

    std::string reported_unplayable; // this connection's one vote per track (see g_community_unplayable_by_station)
    for (;;) {
        auto result = ws_read_frame(fd);
        if (result.status == WsReadStatus::Closed) break;
        if (result.status == WsReadStatus::Ok) {
            if (result.frame.opcode == WsOpcode::Close) break;
            if (result.frame.opcode == WsOpcode::Text) {
                const std::string& s = result.frame.payload;
                if (auto raw = extract_json_string_field(s, "\"add\"")) {
                    if (auto id = extract_youtube_id(trim_ws(*raw))) {
                        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        std::lock_guard<std::mutex> lock(g_community_mu);
                        auto& now_playing = g_community_now_by_station[station];
                        auto& upcoming = g_community_upcoming_by_station[station];
                        if (!now_playing.active && upcoming.empty()) {
                            now_playing = CommunityNowPlaying{*id, username, 0, now, true};
                            g_store.set_jukebox_pos(station, *id, 0, now);
                            g_community_last_flush_by_station[station] = now;
                        } else {
                            upcoming.push_back(CommunityTrack{*id, username});
                        }
                    }
                } else if (auto raw = extract_json_string_field(s, "\"play\"")) {
                    // A click starts the video when this listener is the only one
                    // connected, or when the station is idle. With someone else
                    // listening, it queues, so one person cannot skip the room.
                    if (auto id = extract_youtube_id(trim_ws(*raw))) {
                        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        std::lock_guard<std::mutex> lock(g_community_mu);
                        auto& now_playing = g_community_now_by_station[station];
                        auto& upcoming = g_community_upcoming_by_station[station];
                        const int listeners = g_community_listeners_by_station[station];
                        const bool alone = listeners <= 1;
                        if (alone || (!now_playing.active && upcoming.empty())) {
                            if (now_playing.active && now_playing.video_id != *id) {
                                auto& hist = g_community_history_by_station[station];
                                hist.push_back(CommunityTrack{now_playing.video_id, now_playing.queued_by});
                                if (hist.size() > kCommunityHistoryCap) hist.pop_front();
                            }
                            now_playing = CommunityNowPlaying{*id, username, 0, now, true};
                            g_store.set_jukebox_pos(station, *id, 0, now);
                            g_community_last_flush_by_station[station] = now;
                        } else {
                            upcoming.push_back(CommunityTrack{*id, username});
                        }
                        lglog::emit("jukebox", "play "+station+" "+*id+" by "+username);
                    }
                } else if (s.find("\"ended\"") != std::string::npos || s.find("\"unplayable\"") != std::string::npos) {
                    const bool unplayable = s.find("\"unplayable\"") != std::string::npos;
                    // {"ended":"<id>"} / {"unplayable":"<id>"} name the track; an old client's {"ended":true} names none.
                    const auto named = extract_json_string_field(s, unplayable ? "\"unplayable\"" : "\"ended\"");
                    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    std::lock_guard<std::mutex> lock(g_community_mu);
                    auto& now_playing = g_community_now_by_station[station];
                    const int listeners = g_community_listeners_by_station[station];
                    const bool alone = listeners <= 1;
                    bool advance = false;
                    if (!now_playing.active) {
                        // nothing to end
                    } else if (named && *named != now_playing.video_id) {
                        lglog::emit("jukebox", "ended-stale " + station + " by " + username);
                    } else if (unplayable) {
                        if (named && reported_unplayable != now_playing.video_id) {
                            reported_unplayable = now_playing.video_id;
                            auto& votes = g_community_unplayable_by_station[station];
                            if (votes.first != now_playing.video_id) votes = {now_playing.video_id, 0};
                            ++votes.second;
                        }
                        const auto& votes = g_community_unplayable_by_station[station];
                        const int n = votes.first == now_playing.video_id ? votes.second : 0;
                        // Brandon 2026-11-08: skip "if it wont play for at least 25% or more people trying to use
                        // juketube" -- so one silent listener (background tab, old page) can't stall the room.
                        advance = n > 0 && (alone || n * 4 >= listeners);
                        lglog::emit("jukebox", "unplayable " + station + " " + now_playing.video_id + " " +
                                    std::to_string(n) + "/" + std::to_string(listeners) + " by " + username);
                    } else {
                        advance = alone || (named && community_elapsed_locked(station, now) >= kCommunityMinEndedSec);
                        if (!advance) lglog::emit("jukebox", "ended-ignored " + station + " by " + username);
                    }
                    if (advance) {
                        // Bug (Brandon: "i dont see an accurate history"): nothing ever
                        // recorded a shared station's plays anywhere, so the History
                        // section's /jukebox/history?station= call always came back empty
                        // and silently fell back to the viewer's OWN personal plays --
                        // "inaccurate," not missing. Recorded here, the one place a track
                        // on this station is actually known to be done.
                        auto& hist = g_community_history_by_station[station];
                        hist.push_back(CommunityTrack{now_playing.video_id, now_playing.queued_by});
                        if (hist.size() > kCommunityHistoryCap) hist.pop_front();
                        community_advance_locked(station, now);
                        const auto& np = g_community_now_by_station[station];
                        if (np.active) g_store.set_jukebox_pos(station, np.video_id, 0, now);
                        else g_store.clear_jukebox_pos(station);
                        g_community_last_flush_by_station[station] = now;
                    }
                }
            }
        }

        std::string snapshot;
        {
            std::lock_guard<std::mutex> lock(g_community_mu);
            const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const auto& now_playing = g_community_now_by_station[station];
            const auto elapsed = community_elapsed_locked(station, now);
            // Roughly every 15s while playing, not every ~500ms tick --
            // shared across however many listeners are connected (only
            // whichever one's tick crosses the threshold actually writes).
            if (now_playing.active) {
                auto& last_flush = g_community_last_flush_by_station[station];
                if (now - last_flush >= 15) {
                    last_flush = now;
                    g_store.set_jukebox_pos(station, now_playing.video_id, elapsed, now);
                }
            }
            std::ostringstream os;
            os << "{";
            if (now_playing.active) {
                os << "\"now\":{\"video_id\":\"" << now_playing.video_id
                   << "\",\"queued_by\":\"" << now_playing.queued_by
                   << "\",\"elapsed\":" << elapsed << "}";
            } else {
                os << "\"now\":null";
            }
            os << ",\"listeners\":" << g_community_listeners_by_station[station];
            os << ",\"upcoming\":" << g_community_upcoming_by_station[station].size();
            os << ",\"queue\":[";
            {
                bool qfirst = true;
                for (const auto& t : g_community_upcoming_by_station[station]) {
                    if (!qfirst) os << ",";
                    qfirst = false;
                    os << "{\"video_id\":\"" << t.video_id << "\",\"queued_by\":\"" << t.queued_by << "\"}";
                }
            }
            os << "]}";
            snapshot = os.str();
        }
        if (!ws_write_frame(fd, WsOpcode::Text, snapshot)) break;
    }

    // Leaving is a ...->0 transition (or not, if someone else is still
    // here): only the actual last listener freezes and saves. WS close is
    // enough on its own to catch this -- no client-side pagehide beacon
    // needed for community/site-wide stations, unlike the personal
    // (HTTP-polled) ones below.
    {
        std::lock_guard<std::mutex> lock(g_community_mu);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        // Elapsed has to be computed while the count still reflects this
        // connection as live -- community_elapsed_locked only adds live
        // time when count>0, so doing this after decrementing would
        // freeze at frozen_offset alone, silently dropping this session's
        // own listening time (caught live: a 5s hold froze at 0, not 5).
        auto& np = g_community_now_by_station[station];
        const auto elapsed = community_elapsed_locked(station, now);
        int& count = g_community_listeners_by_station[station];
        if (count > 0) --count;
        if (count == 0 && np.active) {
            if (elapsed > kJukeboxResumeMaxSec) {
                np = CommunityNowPlaying{};
                g_store.clear_jukebox_pos(station);
            } else {
                np.frozen_offset = elapsed;
                g_store.set_jukebox_pos(station, np.video_id, elapsed, now);
            }
        }
    }
    close(fd);
}

