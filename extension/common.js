/* Shared by the popup and the background. */
var LG_SITE = 'https://liquidgov.us';

// Sites drops never appear on, whatever you choose: banking, payments, mail, health, accounts and sign-in pages,
// government services, and anything local. The address of such a page is never hashed or sent anywhere.
var LG_SENSITIVE = [
  /(^|\.)(bank|banking|chase|wellsfargo|bankofamerica|citi|capitalone|usbank|pnc|schwab|fidelity|vanguard|paypal|venmo|stripe|coinbase|robinhood|americanexpress|discover)\./,
  /(^|\.)(mail\.google|outlook\.live|outlook\.office|mail\.yahoo|proton|icloud)\./,
  /(^|\.)(accounts|login|signin|auth|sso|id|secure|myaccount)\./,
  /(^|\.)(mychart|patient|health|kaiser|cvs|walgreens)\./,
  /\.(gov|mil)$/,
  /^(localhost|127\.|10\.|192\.168\.|172\.(1[6-9]|2\d|3[01])\.)/,
  /(^|\.)liquidgov\.us$/
];
function lgAllowedHost(host) {
  host = (host || '').toLowerCase();
  return !!host && LG_SENSITIVE.every(function (re) { return !re.test(host); });
}
function lgAllowedUrl(href) {
  try {
    var u = new URL(href);
    return u.protocol === 'https:' && lgAllowedHost(u.hostname) && !/\/(login|signin|sign-in|checkout|account|settings|password)(\/|$)/i.test(u.pathname);
  } catch (e) { return false; }
}

// The room for a page: the SHA-256 of its address without the query or #fragment (so tokens and searches never go
// into it), except a few query keys that name the thing itself (a YouTube video). Only this hash leaves the browser.
var LG_KEEP_QUERY = { 'youtube.com': ['v'], 'www.youtube.com': ['v'], 'm.youtube.com': ['v'] };
function lgPageKey(href) {
  var u = new URL(href);
  var keep = LG_KEEP_QUERY[u.hostname.toLowerCase()] || [], q = [];
  keep.forEach(function (k) { var v = u.searchParams.get(k); if (v) q.push(k + '=' + v); });
  var host = u.hostname.toLowerCase().replace(/^(www|m|mobile)\./, '');
  return 'lg-web-v1|https://' + host + u.pathname.replace(/\/+$/, '') + (q.length ? '?' + q.join('&') : '');
}
function lgRoomFor(href) {
  return crypto.subtle.digest('SHA-256', new TextEncoder().encode(lgPageKey(href))).then(function (buf) {
    return '/x/' + Array.prototype.map.call(new Uint8Array(buf), function (b) { return ('0' + b.toString(16)).slice(-2); }).join('');
  });
}
function lgOriginPattern(href) { return new URL(href).origin + '/*'; }
