/* Video links play where they are shared (Brandon 2026-11-15: "anywhere you can share a url or video would be able to
   see the video player instead of just seeing letters and numbers and symbols. whatever is realistic").
   On every page, in what people wrote (messages, posts, comments, groups): a web address in plain text becomes a link,
   and under a link to a video on a site with an official embedded player (VIDEO_SITES below) or to a video file
   (.mp4, .webm, .mov, .m4v, .ogv) a card appears: "Play video". Nothing is loaded from that site until you tap the
   card (no request, no tracking before you choose to watch); then its player takes the card's place.
   Adult sites are behind a "tap to show" cover, unless you turned that off in Settings (the lg_nsfw=on cookie,
   "Show adult videos without a cover"). Any other link stays a plain link: the site never fetches addresses people
   share. Players are built from the parsed address only (ids checked against a pattern), never from the text as HTML. */
(function () {
  if (window.LG_VIDEO_LINKS) return;
  var NSFW_OPEN = /(?:^|;\s*)lg_nsfw=on(?:;|$)/.test(document.cookie || '');
  var FILE = /\.(mp4|webm|mov|m4v|ogv)$/i;
  var ID = /^[A-Za-z0-9_-]+$/;
  function host(u) { return u.hostname.toLowerCase().replace(/^(www|m|mobile|web)\./, ''); }
  function seg(u) { return u.pathname.split('/').filter(Boolean); }
  function ok(id) { return id && ID.test(id) ? id : null; }
  // Each site: name, match(url) -> {src, ratio (width/height; 0 = fixed height), adult} or null
  var VIDEO_SITES = [
    { name: 'YouTube', match: function (u) {
      var h = host(u), s = seg(u), id = null, tall = false;
      if (h === 'youtu.be') id = s[0];
      else if (h === 'youtube.com' || h === 'music.youtube.com') {
        if (s[0] === 'watch') id = u.searchParams.get('v');
        else if (s[0] === 'shorts' || s[0] === 'live' || s[0] === 'embed') { id = s[1]; tall = s[0] === 'shorts'; }
      }
      id = ok(id);
      return id && id.length === 11 ? { src: 'https://www.youtube-nocookie.com/embed/' + id + '?autoplay=1&playsinline=1', ratio: tall ? 9 / 16 : 16 / 9 } : null;
    } },
    { name: 'Vimeo', match: function (u) {
      var s = seg(u), n = host(u) === 'vimeo.com' && /^\d+$/.test(s[0] || '') ? s[0] : null;
      return n ? { src: 'https://player.vimeo.com/video/' + n + '?autoplay=1', ratio: 16 / 9 } : null;
    } },
    { name: 'Facebook', match: function (u) {
      var h = host(u), p = u.pathname;
      var video = h === 'fb.watch' || (h === 'facebook.com' && (/\/videos\//.test(p) || /^\/reel\//.test(p) || /^\/watch\/?$/.test(p) || /^\/share\/[rv]\//.test(p)));
      return video ? { src: 'https://www.facebook.com/plugins/video.php?show_text=false&autoplay=true&href=' + encodeURIComponent(u.href),
        ratio: /^\/(reel|share\/r)\//.test(p) ? 9 / 16 : 16 / 9 } : null;
    } },
    { name: 'Instagram', match: function (u) {
      var s = seg(u), id = host(u) === 'instagram.com' && /^(reel|reels|p|tv)$/.test(s[0] || '') ? ok(s[1]) : null;
      return id ? { src: 'https://www.instagram.com/' + (s[0] === 'reels' ? 'reel' : s[0]) + '/' + id + '/embed', ratio: 9 / 16 } : null;
    } },
    { name: 'TikTok', match: function (u) {
      var s = seg(u), n = host(u) === 'tiktok.com' && s[1] === 'video' && /^\d+$/.test(s[2] || '') ? s[2] : null;
      return n ? { src: 'https://www.tiktok.com/embed/v2/' + n, ratio: 9 / 16 } : null;
    } },
    { name: 'X', match: function (u) {
      var h = host(u), s = seg(u);
      var n = (h === 'x.com' || h === 'twitter.com') && s[1] === 'status' && /^\d+$/.test(s[2] || '') ? s[2] : null;
      return n ? { src: 'https://platform.twitter.com/embed/Tweet.html?dnt=true&id=' + n, ratio: 0 } : null;
    } },
    { name: 'Twitch', match: function (u) {
      var h = host(u), s = seg(u), clip = null;
      if (h === 'clips.twitch.tv') clip = ok(s[0]);
      else if (h === 'twitch.tv' && s[1] === 'clip') clip = ok(s[2]);
      return clip ? { src: 'https://clips.twitch.tv/embed?autoplay=true&clip=' + clip + '&parent=' + encodeURIComponent(location.hostname), ratio: 16 / 9 } : null;
    } },
    { name: 'Streamable', match: function (u) {
      var id = host(u) === 'streamable.com' ? ok(seg(u)[0]) : null;
      return id ? { src: 'https://streamable.com/e/' + id + '?autoplay=1', ratio: 16 / 9 } : null;
    } },
    { name: 'Dailymotion', match: function (u) {
      var h = host(u), s = seg(u), id = null;
      if (h === 'dailymotion.com' && s[0] === 'video') id = ok(s[1]);
      else if (h === 'dai.ly') id = ok(s[0]);
      return id ? { src: 'https://www.dailymotion.com/embed/video/' + id + '?autoplay=1', ratio: 16 / 9 } : null;
    } },
    { name: 'Pornhub', adult: true, match: function (u) {
      var h = host(u), key = null;
      if (/(^|\.)pornhub\.com$/.test(h)) {
        if (u.pathname === '/view_video.php') key = ok(u.searchParams.get('viewkey'));
        else if (seg(u)[0] === 'embed') key = ok(seg(u)[1]);
      }
      return key ? { src: 'https://www.pornhub.com/embed/' + key, ratio: 16 / 9 } : null;
    } },
    { name: 'RedGIFs', adult: true, match: function (u) {
      var s = seg(u), id = host(u) === 'redgifs.com' && (s[0] === 'watch' || s[0] === 'ifr') ? ok(s[1]) : null;
      return id ? { src: 'https://www.redgifs.com/ifr/' + id.toLowerCase(), ratio: 9 / 16 } : null;
    } }
  ];

  // what a link plays, or null
  function videoOf(href) {
    var u;
    try { u = new URL(href, location.href); } catch (e) { return null; }
    if (u.protocol !== 'https:' && u.protocol !== 'http:') return null;
    for (var i = 0; i < VIDEO_SITES.length; i++) {
      var m = VIDEO_SITES[i].match(u);
      if (m) return { name: VIDEO_SITES[i].name, adult: !!VIDEO_SITES[i].adult, src: m.src, ratio: m.ratio, file: false };
    }
    if (FILE.test(u.pathname)) return { name: 'Video', adult: false, src: u.href, ratio: 16 / 9, file: true };
    return null;
  }

  // the card under a link: a button that becomes the player when tapped
  function card(v) {
    var box = document.createElement('div');
    box.className = 'lg-vcard' + (v.adult && !NSFW_OPEN ? ' lg-vcard-adult' : '');
    box.setAttribute('data-site', v.name);
    var b = document.createElement('button');
    b.type = 'button';
    b.className = 'lg-vcard-play';
    var icon = document.createElement('span');
    icon.className = 'lg-vcard-icon';
    icon.setAttribute('aria-hidden', 'true');
    icon.textContent = '▶';
    var label = document.createElement('span');
    label.className = 'lg-vcard-label';
    label.textContent = v.adult && !NSFW_OPEN ? 'Adult site (' + v.name + '). Tap to show' : 'Play video · ' + v.name;
    b.appendChild(icon);
    b.appendChild(label);
    b.addEventListener('click', function (e) { e.preventDefault(); e.stopPropagation(); play(box, v); });
    box.appendChild(b);
    return box;
  }
  function play(box, v) {
    var el;
    if (v.file) {
      el = document.createElement('video');
      el.controls = true;
      el.playsInline = true;
      el.autoplay = true;
      el.preload = 'metadata';
      el.src = v.src;
    } else {
      el = document.createElement('iframe');
      el.src = v.src;
      el.title = v.name + ' video';
      el.allow = 'autoplay; fullscreen; picture-in-picture; encrypted-media';
      el.allowFullscreen = true;
      el.referrerPolicy = 'strict-origin-when-cross-origin';
      el.loading = 'eager';
    }
    el.className = 'lg-vcard-player';
    if (v.ratio) el.style.aspectRatio = String(v.ratio);
    else el.style.height = '560px';
    box.textContent = '';
    box.classList.remove('lg-vcard-adult');
    box.classList.add('lg-vcard-on');
    box.appendChild(el);
  }

  // where people's words are: the page's main column, not the site's own controls or anything being typed
  var SKIP = 'a,button,script,style,textarea,input,select,code,pre,header,nav,footer,.lg-vcard,[contenteditable],[data-no-video],#lg-here,#lg-here-pad,#lg-drop-menu';
  var URL_IN_TEXT = /https?:\/\/[^\s<>"']+/g;
  function trimUrl(s) { return s.replace(/[.,;:!?)\]]+$/, ''); }
  // plain-text addresses become links (the same rule as the server's linkify_post_text: trailing punctuation stays text)
  function linkify(root) {
    var w = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, {
      acceptNode: function (n) {
        if (!n.nodeValue || n.nodeValue.indexOf('http') < 0) return NodeFilter.FILTER_REJECT;
        return n.parentElement && !n.parentElement.closest(SKIP) ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_REJECT;
      }
    });
    var nodes = [];
    while (w.nextNode()) nodes.push(w.currentNode);
    nodes.forEach(function (n) {
      var text = n.nodeValue, frag = document.createDocumentFragment(), last = 0, m;
      URL_IN_TEXT.lastIndex = 0;
      while ((m = URL_IN_TEXT.exec(text))) {
        var url = trimUrl(m[0]);
        if (!url) continue;
        frag.appendChild(document.createTextNode(text.slice(last, m.index)));
        var a = document.createElement('a');
        a.href = url;
        a.textContent = url;
        a.target = '_blank';
        a.rel = 'noopener noreferrer';
        frag.appendChild(a);
        last = m.index + url.length;
      }
      if (!last) return;
      frag.appendChild(document.createTextNode(text.slice(last)));
      n.parentNode.replaceChild(frag, n);
    });
  }
  // a card after each video link, once
  function cards(root) {
    Array.prototype.forEach.call(root.querySelectorAll('a[href]'), function (a) {
      if (a.hasAttribute('data-lg-video') || a.closest('header,nav,footer,.lg-vcard,[data-no-video],#lg-drop-menu')) return;
      a.setAttribute('data-lg-video', '');
      var v = videoOf(a.getAttribute('href'));
      if (!v) return;
      a.parentNode.insertBefore(card(v), a.nextSibling);
    });
  }
  function scan(root) { linkify(root); cards(root); }

  var main = document.querySelector('main') || document.body;
  scan(main);
  // things added later (a new message, more posts) get the same treatment
  if (window.MutationObserver) {
    var pending = false;
    new MutationObserver(function () {
      if (pending) return;
      pending = true;
      setTimeout(function () { pending = false; scan(main); }, 150);
    }).observe(main, { childList: true, subtree: true });
  }
  window.LG_VIDEO_LINKS = { videoOf: videoOf, sites: VIDEO_SITES.map(function (s) { return s.name; }), nsfwOpen: NSFW_OPEN };
})();
