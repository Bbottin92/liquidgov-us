/* The drawing tool on /me/canvas, /me/portrait and the post composers' canvas pages (moved out of
   canvas.cpp's inline script 2026-11-13, code moved, not rewritten). Per-page values come from
   window.LG_CANVAS = {px, frames, user, mode: 'portrait'|'post'|'frames', postTarget}, set just before this file. */
(function(){var C=window.LG_CANVAS||{};var LAYN=3,layers=[],PX=C.px;for(var li=0;li<LAYN;li++){var lc=document.getElementById('clayer'+li);layers.push({cv:lc,ctx:lc.getContext('2d'),undo:[],redo:[],opacity:1});}if(C.frames>0){(function(){var img=new Image();img.onload=function(){layers[0].ctx.drawImage(img,0,0,layers[0].cv.width,layers[0].cv.height);};img.src='/canvas-frame/'+C.user+'/'+(C.frames-1)+'';})();}document.querySelectorAll('.layeropacity').forEach(function(s){s.addEventListener('input',function(){var L=layers[Number(s.dataset.layer)];L.opacity=Number(s.value)/100;L.cv.style.opacity=L.opacity;});});var CBG={plain:'#fff',wash:"radial-gradient(circle at 30% 35%,rgba(201,168,106,.35) 0%,rgba(201,168,106,0) 65%),"+"radial-gradient(circle at 75% 70%,rgba(162,130,59,.28) 0%,rgba(162,130,59,0) 55%),"+"radial-gradient(circle at 60% 15%,rgba(110,59,162,.10) 0%,rgba(110,59,162,0) 45%),#f7f1e4",hatch:"repeating-linear-gradient(45deg,rgba(26,24,20,.16) 0,rgba(26,24,20,.16) 1px,transparent 1px,transparent 9px),#f7f1e4",ruled:"repeating-linear-gradient(to bottom,transparent 0,transparent 17px,rgba(26,24,20,.22) 17px,rgba(26,24,20,.22) 18px),#f7f1e4"};var cbgBtns=document.querySelectorAll('.cbgbtn');cbgBtns.forEach(function(b){b.addEventListener('click',function(){cbgBtns.forEach(function(o){o.dataset.on='0';o.style.background='';});b.dataset.on='1';b.style.background='var(--rule)';document.getElementById('ccanvaswrap').style.background=CBG[b.dataset.bg];});});cbgBtns.forEach(function(b){if(b.dataset.on==='1')b.style.background='var(--rule)';});var activeLayer=0;function acv(){return layers[activeLayer].cv;}function actx(){return layers[activeLayer].ctx;}var overlay=document.getElementById('coverlay'),octx=overlay.getContext('2d');function clearOverlay(){octx.clearRect(0,0,overlay.width,overlay.height);}var drawing=false,lastX=0,lastY=0,shapeStart=null;function snap(){var L=layers[activeLayer];L.undo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));if(L.undo.length>20)L.undo.shift();L.redo=[];}function pos(e){var r=overlay.getBoundingClientRect();var p=e.touches?e.touches[0]:e;return{x:(p.clientX-r.left)*overlay.width/r.width,y:(p.clientY-r.top)*overlay.height/r.height};}var curTool='pen';function toolVal(){return curTool;}function syncBucketColor(){var c=document.getElementById('ccolor').value;var p=document.getElementById('bucketpaint'),d=document.getElementById('bucketdrip');if(p)p.setAttribute('fill',c);if(d)d.setAttribute('fill',c);}document.getElementById('ccolor').addEventListener('input',syncBucketColor);syncBucketColor();var toolBtns=document.querySelectorAll('.toolbtn');function refreshToolBtns(){toolBtns.forEach(function(b){b.style.background=b.dataset.tool===curTool?'var(--rule)':'';});document.getElementById('ctextrow').style.display=curTool==='text'?'flex':'none';}toolBtns.forEach(function(b){b.addEventListener('click',function(){curTool=b.dataset.tool;refreshToolBtns();});});refreshToolBtns();document.querySelectorAll('.fmtbtn').forEach(function(b){b.addEventListener('click',function(){var on=b.dataset.on==='1';b.dataset.on=on?'0':'1';b.style.background=on?'':'var(--rule)';});});function drawSeg(ax,ay,bx,by){var b=toolVal(),color=document.getElementById('ccolor').value;var size=Number(document.getElementById('csize').value)*PX;var opacity=Number(document.getElementById('copacity').value)/100;var glow=document.getElementById('cglow').dataset.on==='1';var cx=actx();cx.save();if(glow){cx.shadowColor=color;cx.shadowBlur=size*1.4;}if(b==='eraser'){cx.globalCompositeOperation='destination-out';cx.globalAlpha=1;cx.lineCap='round';cx.lineJoin='round';cx.lineWidth=size*2;cx.beginPath();cx.moveTo(ax,ay);cx.lineTo(bx,by);cx.stroke();}else if(b==='spray'){cx.globalAlpha=opacity;cx.fillStyle=color;var n=Math.max(6,Math.floor(size));for(var i=0;i<n;i++){var ang=Math.random()*Math.PI*2,rad=Math.random()*size;cx.beginPath();cx.arc(bx+Math.cos(ang)*rad,by+Math.sin(ang)*rad,PX,0,Math.PI*2);cx.fill();}}else{cx.lineCap='round';cx.lineJoin='round';cx.strokeStyle=color;if(b==='marker'){cx.lineWidth=size*1.6;cx.globalAlpha=Math.min(opacity,.45);}else{cx.lineWidth=size;cx.globalAlpha=opacity;}cx.beginPath();cx.moveTo(ax,ay);cx.lineTo(bx,by);cx.stroke();}cx.restore();}function strokeAt(x0,y0,x1,y1){drawSeg(x0,y0,x1,y1);if(document.getElementById('csymmetry').dataset.on==='1')drawSeg(acv().width-x0,y0,acv().width-x1,y1);}function drawShape(x0,y0,x1,y1,targetCtx){var t=toolVal(),color=document.getElementById('ccolor').value;var size=Number(document.getElementById('csize').value)*PX;var opacity=Number(document.getElementById('copacity').value)/100;var fill=document.getElementById('cfill').dataset.on==='1';var glow=document.getElementById('cglow').dataset.on==='1';targetCtx.save();targetCtx.globalAlpha=opacity;targetCtx.strokeStyle=color;targetCtx.fillStyle=color;targetCtx.lineWidth=size;targetCtx.lineCap='round';targetCtx.lineJoin='round';if(glow){targetCtx.shadowColor=color;targetCtx.shadowBlur=size*1.4;}if(t==='line'){targetCtx.beginPath();targetCtx.moveTo(x0,y0);targetCtx.lineTo(x1,y1);targetCtx.stroke();}else if(t==='rect'){var rx=Math.min(x0,x1),ry=Math.min(y0,y1),rw=Math.abs(x1-x0),rh=Math.abs(y1-y0);if(fill)targetCtx.fillRect(rx,ry,rw,rh);else targetCtx.strokeRect(rx,ry,rw,rh);}else if(t==='circle'){var mx=(x0+x1)/2,my=(y0+y1)/2,rxr=Math.abs(x1-x0)/2,ryr=Math.abs(y1-y0)/2;targetCtx.beginPath();targetCtx.ellipse(mx,my,rxr,ryr,0,0,Math.PI*2);if(fill)targetCtx.fill();else targetCtx.stroke();}targetCtx.restore();}function floodFill(fx,fy){var c=acv(),cx=actx();fx=Math.floor(fx);fy=Math.floor(fy);if(fx<0||fy<0||fx>=c.width||fy>=c.height)return;var img=cx.getImageData(0,0,c.width,c.height),data=img.data,w=c.width,h=c.height;var startI=(fy*w+fx)*4;var tr=data[startI],tg=data[startI+1],tb=data[startI+2],ta=data[startI+3];var hex=document.getElementById('ccolor').value;var opacity=Number(document.getElementById('copacity').value)/100;var fr=parseInt(hex.slice(1,3),16),fg=parseInt(hex.slice(3,5),16),fb=parseInt(hex.slice(5,7),16);var fa=Math.round(opacity*255);if(tr===fr&&tg===fg&&tb===fb&&ta===fa)return;var tol=32,tol2=tol*tol*4;function matches(i){var dr=data[i]-tr,dg=data[i+1]-tg,db=data[i+2]-tb,da=data[i+3]-ta;return dr*dr+dg*dg+db*db+da*da<=tol2;}var visited=new Uint8Array(w*h);visited[fy*w+fx]=1;var stack=[startI];while(stack.length){var i=stack.pop();data[i]=fr;data[i+1]=fg;data[i+2]=fb;data[i+3]=fa;var px=(i/4)|0,x=px%w,y=(px/w)|0;var nbrs=[[x-1,y],[x+1,y],[x,y-1],[x,y+1]];for(var k=0;k<4;k++){var nx=nbrs[k][0],ny=nbrs[k][1];if(nx<0||ny<0||nx>=w||ny>=h)continue;var vi=ny*w+nx;if(visited[vi])continue;var ni=vi*4;if(matches(ni)){visited[vi]=1;stack.push(ni);}}}cx.putImageData(img,0,0);}var smudgeBuf=document.createElement('canvas');function smudgeAt(x,y){var cx=actx(),c=acv();var r=Math.max(4,Number(document.getElementById('csize').value))*PX;var sx=Math.max(0,Math.floor(lastX-r)),sy=Math.max(0,Math.floor(lastY-r));var sw=Math.min(c.width-sx,r*2),sh=Math.min(c.height-sy,r*2);if(sw<=0||sh<=0)return;smudgeBuf.width=sw;smudgeBuf.height=sh;var sctx=smudgeBuf.getContext('2d');sctx.clearRect(0,0,sw,sh);sctx.drawImage(c,sx,sy,sw,sh,0,0,sw,sh);cx.save();cx.globalAlpha=.45;cx.drawImage(smudgeBuf,0,0,sw,sh,x-r,y-r,sw,sh);cx.restore();}var selRect=null,clip=null,pastePending=false;function rectFrom(a,b){return{x:Math.min(a.x,b.x),y:Math.min(a.y,b.y),w:Math.abs(b.x-a.x),h:Math.abs(b.y-a.y)};}function drawMarquee(r){octx.save();octx.strokeStyle='#000';octx.lineWidth=PX;octx.setLineDash([5*PX,4*PX]);octx.strokeRect(Math.round(r.x)+.5,Math.round(r.y)+.5,Math.round(r.w),Math.round(r.h));octx.restore();}function copySel(){if(!selRect||selRect.w<1||selRect.h<1)return;var c=document.createElement('canvas');c.width=Math.round(selRect.w);c.height=Math.round(selRect.h);c.getContext('2d').drawImage(acv(),selRect.x,selRect.y,c.width,c.height,0,0,c.width,c.height);clip=c;}document.getElementById('ccopy').addEventListener('click',copySel);document.getElementById('ccut').addEventListener('click',function(){if(!selRect||selRect.w<1||selRect.h<1)return;copySel();snap();actx().clearRect(selRect.x,selRect.y,selRect.w,selRect.h);captureFrame(false);});document.getElementById('cpaste').addEventListener('click',function(){if(clip)pastePending=true;});function placeClipboardAt(x,y){if(!clip)return;snap();actx().drawImage(clip,x-clip.width/2,y-clip.height/2);captureFrame(false);}function resizeAllLayers(nw,nh,sampleRect){layers.forEach(function(L){var tmp=document.createElement('canvas');if(sampleRect){tmp.width=nw;tmp.height=nh;tmp.getContext('2d').drawImage(L.cv,sampleRect.x,sampleRect.y,nw,nh,0,0,nw,nh);}else{tmp.width=L.cv.width;tmp.height=L.cv.height;tmp.getContext('2d').drawImage(L.cv,0,0);}L.cv.width=nw;L.cv.height=nh;if(sampleRect)L.ctx.drawImage(tmp,0,0);else L.ctx.drawImage(tmp,0,0,tmp.width,tmp.height,0,0,nw,nh);L.undo=[];L.redo=[];});overlay.width=nw;overlay.height=nh;var wrap=document.getElementById('ccanvaswrap');wrap.style.maxWidth=(nw/PX)+'px';wrap.style.aspectRatio=nw+'/'+nh;selRect=null;clearOverlay();captureFrame(false);}document.getElementById('ccrop').addEventListener('click',function(){if(!selRect||selRect.w<2||selRect.h<2)return;resizeAllLayers(Math.round(selRect.w),Math.round(selRect.h),{x:Math.round(selRect.x),y:Math.round(selRect.y)});});(function(){var handle=document.getElementById('cresizehandle'),wrap=document.getElementById('ccanvaswrap');var dragging=false,startX=0,startY=0,startCw=0,startCh=0,cssToCanvas=1;handle.addEventListener('pointerdown',function(e){dragging=true;startX=e.clientX;startY=e.clientY;var r=wrap.getBoundingClientRect();startCw=r.width;startCh=r.height;cssToCanvas=acv().width/r.width;handle.setPointerCapture(e.pointerId);e.preventDefault();e.stopPropagation();});handle.addEventListener('pointermove',function(e){if(!dragging)return;var nw=Math.max(40,startCw+(e.clientX-startX));var nh=Math.max(40,startCh+(e.clientY-startY));wrap.style.maxWidth=nw+'px';wrap.style.aspectRatio=nw+'/'+nh;e.preventDefault();e.stopPropagation();});handle.addEventListener('pointerup',function(e){if(!dragging)return;dragging=false;var nw=Math.max(16,Math.min(2000,Math.round((startCw+(e.clientX-startX))*cssToCanvas)));var nh=Math.max(16,Math.min(2000,Math.round((startCh+(e.clientY-startY))*cssToCanvas)));resizeAllLayers(nw,nh,null);e.preventDefault();e.stopPropagation();});})();document.querySelectorAll('.wheel').forEach(function(w){var target=document.getElementById(w.dataset.target),step=Number(w.dataset.step),rotation=0;function apply(dir){var min=Number(target.min)||0,max=Number(target.max)||9999;target.value=Math.max(min,Math.min(max,Number(target.value)+dir*step));rotation+=dir*20;w.style.transform='rotate('+rotation+'deg)';}w.addEventListener('wheel',function(e){e.preventDefault();apply(e.deltaY<0?1:-1);},{passive:false});var dragging=false,lastY=0;w.addEventListener('pointerdown',function(e){dragging=true;lastY=e.clientY;w.setPointerCapture(e.pointerId);e.preventDefault();});w.addEventListener('pointermove',function(e){if(!dragging)return;var dy=lastY-e.clientY;if(Math.abs(dy)>=6){apply(dy>0?1:-1);lastY=e.clientY;}});w.addEventListener('pointerup',function(){dragging=false;});});[['csizer','csize'],['copacityr','copacity']].forEach(function(p){var r=document.getElementById(p[0]),n=document.getElementById(p[1]);if(!r||!n)return;r.addEventListener('input',function(){n.value=r.value;});n.addEventListener('input',function(){r.value=n.value;});});function addTextBox(x,y,boxW,fontPx){var r=overlay.getBoundingClientRect(),scale=r.width/overlay.width;var ta=document.createElement('textarea');ta.placeholder='Text';var bold=document.getElementById('cbold').dataset.on==='1';var italic=document.getElementById('citalic').dataset.on==='1';var underline=document.getElementById('cunderline').dataset.on==='1';var fam=document.getElementById('cfont').value;var color=document.getElementById('ccolor').value;ta.style.cssText='position:absolute;z-index:10;width:auto;min-height:0;resize:both;overflow:auto;'+'padding:2px;box-sizing:border-box;background:rgba(255,255,255,.85);'+'border:1px dashed '+color+';color:'+color+';font-family:'+fam+';'+'font-size:'+(fontPx*scale)+'px;'+(bold?'font-weight:bold;':'')+(italic?'font-style:italic;':'')+(underline?'text-decoration:underline;':'');ta.style.left=(x*scale)+'px';ta.style.top=(y*scale)+'px';ta.style.width=(boxW*scale)+'px';ta.style.height=Math.max(fontPx*1.6,40*PX)*scale+'px';document.getElementById('ccanvaswrap').appendChild(ta);ta.focus();var done=false;function commit(){if(done)return;done=true;var val=ta.value;ta.remove();if(!val)return;snap();var cx=actx();cx.save();cx.font=(italic?'italic ':'')+(bold?'bold ':'')+fontPx+'px '+fam;cx.textBaseline='top';var lineH=fontPx*1.25,lines=[];val.split('\n').forEach(function(paragraph){var words=paragraph.split(' '),cur='';words.forEach(function(w){var test=cur?cur+' '+w:w;if(cur&&cx.measureText(test).width>boxW){lines.push(cur);cur=w;}else{cur=test;}});lines.push(cur);});if(document.getElementById('ctextbg').dataset.on==='1'){cx.fillStyle=document.getElementById('ctextbgcolor').value;cx.fillRect(x,y,boxW,lines.length*lineH);}cx.fillStyle=color;lines.forEach(function(line,i){var ly=y+i*lineH;cx.fillText(line,x,ly);if(underline){var w=cx.measureText(line).width;cx.fillRect(x,ly+fontPx*.92,w,Math.max(1,fontPx*.06));}});cx.restore();captureFrame(false);}ta.addEventListener('keydown',function(e){if(e.key==='Escape'){done=true;ta.remove();}});ta.addEventListener('blur',commit);}document.addEventListener('paste',function(e){if(C.mode==='portrait')return;var items=(e.clipboardData||{}).items||[];for(var i=0;i<items.length;i++){if(items[i].type.indexOf('image')!==0)continue;var file=items[i].getAsFile();if(!file)continue;var url=URL.createObjectURL(file),img=new Image();img.onload=function(){snap();var cvw=acv().width,cvh=acv().height;var scale=Math.min(cvw/img.width,cvh/img.height,1);var dw=img.width*scale,dh=img.height*scale;actx().drawImage(img,(cvw-dw)/2,(cvh-dh)/2,dw,dh);captureFrame(false);URL.revokeObjectURL(url);};img.src=url;e.preventDefault();break;}});overlay.addEventListener('pointerdown',function(e){var t=toolVal(),p=pos(e);if(pastePending){pastePending=false;placeClipboardAt(p.x,p.y);e.preventDefault();return;}if(t==='text'||t==='select'){shapeStart=p;e.preventDefault();return;}if(t==='bucket'){snap();floodFill(p.x,p.y);captureFrame(false);e.preventDefault();return;}if(t==='line'||t==='rect'||t==='circle'){snap();shapeStart=p;e.preventDefault();return;}snap();drawing=true;lastX=p.x;lastY=p.y;if(t!=='smudge')strokeAt(p.x,p.y,p.x+.1,p.y+.1);e.preventDefault();});overlay.addEventListener('pointermove',function(e){var t=toolVal(),p=pos(e);if(shapeStart&&(t==='select'||t==='text')){clearOverlay();drawMarquee(rectFrom(shapeStart,p));e.preventDefault();return;}if(shapeStart){clearOverlay();drawShape(shapeStart.x,shapeStart.y,p.x,p.y,octx);e.preventDefault();return;}if(!drawing)return;if(t==='smudge')smudgeAt(p.x,p.y);else strokeAt(lastX,lastY,p.x,p.y);lastX=p.x;lastY=p.y;e.preventDefault();});window.addEventListener('pointerup',function(e){if(shapeStart&&toolVal()==='select'){var p=pos(e);var r=rectFrom(shapeStart,p);clearOverlay();if(r.w>=2&&r.h>=2){selRect=r;drawMarquee(r);}shapeStart=null;return;}if(shapeStart&&toolVal()==='text'){var p=pos(e);var r=rectFrom(shapeStart,p);var start=shapeStart;clearOverlay();shapeStart=null;var fontPx=r.h>=6*PX?Math.round(r.h):24*PX;var boxW=r.w>=20*PX?Math.round(r.w):200*PX;var tx=r.w>=6*PX?r.x:start.x,ty=r.h>=6*PX?r.y:start.y;addTextBox(tx,ty,boxW,fontPx);return;}if(shapeStart){var p=pos(e);clearOverlay();drawShape(shapeStart.x,shapeStart.y,p.x,p.y,actx());shapeStart=null;captureFrame(false);return;}if(!drawing)return;drawing=false;captureFrame(false);});var layerBtns=document.querySelectorAll('.layerbtn');function refreshLayerBtns(){layerBtns.forEach(function(b){b.style.background=Number(b.dataset.layer)===activeLayer?'var(--rule)':'';});}layerBtns.forEach(function(b){b.addEventListener('click',function(){activeLayer=Number(b.dataset.layer);refreshLayerBtns();});});refreshLayerBtns();if(C.mode!=='portrait'){document.getElementById('cimport').addEventListener('change',function(e){var input=e.target,f=input.files[0];if(!f)return;var status=document.getElementById('cstatus');
/* The picker is emptied only when the file has been read (or has failed), never right after the click: iPhone Safari can
   drop the chosen file once the input is cleared, and then the read fails with nothing shown (Brandon 2026-11-16: the
   upload "does not seem to work"; his canvas visits in the live log are iPhone Safari 18.5). */
function release(){try{input.value='';}catch(err){}}
function place(imgLike,iw,ih){if(!iw||!ih){status.textContent='Could not read that file.';release();return;}snap();var cvw=acv().width,cvh=acv().height;var scale=Math.min(cvw/iw,cvh/ih);var dw=iw*scale,dh=ih*scale,dx=(cvw-dw)/2,dy=(cvh-dh)/2;actx().drawImage(imgLike,dx,dy,dw,dh);captureFrame(false);status.textContent='';release();}
if(f.type.indexOf('video')===0){status.textContent='Reading video frame\u2026';var url=URL.createObjectURL(f);var vid=document.createElement('video');vid.muted=true;vid.playsInline=true;vid.preload='auto';
/* iPhone Safari may never send loadeddata for a video that is not on the page; ask again on every sign of life, seek once,
   and give up with a message after 10 seconds instead of staying on "Reading video frame" for ever. */
var done=false,seeking=false,timer=null;
function cleanup(){clearTimeout(timer);URL.revokeObjectURL(url);}
function finish(){if(done)return;done=true;cleanup();place(vid,vid.videoWidth,vid.videoHeight);}
function fail(msg){if(done)return;done=true;cleanup();status.textContent=msg;release();}
function seekOnce(){if(seeking)return;seeking=true;try{vid.currentTime=Math.min(.1,vid.duration||0);}catch(err){}setTimeout(function(){if(!done&&vid.readyState>=2&&vid.videoWidth)finish();},1200);}
['loadedmetadata','loadeddata','canplay'].forEach(function(n){vid.addEventListener(n,seekOnce);});
vid.addEventListener('seeked',function(){if(vid.readyState>=2&&vid.videoWidth)finish();});
vid.addEventListener('error',function(){fail('Could not read that video.');});
timer=setTimeout(function(){fail('This video would not open here. Try a picture, or a shorter video.');},10000);
vid.src=url;}
else{status.textContent='Loading image\u2026';var reader=new FileReader();reader.onload=function(ev){var img=new Image();img.onload=function(){place(img,img.naturalWidth,img.naturalHeight);};img.onerror=function(){status.textContent='That file is not a usable image.';release();};img.src=ev.target.result;};reader.onerror=function(){status.textContent='Could not read that picture. Try choosing it again.';release();};reader.readAsDataURL(f);}});}function flatten(){var out=document.createElement('canvas');out.width=layers[0].cv.width;out.height=layers[0].cv.height;var octx2=out.getContext('2d');octx2.fillStyle='#fff';octx2.fillRect(0,0,out.width,out.height);for(var i=0;i<layers.length;i++){octx2.globalAlpha=layers[i].opacity;octx2.drawImage(layers[i].cv,0,0);}octx2.globalAlpha=1;return out;}document.getElementById('cundo').addEventListener('click',function(){var L=layers[activeLayer];if(!L.undo.length)return;L.redo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));L.ctx.putImageData(L.undo.pop(),0,0);});document.getElementById('credo').addEventListener('click',function(){var L=layers[activeLayer];if(!L.redo.length)return;L.undo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));L.ctx.putImageData(L.redo.pop(),0,0);});document.getElementById('cclear').addEventListener('click',function(){snap();var L=layers[activeLayer];L.ctx.clearRect(0,0,L.cv.width,L.cv.height);});/* Earlier portraits (portrait page): a click loads that picture into the canvas, replacing the drawing; each layer keeps its undo. */
document.querySelectorAll('.oldportrait').forEach(function(b){b.addEventListener('click',function(){var status=document.getElementById('cstatus');status.textContent='Loading…';var img=new Image();img.onload=function(){layers.forEach(function(L){L.undo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));if(L.undo.length>20)L.undo.shift();L.redo=[];L.ctx.clearRect(0,0,L.cv.width,L.cv.height);});activeLayer=0;refreshLayerBtns();var cw=layers[0].cv.width,ch=layers[0].cv.height,s=Math.min(cw/img.naturalWidth,ch/img.naturalHeight);var dw=img.naturalWidth*s,dh=img.naturalHeight*s;layers[0].ctx.drawImage(img,(cw-dw)/2,(ch-dh)/2,dw,dh);status.textContent='Loaded. Change it if you like, then Save as self-portrait.';};img.onerror=function(){status.textContent='Could not load that portrait.';};img.src=b.dataset.src;});});
if(C.mode==='portrait'){function captureFrame(force,done){if(done)done();}document.getElementById('csave').addEventListener('click',function(){var status=document.getElementById('cstatus');status.textContent='Saving\u2026';flatten().toBlob(function(blob){var fd=new FormData();fd.append('photo',blob,'portrait.png');fetch('/me/avatar',{method:'POST',body:fd}).then(function(r){window.location=r.url||'/me';}).catch(function(){status.textContent='Could not save. Try again.';});},'image/png');});}else if(C.mode==='post'){function captureFrame(force,done){if(done)done();}document.getElementById('csave').addEventListener('click',function(){var status=document.getElementById('cstatus');status.textContent='Posting\u2026';flatten().toBlob(function(blob){var fd=new FormData();fd.append('text',document.getElementById('cpcaption').value);fd.append('media',blob,'post.png');fetch(C.postTarget,{method:'POST',body:fd}).then(function(r){window.location=r.url||C.postTarget;}).catch(function(){status.textContent='Could not post. Try again.';});},'image/png');});}else{var clearedOnce=false,frameCount=C.frames;function updateCounter(){document.getElementById('cframecount').textContent=frameCount+'/24 frames captured';}function captureFrame(force,done){if(frameCount>=23&&!force){if(done)done();return;}flatten().toBlob(function(blob){var fd=new FormData();fd.append('frame',blob,'frame.png');function go(){fetch('/me/canvas/frame',{method:'POST',body:fd}).then(function(r){if(r.ok){frameCount++;updateCounter();}if(done)done();}).catch(function(){if(done)done();});}if(!clearedOnce){clearedOnce=true;fetch('/me/canvas/clear',{method:'POST'}).then(go).catch(go);}else{go();}},'image/png');}document.getElementById('csave').addEventListener('click',function(){var status=document.getElementById('cstatus');status.textContent='Saving\u2026';var timelapse=document.getElementById('ctimelapse').checked?'1':'0';captureFrame(true,function(){fetch('/me/canvas/mode',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'timelapse='+timelapse}).then(function(){window.location='/u/'+C.user;});});});}
/* Fullscreen editor on phones (Brandon 2026-11-19, q236: the canvas fills the screen, the buttons float on top like Sketchbook, pinch zoom and
   two-finger pan, no scrolling away and no Back by accident). Phone widths only; the tool page keeps its markup, ids and handlers: this block moves
   the existing groups into a floating top bar, a bottom dock and a sheet, and adds the view (zoom and pan) and the gestures. */
