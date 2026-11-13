#include "site/features/pages/genesis.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/ui/assets.hpp"

namespace liquidgov::site {
std::string genesis_body() {
    // A public stats strip, seen in the mobile design's landing screen --
    // real counts (account_count/all_communities), not the design's own
    // placeholder "1,284"; elections held stays honestly 0, same posture
    // as the /control voting card.
    std::string stats =
        "<div class=\"stattiles\">"
        "<div class=\"stattile\"><strong>" + std::to_string(g_store.account_count()) + "</strong><span>" +
        std::string(g_store.account_count()==1?"Account":"Accounts") + "</span></div>"
        "<div class=\"stattile\"><strong>" + std::to_string(g_store.all_communities().size()) + "</strong><span>" +
        std::string(g_store.all_communities().size()==1?"Group":"Groups") + "</span></div>"
        "<div class=\"stattile\"><strong>0</strong><span>Elections held</span></div>"
        "</div>";
    // The scrolling problems ticker and the two pull-quotes, pulled from
    // the actual authored content in the mobile design's own source (its
    // PROBLEMS array and its two quote blocks) -- not from the rendered
    // preview, which never resolved its {{ }} bindings locally. Same list
    // doubled back-to-back so the CSS loop has no visible seam.
    static const std::vector<std::string> problems = {
        "Taxpayer money funding pedophilia", "State-funded propaganda",
        "Overclassification of official information", "Corporate money buying legislation",
        "Bureaucracy that answers to no one", "Endless foreign wars",
        "Federal land locked away from use", "Agricultural mismanagement",
        "One law for them, another for you", "Qualified immunity for abuse of power",
        "Emergency powers that never expire",
    };
    std::string ticker;
    for (int rep = 0; rep < 2; ++rep)
        for (const auto& p : problems)
            ticker += "<div>" + html_escape(p) + "</div>";
    std::string solution =
        "<div class=\"row solution-row\" style=\"align-items:stretch;gap:1rem;flex-wrap:wrap\">"
        "<div style=\"flex:2 1 12rem;min-width:12rem;display:flex;flex-direction:column\">"
        "<p class=\"eyebrow\" style=\"color:var(--accent)\">A legitimate solution to:</p>"
        "<div class=\"ticker-wrap\"><div class=\"ticker-roll\">" + ticker + "</div></div>"
        "</div>"
        "<div class=\"card isnt-card\">"
        "<p class=\"eyebrow\" style=\"color:var(--accent)\">Lead Yourself &mdash; together</p>"
        "<p style=\"margin:.6rem 0\"><strong>Not a party</strong><br>"
        "<span class=\"muted\">Nothing here asks you to pick a side, and no side owns it.</span></p>"
        "<p style=\"margin:.6rem 0\"><strong>Not a government</strong><br>"
        "<span class=\"muted\">No election has been held. Nothing here binds you to anything.</span></p>"
        "<p style=\"margin:.6rem 0\"><strong>Not a protest</strong><br>"
        "<span class=\"muted\">It is a structure you can test, argue with, and walk away from.</span></p>"
        "<p style=\"margin:.6rem 0\"><strong>Not for sale</strong><br>"
        "<span class=\"muted\">Your account is not a product and it is not anyone else's leverage.</span></p>"
        "</div></div>";
    // Two full-width cards, stacked -- not side by side. Matches the
    // source exactly: only the second (anonymous) quote is centered and
    // carries stars; the first (attributed) quote is left-aligned,
    // unstarred, on a plain card, same as the design's own two different
    // treatments for a named vs. an anonymous testimonial.
    std::string quotes =
        "<p class=\"eyebrow\" style=\"color:var(--accent)\">Consider Incentives</p>"
        "<div class=\"card quote\"><p>&ldquo;These aren't &lsquo;people problems.&rsquo; They're structural "
        "problems. Good people placed inside broken structures produce broken outcomes. Broken people placed "
        "inside well-designed structures are naturally corrected by them.&rdquo;</p>"
        "<p class=\"attrib\">&mdash; Brandon Bottin</p></div>"
        "<div class=\"card quote centered\"><p class=\"stars\">&#9733;&#9733;&#9733;&#9733;&#9733;</p>"
        "<p>&ldquo;It's like Myspace had a three-way with a ballot box and a gameboy.&rdquo;</p>"
        "<p class=\"attrib muted\">&mdash; Anonymous</p></div>";
    // The decorative emblem from the top of the design's own welcome
    // screen -- a triangle (the three laws) with a droplet at its
    // center and a ringed node at each vertex, wrapped in a faint
    // circle. Pure SVG with no text content, which is why it never
    // showed up in an accessibility-tree read of the design -- found by
    // reading the .dc.html's raw markup instead.
    std::string emblem =
        "<div style=\"text-align:center\"><svg viewBox=\"0 0 180 172\" width=\"140\" height=\"134\" aria-hidden=\"true\">"
        "<circle cx=\"90\" cy=\"96\" r=\"54\" fill=\"none\" style=\"stroke:var(--rule)\" stroke-width=\"1\"/>"
        "<path d=\"M90 30 L32 130 L148 130 Z\" fill=\"none\" style=\"stroke:var(--stone)\" stroke-width=\"1\"/>"
        "<path d=\"M90 70c0 0 13 17.5 13 24.5a13 13 0 0 1-26 0C77 87.5 90 70 90 70z\" style=\"fill:var(--accent)\"/>"
        "<circle cx=\"90\" cy=\"30\" r=\"6\" style=\"fill:var(--paper);stroke:var(--accent)\" stroke-width=\"1.4\"/>"
        "<circle cx=\"32\" cy=\"130\" r=\"6\" style=\"fill:var(--paper);stroke:var(--accent)\" stroke-width=\"1.4\"/>"
        "<circle cx=\"148\" cy=\"130\" r=\"6\" style=\"fill:var(--paper);stroke:var(--accent)\" stroke-width=\"1.4\"/>"
        "</svg></div>";
    std::string lawstrip =
        "<div class=\"lawstrip\">"
        "<div><p class=\"n\">01</p><p class=\"t\">Do not steal</p></div>"
        "<div><p class=\"n\">02</p><p class=\"t\">Do not damage</p></div>"
        "<div><p class=\"n\">03</p><p class=\"t\">Do not deprive of liberty</p></div>"
        "</div>"
        "<p class=\"muted\" style=\"text-align:center;max-width:22rem;margin:.7rem auto 0\">"
        "Three rules hold. Whoever you are, wherever you're starting from, the rest is your call.</p>";
    // The Commons is closed until it is built. No walk-in on this page.
    std::string inside;
    // Real recorded narration (Helios), served from /assets/chapter1.mp3.
    // The latest file already starts on the first sentence (no spoken
    // title), so no #t= skip is needed anymore. No native <audio controls>
    // chrome -- the pulsing round button IS the control, a bare <audio>
    // with no `controls` attribute renders nothing on its own. Icon
    // markup is defined once in C++ and reused for both the server-
    // rendered initial state and the JS reset-on-ended/pause swap (via
    // js_string_literal, not hand-escaped) so the two can't drift.
    // A play triangle reads unambiguously as "tap to hear this" at 16px;
    // the earlier speaker-with-sound-waves glyph didn't (too many thin
    // strokes at this size, and a pale outline against parchment barely
    // registered as a button at all). Solid accent-filled circle + paper-
    // colored icon fixes both the contrast and the "is this tappable"
    // question in one move.
    const char* listenPlayIcon =
        "<svg viewBox=\"0 0 24 24\" width=\"16\" height=\"16\" fill=\"currentColor\" aria-hidden=\"true\">"
        "<path d=\"M6 4l14 8-14 8V4z\"/></svg>";
    const char* listenPauseIcon =
        "<svg viewBox=\"0 0 24 24\" width=\"16\" height=\"16\" fill=\"currentColor\" aria-hidden=\"true\">"
        "<rect x=\"5\" y=\"4\" width=\"5\" height=\"16\" rx=\"1\"/>"
        "<rect x=\"14\" y=\"4\" width=\"5\" height=\"16\" rx=\"1\"/></svg>";
    std::string listen_audio =
        "<button type=\"button\" class=\"listen-pulse\" aria-label=\"Listen to Chapter 1\" "
        "aria-pressed=\"false\">" + std::string(listenPlayIcon) + "</button>"
        "<audio id=\"ch1audio\" preload=\"none\" src=\"/assets/chapter1.mp3\"></audio>"
        "<script>(function(){"
        "var btn=document.querySelector('.listen-pulse');"
        "if(!btn)return;"
        "var audio=document.getElementById('ch1audio');"
        "var playIcon="+js_string_literal(listenPlayIcon)+";"
        "var pauseIcon="+js_string_literal(listenPauseIcon)+";"
        "function reset(){"
        "btn.classList.remove('playing');"
        "btn.setAttribute('aria-pressed','false');"
        "btn.innerHTML=playIcon;"
        "}"
        "audio.addEventListener('ended',reset);"
        // The intro overlay (#lgintro) removes itself from the DOM once
        // dismissed/finished -- its presence is enough to know it's still
        // up, no separate flag needed. Ignoring the tap rather than
        // queuing it: the visitor can just tap again once it's gone.
        "btn.addEventListener('click',function(){"
        "if(document.getElementById('lgintro'))return;"
        "if(audio.paused){"
        "audio.play();"
        "btn.classList.add('playing');"
        "btn.setAttribute('aria-pressed','true');"
        "btn.innerHTML=pauseIcon;"
        "}else{"
        "audio.pause();"
        "reset();"
        "}"
        "});"
        "})();</script>";
    // position:relative on the hero block + position:absolute on the
    // button (see .listen-pulse) pins it to the block's own top-right
    // corner. Scrolls away with the page like everything else here --
    // not position:fixed to the viewport, which would either need to
    // clear the sticky header by a guessed pixel amount or risk sitting
    // over the iOS install banner (also viewport-bottom-fixed) when
    // that's showing.
    return "<div style=\"position:relative\">" +
           listen_audio +
           emblem +
           "<p class=\"eyebrow\" style=\"color:var(--accent)\">A Voluntary Natural Democracy</p>"
           "<h1>This is YOUR government!</h1>"
           "<p class=\"lede\">Here you represent yourself, and accountability is universally distributed. "
           "Officials are incentivized to serve the people, not the other way around.</p>"
           "</div>"
           "<hr style=\"border:0;border-top:1px solid var(--rule);margin:1rem 0\">" +
           lawstrip +
           solution + quotes + inside +
           "<div class=\"card\"><p>This isn't a legal government entity — not a city, state, or federal "
           "authority, and it doesn't replace the one you already live under; that government still governs "
           "you exactly as it did before you found this site. What it is: a space to organize, collaborate, "
           "and build the systems a real government will need, so they're ready when the time comes to "
           "actually become one. No office seated from these pages carries legal authority over anyone who "
           "hasn't chosen to associate with it.</p>"
           "<p>Website control is temporary. When a working election exists among enrolled citizens, "
           "that control is meant to end. The internal shape of that election is itself a citizen vote. "
           "Real governmental authority is a later, explicit landslide — Phase 2 — not this page and not "
           "the first people who show up.</p>"
           "<p class=\"warn\">Until then, genesis operators run the machines and say so in public. "
           "That is a disclosure, not a claim of sovereignty.</p></div>"
           "<div class=\"clocks\">"
           "<div class=\"card\"><h2>Two clocks</h2><p><strong>Site control</strong> ends when a working election exists among enrolled citizens.</p>"
           "<p><strong>Governmental authority</strong> stays a later landslide — more than half of the eligible citizen population. However long that takes.</p></div>"
           "<div class=\"card\"><h2>This website is the door</h2><p>Feed, friends, mail, and groups live here. "
           "Voting weight, offices, land, and judgments live in a separate governance kernel. "
           "A platform account is not a citizen identity. Combined verification for civic acts is not live.</p></div>"
           "</div>" +
           stats +
           // Drop on the home page (Brandon 2026-11-13 4:03 AM): walker.js's switch, touchpad and note (tip.js), and
           // home_drop.js walking a plain drop down the page. Only logged-out visitors get this page (no Juketube,
           // so tip.js loads here).
           "<script src=\"" + asset_url("tip.js") + "\" defer></script>"
           "<script src=\"" + asset_url("walker.js") + "\" defer></script>"
           "<script src=\"" + asset_url("home_drop.js") + "\" defer></script>";
}

}  // namespace liquidgov::site
