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
    :root{color-scheme:dark;--glass:rgba(255,255,255,.20);--glass-active:rgba(255,255,255,.72);--stop:#e53935;--safe-top:env(safe-area-inset-top,0px);--safe-bottom:env(safe-area-inset-bottom,0px)}
    *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
    html,body{width:100%;height:100%;margin:0;overflow:hidden;background:#050505;color:#fff;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
    .camera{position:fixed;inset:0;width:100vw;height:100dvh;overflow:hidden;background:radial-gradient(circle at center,#242424,#050505 70%)}
    .video{position:absolute;inset:0;width:100%;height:100%;display:block;object-fit:cover;background:transparent}
    .shade{position:absolute;inset:0;pointer-events:none;background:linear-gradient(to bottom,rgba(0,0,0,.55),transparent 25%,transparent 62%,rgba(0,0,0,.58))}
    .topbar{position:absolute;z-index:2;top:0;left:0;right:0;padding:calc(14px + var(--safe-top)) 18px 14px;display:flex;align-items:flex-start;justify-content:space-between;gap:16px;text-shadow:0 1px 5px rgba(0,0,0,.8);pointer-events:none}
    .camera-state{display:flex;align-items:center;gap:8px;font-size:.92rem;font-weight:650}.dot{width:9px;height:9px;border-radius:50%;background:#f6b73c;box-shadow:0 0 0 3px rgba(246,183,60,.18)}
    .dot.online{background:#49d17d;box-shadow:0 0 0 3px rgba(73,209,125,.18)}.dot.offline{background:#ef5350;box-shadow:0 0 0 3px rgba(239,83,80,.18)}
    .clock{text-align:right}.time{font-size:1.15rem;font-weight:720;letter-spacing:.02em}.date{margin-top:2px;font-size:.78rem;opacity:.82}
    .settings{position:absolute;z-index:3;left:50%;bottom:calc(218px + var(--safe-bottom));transform:translateX(-50%);display:grid;grid-template-columns:1fr 1fr;gap:8px 12px;width:min(330px,calc(100vw - 28px));padding:11px 13px;border:1px solid rgba(255,255,255,.18);border-radius:18px;background:rgba(0,0,0,.28);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px)}
    .setting{display:flex;align-items:center;justify-content:space-between;gap:7px;font-size:.78rem}.setting input{width:72px;padding:6px 7px;border:1px solid rgba(255,255,255,.32);border-radius:9px;background:rgba(255,255,255,.16);color:#fff;font:inherit;text-align:center;outline:none}.setting input:focus{border-color:rgba(255,255,255,.8)}
    .wave-info{grid-column:1/-1;text-align:center;font-size:.7rem;opacity:.82}.wave-info.invalid{color:#ff8b87;opacity:1}
    .controls{position:absolute;z-index:3;left:50%;bottom:calc(18px + var(--safe-bottom));transform:translateX(-50%);display:grid;grid-template-columns:repeat(3,64px);grid-template-rows:repeat(3,56px);grid-template-areas:". forward ." "left stop right" ". back .";gap:9px;padding:12px;-webkit-backdrop-filter:blur(10px)}
    button{border:1px solid rgba(255,255,255,.42);border-radius:18px;background:var(--glass);color:#fff;font-size:1.45rem;font-weight:750;text-shadow:0 1px 3px rgba(0,0,0,.5);box-shadow:0 4px 14px rgba(0,0,0,.18);touch-action:none;user-select:none;cursor:pointer;transition:transform .08s ease,background .08s ease}
    button:active,button.active{transform:scale(.93);background:var(--glass-active);color:#151515;text-shadow:none}.forward{grid-area:forward}.back{grid-area:back}.left{grid-area:left}.right{grid-area:right}
    .stop{grid-area:stop;background:rgba(229,57,53,.88);border-color:rgba(255,255,255,.58);font-size:.72rem;letter-spacing:.05em;text-transform:uppercase}.stop:active,.stop.active{background:#ff504b;color:#fff}
    @media(max-height:560px) and (orientation:landscape){.controls{left:auto;right:calc(18px + var(--safe-bottom));bottom:50%;transform:translateY(50%);grid-template-columns:repeat(3,58px);grid-template-rows:repeat(3,50px)}.settings{left:18px;bottom:18px;transform:none;width:310px}.topbar{padding-right:220px}}
  </style>
</head>
<body>
<main class="camera">
  <img id="video" class="video" alt="Camera stream">
  <div class="shade"></div>
  <header class="topbar">
    <div class="camera-state"><span id="cameraDot" class="dot"></span><span id="cameraStatus">Camera Connecting</span></div>
    <div class="clock"><div id="time" class="time">--:--:--</div><div id="date" class="date">----</div></div>
  </header>
  <section class="settings" aria-label="Waveform settings">
    <label class="setting">Cycles<input id="cycleCount" type="number" inputmode="numeric"></label>
    <label class="setting">Frequency<input id="frequency" type="number" inputmode="decimal"><span>Hz</span></label>
    <div id="waveInfo" class="wave-info">Loading settings…</div>
  </section>
  <section class="controls">
    <button class="forward" data-command="forward" aria-label="Forward">▲</button>
    <button class="back" data-command="back" aria-label="Back">▼</button>
    <button class="left" data-command="left" aria-label="Left">◀</button>
    <button class="right" data-command="right" aria-label="Right">▶</button>
    <button class="stop" data-command="stop" aria-label="Stop">STOP</button>
  </section>
</main>
<script>
const video=document.getElementById('video'),cameraDot=document.getElementById('cameraDot'),cameraStatus=document.getElementById('cameraStatus');
const cycleInput=document.getElementById('cycleCount'),frequencyInput=document.getElementById('frequency'),waveInfo=document.getElementById('waveInfo');
let maxDurationMs=500;
function setCameraState(state,text){cameraDot.className='dot '+state;cameraStatus.textContent=text}
function updateClock(){const now=new Date();document.getElementById('time').textContent=now.toLocaleTimeString('en-US', { hour12: false });document.getElementById('date').textContent=now.toLocaleDateString('en-US',{year:'numeric',month:'2-digit',day:'2-digit',weekday:'short'})}
updateClock();setInterval(updateClock,1000);
video.addEventListener('load',()=>setCameraState('online','Camera Online'));video.addEventListener('error',()=>setCameraState('offline','Camera Connection Failed'));
function waveform(){const cycles=Number(cycleInput.value),frequency=Number(frequencyInput.value);const phaseMs=Math.max(1,Math.round(500/frequency));const durationMs=cycles*2*phaseMs;const valid=Number.isInteger(cycles)&&cycles>=Number(cycleInput.min)&&cycles<=Number(cycleInput.max)&&Number.isFinite(frequency)&&frequency>=Number(frequencyInput.min)&&frequency<=Number(frequencyInput.max)&&durationMs<=maxDurationMs;waveInfo.textContent=valid?'Phase '+phaseMs+' ms · Total '+durationMs+' ms':'Invalid: total duration must be ≤ '+maxDurationMs+' ms';waveInfo.classList.toggle('invalid',!valid);return{cycles,frequency,valid}}
cycleInput.addEventListener('input',waveform);frequencyInput.addEventListener('input',waveform);
const send=name=>{let url='/command?name='+encodeURIComponent(name);if(name!=='stop'){const w=waveform();if(!w.valid)return Promise.resolve();url+='&cycleCount='+w.cycles+'&frequency='+w.frequency}return fetch(url,{method:'POST',cache:'no-store'}).then(async r=>{if(!r.ok)throw Error((await r.json()).error||r.status)}).catch(error=>{waveInfo.textContent=error.message;waveInfo.classList.add('invalid')})};
fetch('/config',{cache:'no-store'}).then(r=>r.json()).then(c=>{video.src='http://'+location.hostname+':'+c.videoPort+'/stream';cycleInput.min=c.minCycleCount;cycleInput.max=c.maxCycleCount;cycleInput.value=c.cycleCount;frequencyInput.min=c.minFrequency;frequencyInput.max=c.maxFrequency;frequencyInput.step='0.1';frequencyInput.value=c.frequency;maxDurationMs=c.maxDurationMs;waveform()}).catch(()=>setCameraState('offline','Config Read Failed'));
let active=null;
function clearActive(){if(active){active.classList.remove('active');active=null}}
document.querySelectorAll('button[data-command]').forEach(button=>{
  button.addEventListener('pointerdown',event=>{event.preventDefault();clearActive();active=button;button.classList.add('active');send(button.dataset.command)});
  button.addEventListener('pointerup',clearActive);button.addEventListener('pointercancel',clearActive);button.addEventListener('pointerleave',clearActive);
});
window.addEventListener('blur',clearActive);
</script>
</body>
</html>
)HTML";
}
