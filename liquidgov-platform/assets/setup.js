
(function(){
  var SKIP='liquidgov.getapp.skipUntil';
  function ls(k){try{return localStorage.getItem(k);}catch(e){return null;}}
  function set(k,v){try{localStorage.setItem(k,v);}catch(e){}}
  function ios(){return /iP(hone|od|ad)/.test(navigator.userAgent)||(navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1);}
  function android(){return /Android/i.test(navigator.userAgent);}
  function standalone(){return window.navigator.standalone===true||(window.matchMedia&&matchMedia('(display-mode: standalone)').matches);}
  // No more scan: this used to run hideNags() every second over every p/div/span/button/a/small, hiding prompts the
  // site made itself (and any post that happened to say "Alerts on"). Those prompts are gone at the source
  // (push.js, http_server.cpp ios_install_hint), so nothing flickers in and out (2026-11-07 iPhone sweep).
  try{ if(window.top!==window) return; }catch(e){ return; }   // a page inside the /map field asks nothing
  if(standalone()) return;
  if(!ios() && !android()) return;
  var path=location.pathname||'';
  if(/^\/(apps|install|app\/)/.test(path)) return;
  if((+ls(SKIP)||0)>Date.now()) return;
  if(document.getElementById('lgpushask')) return;   // one prompt at a time: "Turn on alerts" is already showing
  var bar=document.createElement('div');
  bar.id='lggetapp';
  // An iPhone Safari tab can't get alerts until the site is on the Home Screen, so say so (and push.js stays quiet).
  var label=ios()?'Add to Home Screen to get alerts':'Get the app!';
  bar.innerHTML='<a href="/apps">'+label+'</a><button type="button" aria-label="Dismiss">×</button>';
  bar.querySelector('button').addEventListener('click', function(e){
    e.preventDefault(); set(SKIP, String(Date.now()+7*24*3600*1000)); bar.remove();
  });
  var main=document.querySelector('main')||document.body;
  main.insertBefore(bar, main.firstChild);
})();