(function studio(){
if(!window.matchMedia||!window.matchMedia('(max-width: 820px)').matches)return;
var card=document.querySelector('.canvascard'),frame=document.querySelector('.canvas-frame'),row=document.querySelector('.ccanvasrow'),wrap=document.getElementById('ccanvaswrap');
if(!card||!frame||!row||!wrap)return;
var de=document.documentElement;de.classList.add('cfs');
function el(tag,cls,html){var e=document.createElement(tag);if(cls)e.className=cls;if(html)e.innerHTML=html;return e;}
var SV='<svg viewBox="0 0 24 24" width="22" height="22" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">';
function btn(id,label,path,cls){var b=el('button','cfs-btn'+(cls?' '+cls:''),SV+path+'</svg>');b.type='button';b.id=id;b.setAttribute('aria-label',label);b.title=label;return b;}
function click(id){var t=document.getElementById(id);if(t)t.click();}
/* the pieces that already exist */
var tools=frame.querySelector('.ctools'),props=frame.querySelector('.cprops'),layersG=frame.querySelector('.clayers'),edit=frame.querySelector('.cedit'),color=document.getElementById('ccolor');
/* top bar */
var top=el('div','cfs-top');
var bClose=btn('cfs-close','Close','<path d="M6 6l12 12M18 6 6 18"/>'),bUndo=btn('cfs-undo','Undo','<path d="M9 14 4 9l5-5"/><path d="M4 9h10a6 6 0 0 1 0 12h-3"/>'),
    bRedo=btn('cfs-redo','Redo','<path d="m15 14 5-5-5-5"/><path d="M20 9H10a6 6 0 0 0 0 12h3"/>'),bDone=btn('cfs-done','Done','<path d="M5 12.5 10 17.5 19 7"/>','go');
top.appendChild(bClose);var mid=el('span','cfs-mid');mid.appendChild(bUndo);mid.appendChild(bRedo);top.appendChild(mid);top.appendChild(bDone);
/* dock: the tools, the colour, and three buttons that open the sheet */
var dock=el('div','cfs-dock');
if(tools)dock.appendChild(tools);
if(color)dock.appendChild(color);
var bProps=btn('cfs-props','Size and opacity','<path d="M4 7h10M18 7h2M4 17h2M10 17h10"/><circle cx="16" cy="7" r="2"/><circle cx="8" cy="17" r="2"/>'),
    bLayers=btn('cfs-layers','Layers','<path d="M12 3 21 8l-9 5-9-5 9-5Z"/><path d="m3 12 9 5 9-5"/><path d="m3 16 9 5 9-5"/>'),
    bMore=btn('cfs-more','More','<circle cx="5" cy="12" r="1.3"/><circle cx="12" cy="12" r="1.3"/><circle cx="19" cy="12" r="1.3"/>');
dock.appendChild(bProps);dock.appendChild(bLayers);dock.appendChild(bMore);
/* sheet: size/opacity/effects, layers, everything else the page had */
var sheet=el('div','cfs-sheet'),pp=el('div','cfs-pp'),pl=el('div','cfs-pl'),pm=el('div','cfs-pm');
if(props)pp.appendChild(props);if(layersG)pl.appendChild(layersG);if(edit)pm.appendChild(edit);
var toast=el('div','cfs-toast');
Array.prototype.slice.call(card.children).forEach(function(n){
  if(n===frame)return;
  if(n.id==='cstatus'||n.querySelector&&n.querySelector('#cstatus')){toast.appendChild(n);return;}
  if(n.tagName==='H1'||(n.tagName==='P'&&n.classList.contains('muted')&&!n.id)){n.classList.add('cfs-hide');return;}
  pm.appendChild(n);
});
sheet.appendChild(pp);sheet.appendChild(pl);sheet.appendChild(pm);
card.appendChild(top);card.appendChild(sheet);card.appendChild(dock);card.appendChild(toast);
function openSheet(name){sheet.setAttribute('data-open',sheet.getAttribute('data-open')===name?'':name);[bProps,bLayers,bMore].forEach(function(b){b.classList.toggle('on',sheet.getAttribute('data-open')===(b===bProps?'props':b===bLayers?'layers':'more'));});}
bProps.addEventListener('click',function(){openSheet('props');});bLayers.addEventListener('click',function(){openSheet('layers');});bMore.addEventListener('click',function(){openSheet('more');});
bUndo.addEventListener('click',function(){click('cundo');});bRedo.addEventListener('click',function(){click('credo');});bDone.addEventListener('click',function(){click('csave');});
var st=document.getElementById('cstatus');
if(st&&window.MutationObserver){var showToast=function(){toast.classList.toggle('on',!!(st.textContent||'').trim());};new MutationObserver(showToast).observe(st,{childList:true,characterData:true,subtree:true});showToast();}
/* the view: the canvas is laid out to fit the free area, then zoomed and panned by a transform; pointer maths already reads the
   transformed rectangle, so drawing stays under the finger at any zoom */
var s=1,tx=0,ty=0,fx0=0,fy0=0,ff=1,PADT=66,PADB=86,MINS=.5,MAXS=12;
function apply(){wrap.style.transform='translate('+tx+'px,'+ty+'px) scale('+s+')';}
function fit(){var cv=layers[0].cv,vw=row.clientWidth||window.innerWidth,vh=row.clientHeight||window.innerHeight;
  var f=Math.min((vw-16)/cv.width,(vh-PADT-PADB)/cv.height);if(!(f>0))return;
  wrap.style.maxWidth='none';wrap.style.aspectRatio='auto';wrap.style.width=Math.round(cv.width*f)+'px';wrap.style.height=Math.round(cv.height*f)+'px';
  s=1;tx=(vw-cv.width*f)/2;ty=PADT+((vh-PADT-PADB)-cv.height*f)/2;fx0=tx;fy0=ty;ff=f;apply();}
fit();window.addEventListener('resize',fit);window.addEventListener('orientationchange',fit);
if(window.MutationObserver)new MutationObserver(fit).observe(layers[0].cv,{attributes:true,attributeFilter:['width','height']});
/* gestures: one finger draws (the existing handlers); a second finger cancels that stroke and turns the pair into zoom and pan */
var ptrs={},g=null;
function origin(){var r=row.getBoundingClientRect();return{x:r.left,y:r.top};}
function pair(){var ids=Object.keys(ptrs);return ids.length>=2?[ptrs[ids[0]],ptrs[ids[1]]]:null;}
function dist(a,b){return Math.hypot(a.x-b.x,a.y-b.y);}
row.addEventListener('pointerdown',function(e){
  if(e.pointerType==='mouse')return;
  ptrs[e.pointerId]={x:e.clientX,y:e.clientY};
  var p=pair();
  if(p&&!g){
    if(drawing){drawing=false;var L=layers[activeLayer];if(L.undo.length)L.ctx.putImageData(L.undo.pop(),0,0);}
    shapeStart=null;curStroke=null;clearOverlay();var o=origin();
    g={d0:dist(p[0],p[1])||1,s0:s,wx:((p[0].x+p[1].x)/2-o.x-tx)/s,wy:((p[0].y+p[1].y)/2-o.y-ty)/s};
    e.stopPropagation();e.preventDefault();
  }else if(g){e.stopPropagation();e.preventDefault();}
},true);
row.addEventListener('pointermove',function(e){
  if(e.pointerType==='mouse')return;
  if(ptrs[e.pointerId]){ptrs[e.pointerId]={x:e.clientX,y:e.clientY};}
  if(!g)return;
  var p=pair();if(p){var o=origin(),mx=(p[0].x+p[1].x)/2-o.x,my=(p[0].y+p[1].y)/2-o.y;
    s=Math.max(MINS,Math.min(MAXS,g.s0*dist(p[0],p[1])/g.d0));tx=mx-g.wx*s;ty=my-g.wy*s;apply();}
  e.stopPropagation();e.preventDefault();
},true);
function lift(e){
  if(e.pointerType==='mouse')return;
  delete ptrs[e.pointerId];
  if(g){e.stopPropagation();if(Object.keys(ptrs).length<2)g=null;}
}
row.addEventListener('pointerup',lift,true);row.addEventListener('pointercancel',lift,true);
/* Dive (Brandon 2026-11-19, q231): with Dive on, a stroke that is down keeps the picture zooming IN around the pen tip, like an infinite fractal
   zoom (inward only). The zoom is a view transform, baked into the layers at 2x (the visible window is cropped and scaled up, the view goes back
   to "fit", undo is cleared), so it can go on for ever. The brush is scaled down as the view zooms so ink keeps its size on screen. */
var dive={on:false,speed:30},penX=0,penY=0,diving=false,diveLast=0,total=1,bakes=0;
try{var sv=JSON.parse(localStorage.getItem('lg-dive'));if(sv&&typeof sv==='object'){dive.on=!!sv.on;dive.speed=Math.max(1,Math.min(100,parseInt(sv.speed,10)||30));}}catch(err){}
function saveDive(){try{localStorage.setItem('lg-dive',JSON.stringify({on:dive.on,speed:dive.speed}));}catch(err){}}
var dRow=el('div','cdive-row'),dBtn=el('button','ghost cdive-btn','Dive'),dSlider=el('input','cslider');
dBtn.type='button';dBtn.id='cdive';dBtn.setAttribute('aria-pressed','false');
dSlider.type='range';dSlider.id='cdivespeed';dSlider.min='1';dSlider.max='100';dSlider.value=String(dive.speed);dSlider.setAttribute('aria-label','Dive speed');
function showDive(){dBtn.classList.toggle('on',dive.on);dBtn.setAttribute('aria-pressed',dive.on?'true':'false');}
showDive();
dBtn.addEventListener('click',function(){dive.on=!dive.on;showDive();saveDive();});
dSlider.addEventListener('input',function(){dive.speed=parseInt(dSlider.value,10)||30;saveDive();});
dRow.appendChild(dBtn);dRow.appendChild(dSlider);(props||pp).appendChild(dRow);
function penCanvas(){var r=overlay.getBoundingClientRect();return{x:(penX-r.left)*overlay.width/r.width,y:(penY-r.top)*overlay.height/r.height};}
function bake(){
  var W=layers[0].cv.width,H=layers[0].cv.height,sx=(fx0-tx)/(s*ff),sy=(fy0-ty)/(s*ff),sw=W/s,sh=H/s,tmp=document.createElement('canvas');
  tmp.width=W;tmp.height=H;var t=tmp.getContext('2d');t.imageSmoothingEnabled=true;t.imageSmoothingQuality='high';
  layers.forEach(function(L){t.clearRect(0,0,W,H);t.drawImage(L.cv,sx,sy,sw,sh,0,0,W,H);L.ctx.clearRect(0,0,W,H);L.ctx.drawImage(tmp,0,0);L.undo=[];L.redo=[];});
  s=1;tx=fx0;ty=fy0;apply();bakes++;wrap.dataset.bakes=String(bakes);
}
function diveTick(){
  if(!(dive.on&&drawing&&!g)){diving=false;PX=C.px;return;}
  var now=performance.now(),dt=diveLast?Math.min(.05,(now-diveLast)/1000):.016;diveLast=now;
  var k=Math.exp(dive.speed/100*.6*dt),o=origin(),px=penX-o.x,py=penY-o.y;
  s*=k;tx=px-(px-tx)*k;ty=py-(py-ty)*k;total*=k;apply();wrap.dataset.dive=total.toFixed(3);PX=C.px/s;
  var p=penCanvas();if(toolVal()!=='smudge'&&(p.x!==lastX||p.y!==lastY)){strokeAt(lastX,lastY,p.x,p.y);lastX=p.x;lastY=p.y;}
  if(s>=2){bake();PX=C.px;var q=penCanvas();lastX=q.x;lastY=q.y;}
  setTimeout(diveTick,16);
}
row.addEventListener('pointerdown',function(e){penX=e.clientX;penY=e.clientY;if(dive.on&&drawing&&!g&&!diving){diving=true;diveLast=0;setTimeout(diveTick,16);}});
row.addEventListener('pointermove',function(e){penX=e.clientX;penY=e.clientY;});
/* Timelapse (Brandon 2026-11-19, q239, slice 2a): every action is logged with its timing (strokes with their points, shapes, fills, clears,
   undo/redo, the dive's bakes) in localStorage 'lg-log', and "Watch" draws the surviving actions again. The log starts fresh when the chosen
   period (hour, day, week, month) has passed; the old one is kept once as 'lg-log-prev'. Text, pasted pictures and smudges are not logged yet. */
var LOGKEY='lg-log',LOGPREV='lg-log-prev',LOGRESET='lg-log-reset',LOGMAX=1500000,curStroke=null,logFull=false,saveT=0;
function lsGet(k){try{return JSON.parse(localStorage.getItem(k));}catch(err){return null;}}
function lsPut(k,v){try{localStorage.setItem(k,typeof v==='string'?v:JSON.stringify(v));return true;}catch(err){return false;}}
function periodMs(p){return p==='hour'?3600e3:p==='day'?864e5:p==='week'?6048e5:p==='month'?2592e6:0;}
var period=(function(){try{return localStorage.getItem(LOGRESET)||'never';}catch(err){return 'never';}})();
var tlog=lsGet(LOGKEY);
if(!tlog||!tlog.events){tlog={v:1,start:Date.now(),events:[]};}
else if(periodMs(period)&&Date.now()-tlog.start>=periodMs(period)){if(tlog.events.length)lsPut(LOGPREV,tlog);tlog={v:1,start:Date.now(),events:[]};}
function saveLog(){clearTimeout(saveT);saveT=setTimeout(function(){var j=JSON.stringify(tlog);if(j.length>LOGMAX){logFull=true;return;}lsPut(LOGKEY,j);},200);}
saveLog();
function T(){return Math.round(Date.now()-tlog.start);}
function rec(e){if(logFull)return;e.t=T();tlog.events.push(e);saveLog();}
function sty(){var d=function(id){return document.getElementById(id);};return{tool:toolVal(),color:d('ccolor').value,size:Number(d('csize').value),opacity:Number(d('copacity').value)/100,glow:d('cglow').dataset.on==='1',mirror:d('csymmetry').dataset.on==='1',fill:d('cfill').dataset.on==='1',layer:activeLayer};}
var origStrokeAt=strokeAt;
strokeAt=function(x0,y0,x1,y1){if(curStroke){var n=T()-curStroke.st;if(!curStroke.pts.length)curStroke.pts.push([Math.round(x0),Math.round(y0),n]);curStroke.pts.push([Math.round(x1),Math.round(y1),n]);}return origStrokeAt(x0,y0,x1,y1);};
row.addEventListener('pointerdown',function(e){var t=toolVal();if(e.pointerType!=='mouse'&&Object.keys(ptrs).length>1)return;curStroke=(t==='pen'||t==='marker'||t==='spray'||t==='eraser')?Object.assign({type:'stroke',st:T(),pts:[]},sty()):null;},true);
window.addEventListener('pointerup',function(){if(curStroke){var c=curStroke;curStroke=null;if(c.pts.length)rec(c);}});
var origDrawShape=drawShape;
drawShape=function(x0,y0,x1,y1,tc){if(tc===actx()){var q=sty();if(q.tool==='line'||q.tool==='rect'||q.tool==='circle')rec(Object.assign({type:'shape',x0:Math.round(x0),y0:Math.round(y0),x1:Math.round(x1),y1:Math.round(y1)},q));}return origDrawShape.apply(null,arguments);};
var origFill=floodFill;
floodFill=function(fx,fy){var q=sty();rec({type:'fill',x:Math.round(fx),y:Math.round(fy),color:q.color,opacity:q.opacity,layer:q.layer});return origFill(fx,fy);};
var origBake=bake;
bake=function(){rec({type:'bake',k:+s.toFixed(4),sx:+((fx0-tx)/(s*ff)).toFixed(2),sy:+((fy0-ty)/(s*ff)).toFixed(2)});return origBake();};
document.getElementById('cclear').addEventListener('click',function(){rec({type:'clear',layer:activeLayer});},true);
document.getElementById('cundo').addEventListener('click',function(){if(layers[activeLayer].undo.length)rec({type:'undo',layer:activeLayer});},true);
document.getElementById('credo').addEventListener('click',function(){if(layers[activeLayer].redo.length)rec({type:'redo',layer:activeLayer});},true);
/* the actions that survive the undos, in order; a bake is a barrier (undo cannot go back past it, as in the live tool) */
function surviving(events){
  var acts=[],stacks=[[],[],[]],redos=[[],[],[]],barrier=-1;
  events.forEach(function(e){
    if(e.type==='stroke'||e.type==='shape'||e.type==='fill'||e.type==='clear'){var l=e.layer||0;acts.push({e:e,alive:true});stacks[l].push(acts.length-1);redos[l]=[];}
    else if(e.type==='bake'){acts.push({e:e,alive:true});barrier=acts.length-1;stacks=[[],[],[]];redos=[[],[],[]];}
    else if(e.type==='undo'){var l=e.layer||0,i=stacks[l].length?stacks[l][stacks[l].length-1]:-1;if(i>barrier){stacks[l].pop();acts[i].alive=false;redos[l].push(i);}}
    else if(e.type==='redo'){var l2=e.layer||0;if(redos[l2].length){var j=redos[l2].pop();acts[j].alive=true;stacks[l2].push(j);}}
  });
  return acts.filter(function(a){return a.alive;}).map(function(a){return a.e;});
}
function ffCtx(cx,w,h,fx,fy,hex,op){fx=Math.floor(fx);fy=Math.floor(fy);if(fx<0||fy<0||fx>=w||fy>=h)return;var img=cx.getImageData(0,0,w,h),d=img.data,si=(fy*w+fx)*4,tr=d[si],tg=d[si+1],tb=d[si+2],ta=d[si+3],fr=parseInt(hex.slice(1,3),16),fg=parseInt(hex.slice(3,5),16),fb=parseInt(hex.slice(5,7),16),fa=Math.round(op*255);
  if(tr===fr&&tg===fg&&tb===fb&&ta===fa)return;var tol2=32*32*4,vis=new Uint8Array(w*h),st=[si];vis[fy*w+fx]=1;
  function m(i){var a=d[i]-tr,b=d[i+1]-tg,c=d[i+2]-tb,e=d[i+3]-ta;return a*a+b*b+c*c+e*e<=tol2;}
  while(st.length){var i=st.pop();d[i]=fr;d[i+1]=fg;d[i+2]=fb;d[i+3]=fa;var p=(i/4)|0,x=p%w,y=(p/w)|0,nb=[[x-1,y],[x+1,y],[x,y-1],[x,y+1]];for(var k=0;k<4;k++){var nx=nb[k][0],ny=nb[k][1];if(nx<0||ny<0||nx>=w||ny>=h)continue;var vi=ny*w+nx;if(vis[vi])continue;if(m(vi*4)){vis[vi]=1;st.push(vi*4);}}}
  cx.putImageData(img,0,0);}
function drawSegOn(cx,a,x0,y0,x1,y1,mir,W){
  cx.save();var size=a.size*C.px,glow=a.glow;if(glow){cx.shadowColor=a.color;cx.shadowBlur=size*1.4;}
  var seg=function(ax,ay,bx,by){if(a.tool==='eraser'){cx.globalCompositeOperation='destination-out';cx.globalAlpha=1;cx.lineCap='round';cx.lineJoin='round';cx.lineWidth=size*2;cx.beginPath();cx.moveTo(ax,ay);cx.lineTo(bx,by);cx.stroke();}
    else if(a.tool==='spray'){cx.globalAlpha=a.opacity;cx.fillStyle=a.color;var n=Math.max(6,Math.floor(size));for(var i=0;i<n;i++){var an=Math.random()*Math.PI*2,rd=Math.random()*size;cx.beginPath();cx.arc(bx+Math.cos(an)*rd,by+Math.sin(an)*rd,C.px,0,Math.PI*2);cx.fill();}}
    else{cx.lineCap='round';cx.lineJoin='round';cx.strokeStyle=a.color;if(a.tool==='marker'){cx.lineWidth=size*1.6;cx.globalAlpha=Math.min(a.opacity,.45);}else{cx.lineWidth=size;cx.globalAlpha=a.opacity;}cx.beginPath();cx.moveTo(ax,ay);cx.lineTo(bx,by);cx.stroke();}};
  seg(x0,y0,x1,y1);if(mir)seg(W-x0,y0,W-x1,y1);cx.restore();}
function drawShapeOn(cx,a){cx.save();var size=a.size*C.px;cx.globalAlpha=a.opacity;cx.strokeStyle=a.color;cx.fillStyle=a.color;cx.lineWidth=size;cx.lineCap='round';cx.lineJoin='round';if(a.glow){cx.shadowColor=a.color;cx.shadowBlur=size*1.4;}
  if(a.tool==='line'){cx.beginPath();cx.moveTo(a.x0,a.y0);cx.lineTo(a.x1,a.y1);cx.stroke();}
  else if(a.tool==='rect'){var rx=Math.min(a.x0,a.x1),ry=Math.min(a.y0,a.y1),rw=Math.abs(a.x1-a.x0),rh=Math.abs(a.y1-a.y0);if(a.fill)cx.fillRect(rx,ry,rw,rh);else cx.strokeRect(rx,ry,rw,rh);}
  else if(a.tool==='circle'){cx.beginPath();cx.ellipse((a.x0+a.x1)/2,(a.y0+a.y1)/2,Math.abs(a.x1-a.x0)/2,Math.abs(a.y1-a.y0)/2,0,0,Math.PI*2);if(a.fill)cx.fill();else cx.stroke();}
  cx.restore();}
/* Watch: a full-screen replay of the surviving actions (strokes point by point at the recorded pace), 1x to 32x */
var watchEl=null;
function openWatch(){
  if(watchEl)return;
  var W=layers[0].cv.width,H=layers[0].cv.height,acts=surviving(tlog.events),offs=[0,1,2].map(function(){var c=document.createElement('canvas');c.width=W;c.height=H;return c;}),ctxs=offs.map(function(c){return c.getContext('2d');});
  watchEl=el('div','cfs-watch');
  var disp=el('canvas','');disp.id='cwatchcanvas';disp.width=W;disp.height=H;var dctx=disp.getContext('2d');
  var bar=el('div','cfs-watchbar'),bPlay=btn('cwatchplay','Play or pause','<path d="M7 5v14l11-7z"/>'),prog=el('input','');prog.type='range';prog.id='cwatchprogress';prog.min='0';prog.value='0';prog.setAttribute('aria-label','Position');
  /* the speed control (Brandon 2026-11-19): ONE slider that starts at 0 (stopped) in the middle; right plays forward, left plays backward; the
     speed grows exponentially (0.05x just off centre up to 32x at the ends) so small moves give fine control */
  var sp=el('input',''),sl=el('span','cfs-watchspeed','0x'),bX=btn('cwatchclose','Close','<path d="M6 6l12 12M18 6 6 18"/>');
  sp.type='range';sp.id='cwatchspeed';sp.min='-100';sp.max='100';sp.step='0.1';sp.value='50';sp.setAttribute('aria-label','Playback speed, left is backward, right is forward');
  function speedOf(v){var a=Math.abs(v)/100;return v===0?0:(v<0?-1:1)*0.05*Math.pow(640,a);}
  function showSpeed(){var x=speedOf(Number(sp.value));sp.dataset.speed=x.toFixed(3);sl.textContent=(x===0?'0':(x<0?'−':'')+(Math.abs(x)<1?Math.abs(x).toFixed(2):Math.abs(x).toFixed(1)))+'x';}
  bar.appendChild(bPlay);bar.appendChild(prog);bar.appendChild(sp);bar.appendChild(sl);bar.appendChild(bX);watchEl.appendChild(disp);watchEl.appendChild(bar);card.appendChild(watchEl);
  /* the steps: a stroke is one step per segment at its recorded time; the rest are one step each; gaps between actions are capped at 400 ms */
  var ops=[],vt=0,lastT=null;
  acts.forEach(function(a){var at=a.st!==undefined?a.st:a.t;if(lastT!==null)vt+=Math.min(400,Math.max(0,at-lastT));lastT=a.st!==undefined?a.st:a.t;
    if(a.type==='stroke'){var pts=a.pts,base=vt;ops.push({vt:base,f:function(a,pts){return function(){var p=pts[0];drawSegOn(ctxs[a.layer||0],a,p[0],p[1],p[0]+.1,p[1]+.1,a.mirror,W);};}(a,pts)});
      for(var i=1;i<pts.length;i++){ops.push({vt:base+pts[i][2],f:function(a,p0,p1){return function(){drawSegOn(ctxs[a.layer||0],a,p0[0],p0[1],p1[0],p1[1],a.mirror,W);};}(a,pts[i-1],pts[i])});}
      vt=base+pts[pts.length-1][2];lastT=a.st+pts[pts.length-1][2];}
    else if(a.type==='shape'){ops.push({vt:vt,f:function(a){return function(){drawShapeOn(ctxs[a.layer||0],a);};}(a)});}
    else if(a.type==='fill'){ops.push({vt:vt,f:function(a){return function(){var c=ctxs[a.layer||0];ffCtx(c,W,H,a.x,a.y,a.color,a.opacity);};}(a)});}
    else if(a.type==='clear'){ops.push({vt:vt,f:function(a){return function(){ctxs[a.layer||0].clearRect(0,0,W,H);};}(a)});}
    else if(a.type==='bake'){ops.push({vt:vt,f:function(a){return function(){var tmp=document.createElement('canvas');tmp.width=W;tmp.height=H;var t=tmp.getContext('2d');t.imageSmoothingQuality='high';ctxs.forEach(function(c,i){t.clearRect(0,0,W,H);t.drawImage(offs[i],a.sx,a.sy,W/a.k,H/a.k,0,0,W,H);c.clearRect(0,0,W,H);c.drawImage(tmp,0,0);});};}(a)});}
  });
  prog.max=String(ops.length);var idx=0,clock=0,timer=0;
  function paint(){dctx.clearRect(0,0,W,H);for(var i=0;i<3;i++){dctx.globalAlpha=layers[i].opacity;dctx.drawImage(offs[i],0,0);}dctx.globalAlpha=1;prog.value=String(idx);}
  function reset(){ctxs.forEach(function(c){c.clearRect(0,0,W,H);});idx=0;clock=0;}
  function seek(n){reset();while(idx<n&&idx<ops.length){ops[idx].f();idx++;}clock=idx?ops[idx-1].vt:0;paint();}
  function stop(){if(timer){clearInterval(timer);timer=0;}}
  function tick(){
    var x=speedOf(Number(sp.value));if(x===0)return;
    clock+=16*x;var end=ops.length?ops[ops.length-1].vt:0;if(clock>end)clock=end;if(clock<-1)clock=-1;       // -1 is "before the first step": backward all the way shows the empty canvas
    var target=0;while(target<ops.length&&ops[target].vt<=clock)target++;     // how many steps have happened by now
    if(target<idx)seek(target);                                              // backward: rebuild up to that step
    else{while(idx<target){ops[idx].f();idx++;}paint();}
  }
  function go(){if(!timer)timer=setInterval(tick,16);}
  sp.addEventListener('input',showSpeed);showSpeed();
  /* the finer-the-farther scrubbing of this slider comes from assets/finerange.js, which every slider on the site gets */
  bPlay.addEventListener('click',function(){sp.value=Number(sp.value)===0?'50':'0';showSpeed();});
  prog.addEventListener('input',function(){seek(Number(prog.value));});
  bX.addEventListener('click',function(){stop();if(watchEl&&watchEl.parentNode)watchEl.parentNode.removeChild(watchEl);watchEl=null;});
  go();
}
var wRow=el('div','cwatch-row'),bWatch=el('button','ghost cwatch-btn','Watch timelapse'),sel=el('select',''),lab=el('label','','New timelapse every');
bWatch.type='button';bWatch.id='cwatch';sel.id='clogreset';sel.setAttribute('aria-label','New timelapse every');
['never','hour','day','week','month'].forEach(function(v){var o=document.createElement('option');o.value=v;o.textContent=v;if(v===period)o.selected=true;sel.appendChild(o);});
sel.addEventListener('change',function(){period=sel.value;lsPut(LOGRESET,period);});
bWatch.addEventListener('click',function(){openWatch();});
wRow.appendChild(bWatch);wRow.appendChild(lab);wRow.appendChild(sel);pm.appendChild(wRow);
/* a mouse wheel zooms around the cursor (a narrow desktop window, a trackpad) */
row.addEventListener('wheel',function(e){e.preventDefault();var o=origin(),mx=e.clientX-o.x,my=e.clientY-o.y,wx=(mx-tx)/s,wy=(my-ty)/s;
  s=Math.max(MINS,Math.min(MAXS,s*Math.exp(-e.deltaY*.0015)));tx=mx-wx*s;ty=my-wy*s;apply();},{passive:false});
/* the page must never scroll or rubber-band under the canvas, and Safari's own pinch must not take the gesture */
row.addEventListener('touchmove',function(e){e.preventDefault();},{passive:false});
document.addEventListener('gesturestart',function(e){e.preventDefault();});
/* leaving: Close and the browser's Back ask first, so a stray swipe cannot throw the drawing away */
var leaving=false;
function askLeave(){if(leaving)return;if(window.confirm('Leave the canvas? A drawing you have not saved will be lost.')){leaving=true;history.back();}else{try{history.pushState({cfs:1},'');}catch(err){}}}
try{history.pushState({cfs:1},'');}catch(err){}
window.addEventListener('popstate',function(){if(leaving)return;askLeave();});
/* Close: we sit on the entry pushed above, so two steps back leave the canvas page; if there is nothing behind it, go to the profile */
bClose.addEventListener('click',function(){if(leaving)return;if(window.confirm('Leave the canvas? A drawing you have not saved will be lost.')){leaving=true;history.go(-2);setTimeout(function(){window.location.href='/me';},500);}});
/* touching the canvas puts the sheet away */
row.addEventListener('pointerdown',function(){if(sheet.getAttribute('data-open')){openSheet(sheet.getAttribute('data-open'));}});
})();
})();
