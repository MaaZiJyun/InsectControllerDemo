/**
 * @file ControllerPage.h
 * @brief Controller 单页前端。
 */
#pragma once

#include <Arduino.h>

namespace ControllerPage
{
static const char HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
  <title>Insect Controller</title>
  <style>
    :root{color-scheme:dark;--bg:#11151b;--panel:#1b222c;--line:#303b49;--accent:#68d391;--stop:#ef6a6a}
    *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
    body{margin:0;min-height:100vh;background:var(--bg);color:#f4f7fa;font-family:system-ui,sans-serif;display:grid;place-items:center}
    main{width:min(920px,100%);padding:18px;display:grid;gap:18px}
    h1{font-size:1.25rem;margin:0}.panel{background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:14px}
    .video{width:100%;aspect-ratio:4/3;display:block;object-fit:contain;background:#050607;border-radius:10px}
    .status{margin-top:10px;color:#aeb9c7;font-size:.9rem}
    .pad{display:grid;grid-template-columns:repeat(3,minmax(72px,112px));grid-template-areas:". forward ." "left stop right" ". back .";gap:12px;justify-content:center}
    button{min-height:72px;border:1px solid var(--line);border-radius:14px;background:#27313e;color:#fff;font-size:1.05rem;font-weight:700;touch-action:none;cursor:pointer}
    button:active,.active{transform:scale(.97);background:var(--accent);color:#07120b}.forward{grid-area:forward}.back{grid-area:back}.left{grid-area:left}.right{grid-area:right}.stop{grid-area:stop;background:#512b31;border-color:#8a454e}.stop:active{background:var(--stop)}
    @media(min-width:760px){main{grid-template-columns:1.5fr 1fr;align-items:center}}
  </style>
</head>
<body>
<main>
  <section class="panel"><h1>实时摄像头</h1><img id="video" class="video" alt="Camera stream"><div id="status" class="status">正在连接…</div></section>
  <section class="panel"><div class="pad">
    <button class="forward" data-command="forward">前进</button>
    <button class="back" data-command="back">后退</button>
    <button class="left" data-command="left">左转</button>
    <button class="right" data-command="right">右转</button>
    <button class="stop" data-command="stop">停止</button>
  </div></section>
</main>
<script>
const statusNode=document.getElementById('status');
const send=name=>fetch('/command?name='+encodeURIComponent(name),{method:'POST',cache:'no-store'}).then(r=>{if(!r.ok)throw Error(r.status)}).catch(()=>statusNode.textContent='命令发送失败');
fetch('/config',{cache:'no-store'}).then(r=>r.json()).then(c=>{document.getElementById('video').src='http://'+location.hostname+':'+c.videoPort+'/stream';statusNode.textContent='控制端口 '+c.controlPort+' · 视频端口 '+c.videoPort}).catch(()=>statusNode.textContent='配置读取失败');
let repeat=null,active=null;
function release(){if(repeat){clearInterval(repeat);repeat=null}if(active){active.classList.remove('active');active=null}send('stop')}
document.querySelectorAll('button[data-command]').forEach(button=>{
  button.addEventListener('pointerdown',event=>{event.preventDefault();release();const cmd=button.dataset.command;if(cmd==='stop'){send('stop');return}active=button;button.classList.add('active');send(cmd);repeat=setInterval(()=>send(cmd),250)});
  button.addEventListener('pointerup',release);button.addEventListener('pointercancel',release);button.addEventListener('pointerleave',event=>{if(event.buttons)release()});
});
window.addEventListener('blur',release);
</script>
</body>
</html>
)HTML";
}
