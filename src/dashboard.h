#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>

const char HTML_INDEX[] PROGMEM = R"=====(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>LD Switchboard</title>
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,<svg xmlns=%22http://w3.org viewBox=%220 0 100 100%22><rect width=%22100%22 height=%22100%22 rx=%2215%22 fill=%22%2300ff66%22/><text y=%2270%22 x=%2212%22 font-family=%22monospace%22 font-size=%2260%22 font-weight=%22bold%22 fill=%22%23000000%22>LD</text></svg>">
<style>
body{background:#121212;color:#00ff66;font-family:monospace;text-align:center;padding:15px;}
h1{font-size:20px;margin-bottom:2px;color:#fff;}
.version-tag{color:#555;font-size:11px;margin-bottom:15px;}
#status-bar{display:flex;justify-content:space-between;max-width:400px;margin:0 auto 15px auto;color:#888;font-size:13px;}
#mode-color{color:#00ff66;font-weight:bold;background:#222;padding:2px 6px;border-radius:4px;}
.control-panel{background:#1a1a1a;border:1px solid #333;border-radius:6px;max-width:370px;margin:15px auto;padding:15px;text-align:left;}
.control-panel h3{margin:0 0 10px 0;color:#fff;font-size:13px;text-transform:uppercase;border-bottom:1px solid #333;padding-bottom:4px;}
.btn-group{display:flex;gap:6px;margin-bottom:15px;flex-wrap:wrap;}
.btn{background:#222;color:#00ff66;border:1px solid #00ff66;padding:6px 10px;font-family:monospace;font-weight:bold;cursor:pointer;border-radius:4px;font-size:11px;transition:opacity 0.1s;}
.btn:hover{background:#00ff66;color:#000;}
.btn.active{background:#00ff66;color:#000;}
.btn.led-on{background:#3399ff;color:#fff;border-color:#3399ff;}
.btn.led-off{background:#222;color:#aaa;border-color:#444;}
.btn.flip-btn{background:#222;color:#33ffcc;border-color:#33ffcc;width:100%;margin-bottom:6px;}
.btn.flip-btn:hover{background:#33ffcc;color:#000;}
.btn.screen-btn{background:#222;color:#ffaa00;border-color:#ffaa00;width:100%;}
.btn.screen-btn:hover{background:#ffaa00;color:#000;}
.btn.ap-style{background:#223322;color:#33ff33;border-color:#33ff33;width:100%;font-family:monospace;font-weight:bold;padding:6px 10px;cursor:pointer;border-radius:4px;font-size:11px;margin-bottom:6px;}
.btn.ap-style:hover{background:#33ff33;color:#000;}
.btn.wifi-style{background:#222233;color:#3399ff;border-color:#3399ff;width:100%;font-family:monospace;font-weight:bold;padding:6px 10px;cursor:pointer;border-radius:4px;font-size:11px;margin-bottom:6px;}
.btn.wifi-style:hover{background:#3399ff;color:#fff;}
.btn.reboot-style{background:#332222;color:#ff3333;border-color:#ff3333;width:100%;margin-top:4px;font-family:monospace;font-weight:bold;padding:6px 10px;cursor:pointer;border-radius:4px;font-size:11px;}
.btn.reboot-style:hover{background:#ff3333;color:#000;}
.btn:disabled{opacity:0.4;cursor:not-allowed;}
.ota-box{border:1px dashed #444;background:#151515;padding:12px;border-radius:6px;max-width:376px;margin:15px auto;text-align:left;}
</style></head><body>
<h1>LEAK DETECTOR REMOTE</h1><div class="version-tag" id="ver-display">Firmware Loading...</div>
<div id="status-bar"><div>Wireless Link</div><div>Mode: <span id="mode-color">MED</span></div></div>
<div class="control-panel">
<h3>Sensitivity Control</h3><div class="btn-group" id="sens-group">
<button class="btn" id="btn-vlow" onclick="setSensitivity(0)">V_LOW</button>
<button class="btn" id="btn-low" onclick="setSensitivity(1)">LOW</button>
<button class="btn" id="btn-med" onclick="setSensitivity(2)">MED</button>
<button class="btn" id="btn-high" onclick="setSensitivity(3)">HIGH</button></div>
<h3>Hardware Pin Actuators</h3><button class="btn" id="btn-led" onclick="toggleLED()">LED: LOADING</button>
<button class="btn flip-btn" onclick="toggleDisplayFlip()">FLIP DISPLAY SCREEN (180°)</button>
<button class="btn screen-btn" id="btn-screen" onclick="toggleScreenPower()">SWITCH OFF OLED</button>
<hr style="border:0;border-top:1px solid #333;margin:12px 0;">
<button class="btn ap-style" onclick="triggerHotspot()">📡 FORCE FALLBACK HOTSPOT</button>
<button class="btn wifi-style" onclick="triggerWifiReconnect()">🌐 CONNECT TO HOME WIFI</button>
<button class="btn reboot-style" onclick="triggerReboot()">REBOOT DEVICE</button></div>
<div class="ota-box"><span style="color:#fff;font-size:12px;font-weight:bold;">WIRELESS FIRMWARE UPGRADE (OTA)</span>
<form method="POST" action="/update" enctype="multipart/form-data" style="margin-top:8px;">
<input type="file" name="update" accept=".bin" required style="color:#888;font-size:11px;"><br>
<input type="submit" value="Flash Binary" class="btn" style="margin-top:8px;border-color:#aaa;color:#fff;"></form></div>
<script>
let isWaiting=false;let oledOn=true;
async function setSensitivity(m){
if(isWaiting)return;isWaiting=true;setButtonsDisabled(true);
try{await fetch('/setsens?mode='+m);await updateTextLabelsOnce();}catch(e){}
setTimeout(()=>{isWaiting=false;setButtonsDisabled(false);},120);
}
async function toggleDisplayFlip(){
if(isWaiting)return;isWaiting=true;
try{await fetch('/flipdisplay');}catch(e){}
setTimeout(()=>{isWaiting=false;},120);
}
async function toggleScreenPower(){
if(isWaiting)return;isWaiting=true;oledOn=!oledOn;
const b=document.getElementById('btn-screen');
if(oledOn){b.innerText='SWITCH OFF OLED';b.style.color='#ffaa00';b.style.borderColor='#ffaa00';}
else{b.innerText='SWITCH ON OLED';b.style.color='#888';b.style.borderColor='#444';}
try{await fetch('/togglescreen');}catch(e){}
setTimeout(()=>{isWaiting=false;},120);
}
async function toggleLED(){
if(isWaiting)return;isWaiting=true;
try{await fetch('/toggleled');await updateTextLabelsOnce();}catch(e){}
setTimeout(()=>{isWaiting=false;},120);
}
function setButtonsDisabled(state){
const btns=document.getElementById('sens-group').getElementsByClassName('btn');
for(let i=0;i<btns.length;i++){btns[i].disabled=state;}
}
function triggerHotspot(){
if(confirm('Force local hotspot mode?')){fetch('/forceap');alert('Hotspot deployed! Connect to: LeakDetector-AP');}
}
function triggerWifiReconnect(){
if(confirm('Reconnect to home Wi-Fi?')){fetch('/connectwifi');alert('Reconnecting. Hotspot shutting down...');}
}
function triggerReboot(){if(confirm('Confirm system reboot request?')){fetch('/reboot');alert('Reboot deployed. Reconnecting...');setTimeout(()=>{location.reload();},4000);}}
async function updateTextLabelsOnce(){try{let r=await fetch('/getdata');let json=await r.json();
document.getElementById('ver-display').innerText='Firmware: '+json.version;
document.getElementById('mode-color').innerText=json.mode;
document.getElementById('btn-vlow').classList.toggle('active',json.mode==='V_LOW');
document.getElementById('btn-low').classList.toggle('active',json.mode==='LOW');
document.getElementById('btn-med').classList.toggle('active',json.mode==='MED');
document.getElementById('btn-high').classList.toggle('active',json.mode==='HIGH');
const ledBtn=document.getElementById('btn-led');
if(json.led){ledBtn.innerText="BLUE LED: ACTIVE ON";ledBtn.className="btn led-on";}
else{ledBtn.innerText="BLUE LED: OFF";ledBtn.className="btn led-off";}}catch(e){}}
window.onload=function(){setInterval(updateTextLabelsOnce,500);};
</script></body></html>
)=====";

#endif
