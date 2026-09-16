
(function(){
  var SKIP='liquidgov.getapp.skipUntil';
  function ls(k){try{return localStorage.getItem(k);}catch(e){return null;}}
  function set(k,v){try{localStorage.setItem(k,v);}catch(e){}}
  function ios(){return /iP(hone|od|ad)/.test(navigator.userAgent)||(navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1);}
  function android(){return /Android/i.test(navigator.userAgent);}
  function standalone(){return window.navigator.standalone===true||(window.matchMedia&&matchMedia('(display-mode: standalone)').matches);}
  function hideNags(){
    var h=document.getElementById('lgioshint'); if(h) h.style.display='none';
    var path=location.pathname||'';
    if(/^\/(apps|install|app\/)/.test(path)) return;
    var nodes=document.querySelectorAll('p,div,span,button,a,small');
    for(var i=0;i<nodes.length;i++){
      var el=nodes[i];
      if(el.id==='lggetapp'||(el.closest&&el.closest('#lggetapp'))) continue;
      if(el.children && el.children.length) continue;
      var t=(el.textContent||'').replace(/\s+/g,' ').trim();
      if(t==='Alerts on'){el.style.display='none'; continue;}
      var low=t.toLowerCase();
      if(low.indexOf('safari')>=0 && (low.indexOf('alert')>=0||low.indexOf('notif')>=0||low.indexOf('push')>=0||low.indexOf('need')>=0))
        el.style.display='none';
    }
  }
  hideNags();
  setInterval(hideNags, 1000);
  if(standalone()) return;
  if(!ios() && !android()) return;
  var path=location.pathname||'';
  if(/^\/(apps|install|app\/)/.test(path)) return;
  if((+ls(SKIP)||0)>Date.now()) return;
  var bar=document.createElement('div');
  bar.id='lggetapp';
  bar.innerHTML='<a href="/apps">Get the app!</a><button type="button" aria-label="Dismiss">\u00d7</button>';
  bar.querySelector('button').addEventListener('click', function(e){
    e.preventDefault(); set(SKIP, String(Date.now()+7*24*3600*1000)); bar.remove();
  });
  var main=document.querySelector('main')||document.body;
  main.insertBefore(bar, main.firstChild);
})();
