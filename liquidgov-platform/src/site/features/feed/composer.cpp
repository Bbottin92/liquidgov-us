#include "site/features/feed/composer.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/features/profile/profile_css.hpp"
#include "site/ui/favicon_cookies.hpp"
#include "site/core/http.hpp"
#include "site/core/multipart.hpp"

namespace liquidgov::site {

ComposerPost read_composer_post(const std::string& hdrs, const std::string& body) {
    ComposerPost r;
    std::string ctype = header_value(hdrs, "Content-Type");
    std::optional<std::vector<std::uint8_t>> file;
    if (ctype.find("multipart/form-data") != std::string::npos) {
        r.text = multipart_field(ctype, body, "text");
        file = multipart_file(ctype, body, "media");
    } else {
        r.text = parse_form(body)["text"];
    }
    if (r.text.size() > 2000) r.text.resize(2000);
    if (file && !file->empty()) {
        auto res = ingest_file(g_blobs, g_mail_scanner, g_mail_caps, *file);
        if (res.error != FileTransferError::None || !res.blob_id) {
            r.ok = false;
            switch (res.error) {
                case FileTransferError::TooLarge: r.err = "large"; break;
                case FileTransferError::ActiveContentBlocked: r.err = "active"; break;
                case FileTransferError::FailedScan: r.err = "scan"; break;
                default: r.err = "type"; break;
            }
            return r;
        }
        r.attachment_blob_id = *res.blob_id;
        r.attachment_mime = res.stored_mime_type.value_or("");
    }
    return r;
}

// Every page's "something went wrong" banner reads this one query key now
// -- collapses what used to be three (err/perr/jerr), each with its own
// scattered if/else copied into whichever GET handler needed it. That
// scattering is exactly how ?err=short (settings, new password too short)
// ended up wired on the write side but never read on the get side: the
// redirect fired, the message never rendered. One table, one place to add
// a code, nothing to forget on the read side ever again.
//
// A short code is reused across pages ONLY when the message is actually
// the same sentence (e.g. "invite", the same contact-settings conflict on
// a mail group and a community); where two contexts used the same word for
// a genuinely different limit -- the avatar's 2 MiB cap vs. a post
// attachment's 10/50 MiB cap, or a canvas frame's own cap -- they get their
// own name instead (portraitlarge / large / framelarge). That's the
// discipline this exists to enforce: a code names one sentence, never two.
std::string flash_error(const std::map<std::string,std::string>& qs) {
    auto it = qs.find("err");
    if (it == qs.end()) return "";
    const std::string& e = it->second;
    if (e=="login") return "<p class=\"warn\">Invalid username or password.</p>";
    if (e=="ratelimit") return "<p class=\"warn\">Too many attempts. Wait a moment and try again.</p>";
    if (e=="empty") return "<p class=\"warn\">Need a title and a body.</p>";
    if (e=="image") return "<p class=\"warn\">That file was not a usable image (PNG/JPEG/GIF, 2&nbsp;MiB, 1024px). SVG is not accepted.</p>";
    if (e=="portraitlarge") return "<p class=\"warn\">That file is too large. Cap is 2&nbsp;MiB and 1024 pixels on the long edge.</p>";
    if (e=="scan") return "<p class=\"warn\">That file could not be stored.</p>";
    if (e=="profile") return "<p class=\"warn\">Display name is required (up to 80 characters).</p>";
    if (e=="css") {
        auto rit = qs.find("reason");
        return "<p class=\"warn\">"+html_escape(css_reject_message(rit==qs.end()?"":rit->second))+"</p>";
    }
    if (e=="inner") return "<p class=\"warn\">Inner circle holds twelve. Take someone out first.</p>";
    if (e=="invite") return "<p class=\"warn\">Conflicts with another member’s contact settings.</p>";
    if (e=="address") return "<p class=\"warn\">Need a free address: letters, numbers, dots, underscores, hyphens.</p>";
    if (e=="subcategory") return "<p class=\"warn\">Pick or name a subcategory.</p>";
    if (e=="city") return "<p class=\"warn\">Only the person who started this group can change its city.</p>";
    if (e=="list") return "<p class=\"warn\">Join a group in this city before you list yourself here.</p>";
    if (e=="pcity") return "<p class=\"warn\">Name a city that a group already uses, or leave the city blank.</p>";
    if (e=="ctag") return "<p class=\"warn\">Join a group in this city before you add a department or council.</p>";
    if (e=="ctagname") return "<p class=\"warn\">Name a department or council.</p>";
    if (e=="password") return "<p class=\"warn\">Could not change password. Check the current one.</p>";
    if (e=="short") return "<p class=\"warn\">New password needs at least eight characters.</p>";
    if (e=="delete") return "<p class=\"warn\">Type your username to delete.</p>";
    if (e=="large") return "<p class=\"warn\">That file was too large (images 10&nbsp;MiB, video 50&nbsp;MiB).</p>";
    if (e=="active") return "<p class=\"warn\">That document contains active content and was rejected.</p>";
    if (e=="type") return "<p class=\"warn\">That file wasn't a supported image or video type.</p>";
    if (e=="nogroup") return "<p class=\"warn\">Pick at least one group, or turn Groups off.</p>";
    if (e=="options") return "<p class=\"warn\">Need 2–12 options, or 1–12 if people can add their own.</p>";
    if (e=="option") return "<p class=\"warn\">Could not add that choice. Duplicate, empty, or you already added one.</p>";
    // Brief 7 item 5 (PROPOSALS-AUDIT: err=range was redirected to but had no sentence; refusals were silent)
    if (e=="range") return "<p class=\"warn\">Minimum must be a number below the maximum. Leave both blank for any number.</p>";
    if (e=="vote") return "<p class=\"warn\">Your answer didn&rsquo;t save. It was not one of the choices, or it was outside the allowed range or not a real date. Try again.</p>";
    if (e=="dupe") return "<p class=\"warn\">Two options are the same (capital letters don&rsquo;t count). Remove one.</p>";
    if (e=="optfull") return "<p class=\"warn\">This list is full: 48 options is the most one proposal can hold.</p>";
    if (e=="url") return "<p class=\"warn\">That link wasn't recognized as a YouTube video.</p>";
    if (e=="full") return "<p class=\"warn\">Queue already holds "+std::to_string(kJukeboxQueueCap)+" tracks. Remove one first.</p>";
    if (e=="denied") return "<p class=\"warn\">You can't queue a track here.</p>";
    if (e=="pending") return "<p class=\"warn\">You already have a track queued. Wait for it to play first.</p>";
    if (e=="google") return "<p class=\"warn\">Couldn&rsquo;t verify that Google sign-in. Try again.</p>";
    if (e=="google-down") return "<p class=\"warn\">Couldn&rsquo;t reach Google to check your sign-in. Try again in a moment.</p>";
    if (e=="reset") return "<p class=\"warn\">That link has expired or was already used. Request a new one.</p>";
    return "";
}

// One checkbox per name, sharing a form field name -- used for both the
// Anybody hide-list and the Friends show-list (same friend roster, opposite
// meaning) and for the Groups show-list (community slugs).
std::string composer_checkbox_list(const std::string& field_name,
                                    const std::vector<std::pair<std::string,std::string>>& value_label_pairs,
                                    const char* empty_note) {
    if (value_label_pairs.empty())
        return "<p class=\"muted\" style=\"margin:.3rem 0 0\">"+std::string(empty_note)+"</p>";
    std::string html;
    for (const auto& vl : value_label_pairs) {
        html += "<label style=\"display:flex;align-items:center;gap:.4rem;font-weight:400;margin:.25rem 0\">"
                "<input type=\"checkbox\" name=\""+field_name+"\" value=\""+html_escape(vl.first)+"\">"+
                html_escape(vl.second)+"</label>";
    }
    return html;
}

// The /feed composer card. Three icon chips replace the old static
// "Friends only" label -- the glyph carries the reach (globe/people/rooms),
// so nobody has to read a sentence to know what "Anybody" or "Groups"
// means, and each opens its own exceptions/allow-list picker (Grok's
// model, not a Members-relabel and not "post to every group I'm in"):
//   Anybody  -- a personal post, audience=Anybody. Reaches friends in
//              /feed same as a Friends post; ALSO shows on the author's own
//              profile to any other viewer (signed in or not) EXCEPT
//              whoever is checked in its "Exceptions" hide-list. Never
//              lands in a stranger's own /feed -- that stays a friends
//              graph, not a public firehose.
//   Friends  -- audience=Friends, today's only behavior before this
//              existed and still the default. Its "Which friends" list is
//              a show-list: checked names (plus the author) see it in
//              /feed; empty means every current friend, unchanged.
//   Groups   -- one or more real community posts (Post.community + extra
//              extra_groups), same as posting from each community's own
//              page -- a destination picker, not a visibility bit. Only
//              offered when the author actually belongs to a community
//              (join-to-post unchanged), and refused if turned on with
//              nothing picked.
// Anybody is exclusive with the other two (public vs. not is a ladder);
// Friends and Groups can both be on at once -- one Post row, audience
// stays Friends, with community/extra_groups also set, per the "pau"
// LGP1 tag's own comment.
//
// There used to also be a permanent paragraph under the box spelling all
// of this out ("Friends shows in your friends' feed... This is not a civic
// act.") -- gone now. The reach explanation only exists inside each
// picker's own disclosure, read by whoever actually opens it, not by
// everyone on every visit (matches the same "don't stack four empty-state
// lines" note this composer already followed for its media errors).
std::string feed_composer_card(const std::string& user) {
    auto friends = g_store.friends_of(user);
    std::vector<std::pair<std::string,std::string>> friend_pairs;
    for (const auto& f : friends) friend_pairs.push_back({f, f});
    auto my_groups = g_store.communities_of(user);
    std::vector<std::pair<std::string,std::string>> group_pairs;
    for (const auto& c : my_groups) group_pairs.push_back({c.slug, c.name});
    const bool has_groups = !my_groups.empty();

    auto ichip = [](const char* id, const char* svg, const char* label, bool on) {
        return "<button type=\"button\" class=\"ichip"+std::string(on?" on":"")+"\" id=\""+id+"\">"
               +svg+"<span>"+label+"</span></button>";
    };
    std::string groups_chip = has_groups ? ichip("vis-groups", groups_svg(), "Groups", false) : "";
    std::string groups_picker = has_groups
        ? "<details id=\"opts-groups\" hidden><summary class=\"muted\">Which groups</summary>"
          "<p class=\"muted\" style=\"margin:.3rem 0 .5rem\">Pick one or more rooms you belong to.</p>"
          +composer_checkbox_list("group", group_pairs, "")+ "</details>"
        : "";

    return
        "<div class=\"card\"><form method=\"POST\" action=\"/feed\" enctype=\"multipart/form-data\" id=\"feedform\">"
        "<label style=\"margin:0 0 .5rem;display:block\">Write to people who know you</label>"
        "<div class=\"row\" style=\"gap:.5rem;margin:0 0 .3rem\">"
        +ichip("vis-anybody", globe_svg(), "Anybody", false)+
        ichip("vis-friends", friends_svg(), "Friends", true)+
        groups_chip+
        "</div>"
        "<details id=\"opts-anybody\" hidden><summary class=\"muted\">Exceptions</summary>"
        "<p class=\"muted\" style=\"margin:.3rem 0 .5rem\">Hide-list &mdash; checked friends won't see "
        "this, in feed or on your profile.</p>"
        +composer_checkbox_list("except", friend_pairs, "No friends yet to except -- Anybody reaches everyone.")+
        "</details>"
        "<details id=\"opts-friends\" hidden><summary class=\"muted\">Which friends</summary>"
        "<p class=\"muted\" style=\"margin:.3rem 0 .5rem\">Show-list &mdash; leave everyone unchecked to "
        "reach all your friends.</p>"
        +composer_checkbox_list("friend", friend_pairs, "No friends yet -- add some first.")+
        "</details>"
        + groups_picker +
        "<input type=\"hidden\" name=\"anybody\" id=\"anybodyfield\" value=\"0\">"
        "<input type=\"hidden\" name=\"friends\" id=\"friendsfield\" value=\"1\">"
        "<input type=\"hidden\" name=\"groups\" id=\"groupsfield\" value=\"0\">"
        "<textarea name=\"text\" maxlength=\"2000\" placeholder=\"Plain text.\" "
        "oninput=\"document.getElementById('feedchars').textContent=(2000-this.value.length)+' left'\"></textarea>"
        "<p class=\"muted\" id=\"feedchars\" style=\"margin:.2rem 0 0;text-align:right\">2000 left</p>"
        "<div class=\"row\" style=\"justify-content:space-between;align-items:center;margin-top:.4rem\">"
        "<button type=\"button\" class=\"attachbtn\" id=\"feedattachbtn\" aria-label=\"Attach a photo or video\" "
        "title=\"Attach a photo or video\">"+std::string(paperclip_svg())+"</button>"
        "<button type=\"submit\">Post</button></div>"
        "<input type=\"file\" name=\"media\" id=\"feedmedia\" accept=\"image/*,video/*\" hidden>"
        "<p class=\"muted\" id=\"feedfilename\" style=\"margin:.3rem 0 0\" hidden></p>"
        "</form>"
        "<p class=\"muted\" style=\"margin-top:.5rem\">Or <a href=\"/feed/canvas\">open the canvas</a> "
        "to draw, crop, layer, or paste one together first.</p></div>"
        "<script>(function(){"
        "var state={anybody:false,friends:true,groups:false};"
        "var chipA=document.getElementById('vis-anybody'),chipF=document.getElementById('vis-friends'),"
        "chipG=document.getElementById('vis-groups');"
        "var optsA=document.getElementById('opts-anybody'),optsF=document.getElementById('opts-friends'),"
        "optsG=document.getElementById('opts-groups');"
        "var fldA=document.getElementById('anybodyfield'),fldF=document.getElementById('friendsfield'),"
        "fldG=document.getElementById('groupsfield');"
        "function render(){"
        "if(!state.anybody&&!state.friends&&!state.groups)state.friends=true;"
        "chipA.classList.toggle('on',state.anybody);"
        "chipF.classList.toggle('on',state.friends);"
        "if(chipG)chipG.classList.toggle('on',state.groups);"
        "optsA.hidden=!state.anybody;optsF.hidden=!state.friends;"
        "if(optsG)optsG.hidden=!state.groups;"
        "fldA.value=state.anybody?'1':'0';fldF.value=state.friends?'1':'0';fldG.value=state.groups?'1':'0';"
        "}"
        "chipA.addEventListener('click',function(){"
        "state.anybody=!state.anybody;"
        "if(state.anybody){state.friends=false;state.groups=false;}"
        "render();"
        "});"
        "chipF.addEventListener('click',function(){state.friends=!state.friends;if(state.friends)state.anybody=false;render();});"
        "if(chipG)chipG.addEventListener('click',function(){state.groups=!state.groups;if(state.groups)state.anybody=false;render();});"
        "render();"
        "var attachBtn=document.getElementById('feedattachbtn'),mediaInput=document.getElementById('feedmedia'),"
        "fileNameEl=document.getElementById('feedfilename');"
        "attachBtn.addEventListener('click',function(){mediaInput.click();});"
        "mediaInput.addEventListener('change',function(){"
        "var has=mediaInput.files&&mediaInput.files.length>0;"
        "attachBtn.classList.toggle('has-file',has);"
        "fileNameEl.hidden=!has;fileNameEl.textContent=has?mediaInput.files[0].name:'';"
        "});"
        "document.getElementById('feedform').addEventListener('submit',function(e){"
        "if(state.groups&&optsG&&!optsG.querySelector('input:checked')){e.preventDefault();optsG.open=true;optsG.scrollIntoView({block:'center'});}"
        "});"
        "})();</script>";
}

// /me saves itself (Brandon 2026-11-11 4:50 PM, Brief 4 item 11; replaced the "Editing /me" bar with Publish
// revision and Discard): each form[data-draft] -- About you, Profile CSS and Play settings (Brief 6 item 4) -- is
// sent to its own, unchanged endpoint with fetch about 800 ms after the last change, so there is no page load (no
// jump to the top). A small status under the form says Saving..., Saved, or Couldn't save with a Retry button: a
// refused value (the endpoint redirects with err=) or a failed request. These forms have no Save button (Brief 6
// item 4); a submit button, if one is ever added, is hidden here. Enter or a submit saves at once. The name is kept
// (draft_publish_script) so its callers and checks still find it.
std::string draft_publish_script() {
    return
        "<script>(function(){"
        "var forms=Array.prototype.slice.call(document.querySelectorAll('form[data-draft]'));"
        "forms.forEach(function(f){"
        "var btn=f.querySelector('button[type=submit],button:not([type])');"
        "if(btn)btn.hidden=true;"
        "var box=document.createElement('p');box.className='muted';box.style.margin='.35rem 0 0';"
        "var st=document.createElement('span');st.setAttribute('data-draft-status','');"
        "st.setAttribute('role','status');st.setAttribute('aria-live','polite');"
        "var rb=document.createElement('button');rb.type='button';rb.className='ghost';rb.textContent='Retry';"
        "rb.hidden=true;rb.setAttribute('data-draft-retry','');rb.style.marginLeft='.5rem';"
        "box.appendChild(st);box.appendChild(rb);f.parentNode.insertBefore(box,f.nextSibling);"
        "function ser(){var p=[];new FormData(f).forEach(function(v,k){p.push(k+'='+(typeof v==='string'?v:'[file]'));});"
        "return p.sort().join('&');}"
        "var saved=ser(),timer=null,busy=false,again=false;"
        "function show(t,retry){st.textContent=t;rb.hidden=!retry;}"
        // Without a Save button the browser no longer points at a required field left empty (a Google-only account
        // has no display name, so /me/profile refuses every About you save): name the field in the status instead.
        "function why(){var e=f.querySelector(':invalid');if(!e)return '';"
        "var l=e.previousElementSibling,n=l&&l.tagName==='LABEL'?l.textContent.trim():e.name;"
        "return ': '+n+' \\u2014 '+e.validationMessage;}"
        "function save(){"
        "clearTimeout(timer);"
        "if(busy){again=true;return;}"
        "var now=ser();if(now===saved&&rb.hidden)return;"
        "busy=true;show('Saving…',false);"
        "fetch(f.getAttribute('action'),{method:'POST',credentials:'same-origin',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:new URLSearchParams(new FormData(f)).toString()})"
        ".then(function(r){if(!r.ok||/[?&]err=/.test(r.url||''))throw new Error('refused');saved=now;show('Saved',false);})"
        ".catch(function(){show('Couldn\\u2019t save'+why(),true);})"
        ".then(function(){busy=false;if(again){again=false;save();}});"
        "}"
        "function later(){clearTimeout(timer);timer=setTimeout(save,800);}"
        "f.addEventListener('input',later);"
        "f.addEventListener('change',later);"
        "f.addEventListener('submit',function(e){e.preventDefault();save();});"
        "rb.addEventListener('click',save);"
        "});"
        "})();</script>";
}

}  // namespace liquidgov::site
