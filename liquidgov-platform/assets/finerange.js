/* Fine-scrubbing range sliders, site wide (Brandon 2026-11-19: "the farther away up or down you slide while controlling the slider, the more fine the
   adjustment should become ... that should be true of all sliders in liquidgov"). Every input[type=range] works like a video app's scrubber: grab the
   thumb and drag sideways; the farther your finger is from the slider, up or down, the finer the change (full at the slider, half 50 px away, a tenth
   about 450 px away). Tapping the track still jumps there, and the drag then carries on from that point. The slider's own step is honoured and the
   page gets real input and change events, so existing handlers need no change. Keyboard use is the browser's own. */
(function(){
var drag=null;
function snap(r,v){var min=Number(r.min||0),max=Number(r.max||100),st=Number(r.step);if(!(st>0))st=1;v=Math.max(min,Math.min(max,v));return Math.max(min,Math.min(max,min+Math.round((v-min)/st)*st));}
function setValue(r,v){var n=snap(r,v);if(String(n)===r.value)return;r.value=String(n);r.dispatchEvent(new Event('input',{bubbles:true}));}
document.addEventListener('pointerdown',function(e){
  var r=e.target;if(!r||r.tagName!=='INPUT'||r.type!=='range'||r.disabled)return;
  e.preventDefault();try{r.setPointerCapture(e.pointerId);}catch(err){}
  var b=r.getBoundingClientRect(),w=b.width||200,min=Number(r.min||0),max=Number(r.max||100),span=(max-min)||1,thumb=min+(Number(r.value)-min);
  var at=b.left+((Number(r.value)-min)/span)*w;
  if(Math.abs(e.clientX-at)>16)setValue(r,min+((e.clientX-b.left)/w)*span);          // a tap on the track jumps there
  drag={r:r,x:e.clientX,y0:e.clientY,w:w,span:span,v:Number(r.value)};
  r.dataset.fine='1.00';
},true);
document.addEventListener('pointermove',function(e){
  if(!drag)return;
  var f=1/(1+Math.abs(e.clientY-drag.y0)/50);
  drag.v+=(e.clientX-drag.x)/drag.w*drag.span*f;drag.x=e.clientX;           // the exact value is kept, only the shown one snaps to the step
  drag.r.dataset.fine=f.toFixed(2);setValue(drag.r,drag.v);
},true);
function end(){if(!drag)return;var r=drag.r;drag=null;r.dispatchEvent(new Event('change',{bubbles:true}));}
document.addEventListener('pointerup',end,true);
// iPhone Safari (Brandon's phone check 2026-11-20: "doesnt do that"): Safari drags a range slider itself from TOUCH events, which the pointerdown
// preventDefault above does not stop, so its own drag took over and cancelled ours. Stopping the touch start on a slider leaves the pointer events
// (and this script) in charge; nothing else on the page is touched.
document.addEventListener('touchstart',function(e){var r=e.target;if(r&&r.tagName==='INPUT'&&r.type==='range'&&!r.disabled)e.preventDefault();},{capture:true,passive:false});document.addEventListener('pointercancel',end,true);
})();
