#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>

const char HTML_INDEX[] PROGMEM = R"=====(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>LD Dashboard</title>
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,<svg xmlns=%22http://w3.org viewBox=%220 0 100 100%22><rect width=%22100%22 height=%22100%22 rx=%2215%22 fill=%22%2300ff66%22/><text y=%2270%22 x=%2212%22 font-family=%22monospace%22 font-size=%2260%22 font-weight=%22bold%22 fill=%22%23000000%22>LD</text></svg>">
<style>
body{background:#121212;color:#00ff66;font-family:monospace;text-align:center;padding:15px;}
h1{font-size:20px;margin-bottom:2px;color:#fff;}
.version-tag{color:#555;font-size:11px;margin-bottom:15px;}
#status-bar{display:flex;justify-content:space-between;max-width:400px;margin:0 auto 15px auto;color:#888;font-size:13px;}
#mode-color{color:#00ff66;font-weight:bold;background:#222;padding:2px 6px;border-radius:4px;}
canvas{background:#000;border:2px solid #333;border-radius:6px;max-width:100%;display:inline-block;}
.control-panel{background:#1a1a1a;border:1px solid #333;border-radius:6px;max-width:370px;margin:15px auto;padding:15px;text-align:left;}
.control-panel h3{margin:0 0 10px 0;color:#fff;font-size:13px;text-transform:uppercase;border-bottom:1px solid #333;padding-bottom:4px;}
.btn-group{display:flex;gap:6px;margin-bottom:15px;flex-wrap:wrap;}
.btn{background:#222;color:#00ff66;border:1px solid #00ff66;padding:6px 10px;font-family:monospace;font-weight:bold;cursor:pointer;border-radius:4px;font-size:11px;}
.btn:hover{background:#00ff66;color:#000;}
.btn.active{background:#00ff66;color:#000;}
.btn.led-on{background:#3399ff;color:#fff;border-color:#3399ff;}
.btn.led-off{background:#222;color:#aaa;border-color:#444;}
.ota-box{border:1px dashed #444;background:#151515;padding:12px;border-radius:6px;max-width:376px;margin:15px auto;text-align:left;}
</style></head><body>
<h1>LEAK DETECTOR GEQ</h1><div class="version-tag" id="ver-display">Firmware Loading...</div>
<div id="status-bar"><div>Wireless Remote</div><div>Mode: <span id="mode-color">LOW</span></div></div>
<canvas id="geqCanvas" width="400" height="220"></canvas>
<div class="control-panel"><h3>Sensitivity Control</h3><div class="btn-group">
<button class="btn" id="btn-vlow" onclick="setSensitivity(0)">V_LOW</button>
<button class="btn" id="btn-low" onclick="setSensitivity(1)">LOW</button>
<button class="btn" id="btn-med" onclick="setSensitivity(2)">MED</button>
<button class="btn" id="btn-high" onclick="setSensitivity(3)">HIGH</button></div>
<h3>Hardware Pin Actuators</h3><button class="btn" id="btn-led" onclick="toggleLED()">LED: LOADING</button></div>
<div class="ota-box"><span style="color:#fff;font-size:12px;font-weight:bold;">WIRELESS FIRMWARE UPGRADE (OTA)</span>
<form method="POST" action="/update" enctype="multipart/form-data" style="margin-top:8px;">
<input type="file" name="update" accept=".bin" required style="color:#888;font-size:11px;"><br>
<input type="submit" value="Flash Binary" class="btn" style="margin-top:8px;border-color:#aaa;color:#fff;"></form></div>
<script>
function setSensitivity(m){fetch('/setsens?mode='+m);}
function toggleLED(){fetch('/toggleled');}
window.onload=function(){
const canvas=document.getElementById('geqCanvas');const ctx=canvas.getContext('2d');
const numBars=10;let barHeights=Array(numBars).fill(0);
async function updateData(){try{let r=await fetch('/getdata');let json=await r.json();
document.getElementById('ver-display').innerText='Firmware: '+json.version;
document.getElementById('mode-color').innerText=json.mode;
document.getElementById('btn-vlow').classList.toggle('active',json.mode==='V_LOW');
document.getElementById('btn-low').classList.toggle('active',json.mode==='LOW');
document.getElementById('btn-med').classList.toggle('active',json.mode==='MED');
document.getElementById('btn-high').classList.toggle('active',json.mode==='HIGH');
const ledBtn=document.getElementById('btn-led');
if(json.led){ledBtn.innerText="BLUE LED: ACTIVE ON";ledBtn.className="btn led-on";}
else{ledBtn.innerText="BLUE LED: OFF";ledBtn.className="btn led-off";}
barHeights.pop();barHeights.unshift(json.level);}catch(e){}}
function drawVisualizer(){ctx.clearRect(0,0,canvas.width,canvas.height);let barWidth=(canvas.width/numBars)-8;
barHeights.forEach((val,idx)=>{let h=(val/40)*canvas.height;let x=idx*(barWidth+8)+4;let y=canvas.height-h;
ctx.fillStyle='#00ff66';ctx.fillRect(x,y,barWidth,h);});requestAnimationFrame(drawVisualizer);}
setInterval(updateData,45);drawVisualizer();};
</script></body></html>
)=====";

#endif
