#include "StepperWebUI.h"
#include "TMC2209Controller.h"
#include "INA226Monitor.h"
#include "WiFiService.h"

// ======================= WEB UI HTML / CSS / JS =======================
static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 TMC2209 & INA226 Motion & Power Dashboard</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700;800&family=JetBrains+Mono:wght@400;600&display=swap" rel="stylesheet">
<style>
:root {
  --bg-primary: #0a0d14;
  --bg-card: rgba(17, 24, 39, 0.75);
  --bg-card-hover: rgba(31, 41, 55, 0.85);
  --border-color: rgba(255, 255, 255, 0.08);
  --border-highlight: rgba(99, 102, 241, 0.3);
  --text-main: #f3f4f6;
  --text-muted: #9ca3af;
  --text-sub: #6b7280;
  --accent-cyan: #06b6d4;
  --accent-emerald: #10b981;
  --accent-amber: #f59e0b;
  --accent-rose: #f43f5e;
  --accent-indigo: #6366f1;
  --accent-purple: #a855f7;
  --font-sans: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  --font-mono: 'JetBrains Mono', monospace;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  font-family: var(--font-sans);
  background-color: var(--bg-primary);
  background-image: 
    radial-gradient(at 0% 0%, rgba(99, 102, 241, 0.12) 0px, transparent 50%),
    radial-gradient(at 100% 100%, rgba(6, 182, 212, 0.1) 0px, transparent 50%),
    radial-gradient(at 50% 50%, rgba(16, 185, 129, 0.05) 0px, transparent 50%);
  color: var(--text-main);
  min-height: 100vh;
  padding: 1.5rem;
  display: flex;
  flex-direction: column;
  align-items: center;
}
.app-container {
  width: 100%;
  max-width: 1100px;
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
}
/* Header */
header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 1rem;
  padding: 1.25rem 1.75rem;
  background: var(--bg-card);
  backdrop-filter: blur(16px);
  border: 1px solid var(--border-color);
  border-radius: 1.25rem;
  box-shadow: 0 10px 30px rgba(0,0,0,0.4);
}
.brand-group { display: flex; align-items: center; gap: 1rem; }
.logo-icon {
  width: 44px; height: 44px;
  background: linear-gradient(135deg, var(--accent-indigo), var(--accent-cyan));
  border-radius: 12px;
  display: flex; align-items: center; justify-content: center;
  box-shadow: 0 0 20px rgba(99,102,241,0.4);
}
.logo-icon svg { width: 24px; height: 24px; fill: none; stroke: #fff; stroke-width: 2; }
.brand-title h1 { font-size: 1.25rem; font-weight: 800; letter-spacing: -0.02em; }
.brand-title p { font-size: 0.8rem; color: var(--text-muted); }
.header-badges { display: flex; align-items: center; gap: 0.75rem; }
.badge {
  display: inline-flex; align-items: center; gap: 0.4rem;
  padding: 0.35rem 0.75rem; border-radius: 9999px;
  font-size: 0.75rem; font-weight: 600;
  background: rgba(255,255,255,0.05); border: 1px solid var(--border-color);
}
.badge-online { background: rgba(16,185,129,0.15); border-color: rgba(16,185,129,0.3); color: #34d399; }
.pulse-dot {
  width: 8px; height: 8px; border-radius: 50%;
  background: #34d399; box-shadow: 0 0 10px #34d399;
  animation: pulse 1.8s infinite;
}
@keyframes pulse { 0%, 100% { opacity: 1; transform: scale(1); } 50% { opacity: 0.4; transform: scale(0.85); } }

/* Telemetry Hero Grid */
.telemetry-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
  gap: 1.25rem;
}
.metric-card {
  background: var(--bg-card);
  backdrop-filter: blur(16px);
  border: 1px solid var(--border-color);
  border-radius: 1.25rem;
  padding: 1.25rem 1.5rem;
  position: relative;
  overflow: hidden;
  transition: transform 0.2s ease, border-color 0.2s ease, box-shadow 0.2s ease;
}
.metric-card:hover {
  transform: translateY(-3px);
  border-color: var(--border-highlight);
  box-shadow: 0 12px 28px rgba(0,0,0,0.35);
}
.metric-card::before {
  content: ''; position: absolute; top: 0; left: 0; right: 0; height: 3px;
}
.card-voltage::before { background: linear-gradient(90deg, #6366f1, #818cf8); }
.card-current::before { background: linear-gradient(90deg, #10b981, #34d399); }
.card-power::before   { background: linear-gradient(90deg, #f59e0b, #fbbf24); }
.card-shunt::before   { background: linear-gradient(90deg, #a855f7, #c084fc); }
.card-stall::before   { background: linear-gradient(90deg, #ec4899, #f43f5e); }

.metric-header {
  display: flex; justify-content: space-between; align-items: center;
  margin-bottom: 0.75rem;
}
.metric-label { font-size: 0.8rem; font-weight: 600; text-transform: uppercase; letter-spacing: 0.05em; color: var(--text-muted); }
.metric-pill { font-size: 0.7rem; padding: 0.2rem 0.5rem; border-radius: 6px; background: rgba(255,255,255,0.06); color: var(--text-muted); font-family: var(--font-mono); }
.metric-value-row { display: flex; align-items: baseline; gap: 0.35rem; }
.metric-number { font-size: 2.1rem; font-weight: 800; font-family: var(--font-mono); letter-spacing: -0.03em; }
.metric-unit { font-size: 1rem; font-weight: 600; color: var(--text-muted); }

/* Control & Status Main Grid */
.dashboard-main {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 1.5rem;
}
@media (max-width: 860px) {
  .dashboard-main { grid-template-columns: 1fr; }
}

.panel-card {
  background: var(--bg-card);
  backdrop-filter: blur(16px);
  border: 1px solid var(--border-color);
  border-radius: 1.25rem;
  padding: 1.5rem;
  display: flex;
  flex-direction: column;
  gap: 1.25rem;
}
.panel-title {
  display: flex; align-items: center; justify-content: space-between;
  border-bottom: 1px solid var(--border-color);
  padding-bottom: 0.75rem;
}
.panel-title h2 { font-size: 1.05rem; font-weight: 700; letter-spacing: -0.01em; }

/* Shaft animation box */
.shaft-box {
  display: flex; align-items: center; justify-content: center;
  gap: 1.5rem; padding: 1rem;
  background: rgba(0,0,0,0.3); border-radius: 1rem;
  border: 1px solid rgba(255,255,255,0.04);
}
.motor-shaft {
  width: 72px; height: 72px; border-radius: 50%;
  background: radial-gradient(circle, #374151 30%, #1f2937 70%);
  border: 3px solid rgba(255,255,255,0.15);
  display: flex; align-items: center; justify-content: center;
  position: relative;
  box-shadow: 0 0 15px rgba(0,0,0,0.6);
}
.shaft-notch {
  width: 6px; height: 30px; border-radius: 3px;
  background: linear-gradient(to bottom, #6366f1, #38bdf8);
  position: absolute; top: 6px;
  box-shadow: 0 0 10px #38bdf8;
}
.spin-fwd { animation: rotateCW 1.5s linear infinite; }
.spin-rev { animation: rotateCCW 1.5s linear infinite; }
@keyframes rotateCW { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
@keyframes rotateCCW { 0% { transform: rotate(0deg); } 100% { transform: rotate(-360deg); } }

.motion-state-text { display: flex; flex-direction: column; gap: 0.25rem; }
.motion-mode-badge {
  font-size: 1.15rem; font-weight: 800; font-family: var(--font-mono);
  color: var(--accent-cyan);
}
.motion-sub-text { font-size: 0.8rem; color: var(--text-muted); }

/* Control Buttons */
.btn-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 0.75rem; }
.btn {
  display: inline-flex; align-items: center; justify-content: center;
  gap: 0.5rem; padding: 0.85rem 1rem;
  border-radius: 0.75rem; font-size: 0.9rem; font-weight: 700;
  cursor: pointer; border: 1px solid transparent;
  transition: all 0.15s ease;
  user-select: none;
}
.btn:active { transform: scale(0.96); }
.btn-primary {
  background: linear-gradient(135deg, #4f46e5, #6366f1);
  color: #fff; box-shadow: 0 4px 14px rgba(79,70,229,0.35);
}
.btn-primary:hover { background: linear-gradient(135deg, #4338ca, #4f46e5); box-shadow: 0 6px 20px rgba(79,70,229,0.5); }
.btn-stop {
  background: linear-gradient(135deg, #e11d48, #f43f5e);
  color: #fff; box-shadow: 0 4px 14px rgba(225,29,72,0.35);
}
.btn-stop:hover { background: linear-gradient(135deg, #be123c, #e11d48); box-shadow: 0 6px 20px rgba(225,29,72,0.5); }
.btn-secondary {
  background: rgba(255,255,255,0.06); border: 1px solid var(--border-color);
  color: var(--text-main);
}
.btn-secondary:hover { background: rgba(255,255,255,0.1); border-color: rgba(255,255,255,0.18); }
.btn-amber {
  background: linear-gradient(135deg, #d97706, #f59e0b);
  color: #fff; box-shadow: 0 4px 14px rgba(217,119,6,0.3);
}
.btn-amber:hover { background: linear-gradient(135deg, #b45309, #d97706); }

/* Slider & Controls */
.slider-group { display: flex; flex-direction: column; gap: 0.5rem; }
.slider-header { display: flex; justify-content: space-between; font-size: 0.85rem; font-weight: 600; color: var(--text-muted); }
.slider-header span:last-child { color: var(--accent-cyan); font-family: var(--font-mono); }
input[type=range] {
  -webkit-appearance: none; width: 100%; height: 8px;
  background: rgba(255,255,255,0.1); border-radius: 4px; outline: none;
}
input[type=range]::-webkit-slider-thumb {
  -webkit-appearance: none; width: 22px; height: 22px; border-radius: 50%;
  background: var(--accent-indigo); cursor: pointer; box-shadow: 0 0 10px var(--accent-indigo);
}

/* Microstep / Current Pills */
.pill-grid { display: flex; flex-wrap: wrap; gap: 0.5rem; }
.pill-btn {
  padding: 0.4rem 0.75rem; border-radius: 0.5rem;
  background: rgba(255,255,255,0.05); border: 1px solid var(--border-color);
  color: var(--text-muted); font-size: 0.8rem; font-family: var(--font-mono);
  font-weight: 600; cursor: pointer; transition: all 0.15s ease;
}
.pill-btn:hover { background: rgba(255,255,255,0.1); color: var(--text-main); }
.pill-btn.active {
  background: rgba(99,102,241,0.25); border-color: var(--accent-indigo);
  color: #818cf8; box-shadow: 0 0 12px rgba(99,102,241,0.3);
}

/* Toggle Switches */
.toggle-row { display: flex; align-items: center; justify-content: space-between; padding: 0.5rem 0; }
.toggle-label { display: flex; flex-direction: column; gap: 0.15rem; }
.toggle-title { font-size: 0.9rem; font-weight: 600; }
.toggle-desc { font-size: 0.75rem; color: var(--text-muted); }

/* Diagnostics Table */
.diag-table {
  width: 100%; border-collapse: collapse; font-size: 0.8rem; font-family: var(--font-mono);
}
.diag-table tr { border-bottom: 1px solid rgba(255,255,255,0.04); }
.diag-table td { padding: 0.5rem 0.25rem; }
.diag-table td:first-child { color: var(--text-muted); }
.diag-table td:last-child { text-align: right; font-weight: 600; color: var(--accent-cyan); }

footer {
  text-align: center; font-size: 0.75rem; color: var(--text-sub);
  margin-top: 1rem;
}
</style>
</head>
<body>
<div class="app-container">

  <!-- Header -->
  <header>
    <div class="brand-group">
      <div class="logo-icon">
        <svg viewBox="0 0 24 24"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg>
      </div>
      <div class="brand-title">
        <h1>TMC2209 & INA226 Controller</h1>
        <p>ESP32 Motion & Precision Power Telemetry</p>
      </div>
    </div>
    <div class="header-badges">
      <div class="badge badge-online">
        <div class="pulse-dot"></div>
        <span id="badge-status">ONLINE</span>
      </div>
      <div class="badge" id="badge-ip">IP: ---</div>
      <div class="badge" id="badge-rssi">RSSI: ---</div>
    </div>
  </header>

  <!-- Real-Time Telemetry Hero Cards -->
  <section class="telemetry-grid">
    <div class="metric-card card-voltage">
      <div class="metric-header">
        <span class="metric-label">Bus Voltage</span>
        <span class="metric-pill">VBUS</span>
      </div>
      <div class="metric-value-row">
        <span class="metric-number" id="val-vbus">--.--</span>
        <span class="metric-unit">V</span>
      </div>
    </div>

    <div class="metric-card card-current">
      <div class="metric-header">
        <span class="metric-label">DC Bus Current</span>
        <span class="metric-pill">INA226 (0.1Ω)</span>
      </div>
      <div class="metric-value-row">
        <span class="metric-number" id="val-current">--.-</span>
        <span class="metric-unit">mA</span>
      </div>
      <div style="font-size:0.7rem; color:var(--text-sub); margin-top:0.35rem;">Power supply rail input (PWM buck-stepped into coils)</div>
    </div>

    <div class="metric-card card-power">
      <div class="metric-header">
        <span class="metric-label">Power Consumption</span>
        <span class="metric-pill">Live DC</span>
      </div>
      <div class="metric-value-row">
        <span class="metric-number" id="val-power">-.--</span>
        <span class="metric-unit">W</span>
      </div>
    </div>

    <div class="metric-card card-shunt">
      <div class="metric-header">
        <span class="metric-label">Shunt Voltage Drop</span>
        <span class="metric-pill">VSHUNT</span>
      </div>
      <div class="metric-value-row">
        <span class="metric-number" id="val-vshunt">--.--</span>
        <span class="metric-unit">mV</span>
      </div>
    </div>

    <div class="metric-card card-stall">
      <div class="metric-header">
        <span class="metric-label">StallGuard4 Load</span>
        <span class="metric-pill" id="lbl-stall-state">OK</span>
      </div>
      <div class="metric-value-row">
        <span class="metric-number" id="val-sg">---</span>
        <span class="metric-unit">/ 510</span>
      </div>
      <div style="font-size:0.7rem; color:var(--text-sub); margin-top:0.35rem;" id="lbl-sg-hint">Torque margin (drops to 0 when losing steps)</div>
    </div>
  </section>

  <!-- Dashboard Main Panels -->
  <main class="dashboard-main">

    <!-- Motion Controls Panel -->
    <div class="panel-card">
      <div class="panel-title">
        <h2>Motion Control Center</h2>
        <span class="metric-pill" id="lbl-steps-sec">--- steps/s</span>
      </div>

      <!-- Animated Motor Shaft State -->
      <div class="shaft-box">
        <div class="motor-shaft" id="shaft-anim">
          <div class="shaft-notch"></div>
        </div>
        <div class="motion-state-text">
          <span class="motion-mode-badge" id="lbl-motor-state">STOPPED</span>
          <span class="motion-sub-text" id="lbl-motor-sub">Ready for motion commands</span>
        </div>
      </div>

      <!-- Primary Motion Buttons -->
      <div class="btn-grid">
        <button class="btn btn-primary" onclick="sendAction('reverse')">&#9664; REVERSE</button>
        <button class="btn btn-stop" onclick="sendAction('stop')">&#9632; STOP</button>
        <button class="btn btn-primary" onclick="sendAction('forward')">&#9654; FORWARD</button>
      </div>

      <!-- Revolution Jog & Demo -->
      <div class="btn-grid">
        <button class="btn btn-secondary" onclick="sendAction('revs', '&val=1.0&dir=rev')">&#8634; -1 Rev</button>
        <button class="btn btn-amber" onclick="sendAction('autodemo')">&#9889; Auto-Demo</button>
        <button class="btn btn-secondary" onclick="sendAction('revs', '&val=1.0&dir=fwd')">&#8635; +1 Rev</button>
      </div>

      <!-- Stall / Lost Step Alert Banner -->
      <div id="stall-alert-box" style="display:none; padding: 0.75rem 1rem; border-radius: 0.75rem; background: rgba(244,63,94,0.18); border: 1px solid rgba(244,63,94,0.45); color: #fda4af; font-size: 0.85rem; font-weight: 700; text-align: center; animation: pulse 1.5s infinite;">
        ⚠️ STEP LOSS / STALL DETECTED! (Motor slipped synchronization)
      </div>

      <!-- Speed / Step Interval Slider & RPM (Up to 2500 RPM for Step-Loss Benchmarking) -->
      <div class="slider-group">
        <div class="slider-header">
          <span>Target Speed (10 - 2500 RPM Max Test Range)</span>
          <span id="lbl-interval">60 RPM (312 &micro;s)</span>
        </div>
        <input type="range" id="rng-speed" min="10" max="2500" step="10" value="60" oninput="onSpeedSlider(this.value)">
      </div>

      <!-- Quick Speed Presets -->
      <div class="pill-grid" style="justify-content: center;">
        <button class="pill-btn" onclick="sendAction('rpm', '&val=30')">30</button>
        <button class="pill-btn" onclick="sendAction('rpm', '&val=60')">60</button>
        <button class="pill-btn" onclick="sendAction('rpm', '&val=150')">150</button>
        <button class="pill-btn" onclick="sendAction('rpm', '&val=300')">300</button>
        <button class="pill-btn" onclick="sendAction('rpm', '&val=600')">600</button>
        <button class="pill-btn" onclick="sendAction('rpm', '&val=900')">900</button>
        <button class="pill-btn" onclick="sendAction('rpm', '&val=1200')">1200</button>
        <button class="pill-btn" onclick="sendAction('rpm', '&val=1500')">1500</button>
        <button class="pill-btn" onclick="sendAction('rpm', '&val=2000')">2000 RPM</button>
      </div>

      <!-- Fine Step Jog to Pinpoint Exact Step Loss Speed -->
      <div class="btn-grid" style="grid-template-columns: repeat(4, 1fr);">
        <button class="btn btn-secondary" onclick="sendAction('rpm_delta', '&val=-50')">&#8722;50 RPM</button>
        <button class="btn btn-secondary" onclick="sendAction('rpm_delta', '&val=-10')">&#8722;10 RPM</button>
        <button class="btn btn-secondary" onclick="sendAction('rpm_delta', '&val=10')">&#43;10 RPM</button>
        <button class="btn btn-secondary" onclick="sendAction('rpm_delta', '&val=50')">&#43;50 RPM</button>
      </div>
    </div>

    <!-- Driver Tuning & Configuration Panel -->
    <div class="panel-card">
      <div class="panel-title">
        <h2>Driver Tuning & Diagnostics</h2>
        <span class="metric-pill" id="lbl-driver-state">ENABLED</span>
      </div>

      <!-- Chopper Mode Toggle -->
      <div class="toggle-row">
        <div class="toggle-label">
          <span class="toggle-title">Chopper Mode</span>
          <span class="toggle-desc" id="lbl-chopper-desc">StealthChop2 (Ultra-Silent)</span>
        </div>
        <button class="btn btn-secondary" style="padding: 0.5rem 0.9rem;" onclick="sendAction('toggle_chopper')">
          Toggle Chopper
        </button>
      </div>

      <!-- Microstepping Selector -->
      <div class="slider-group">
        <div class="slider-header">
          <span>Microstepping Resolution</span>
          <span id="lbl-microstep-cur">1/16</span>
        </div>
        <div class="pill-grid" id="microstep-pills">
          <button class="pill-btn" onclick="sendAction('microsteps', '&val=1')">1</button>
          <button class="pill-btn" onclick="sendAction('microsteps', '&val=2')">2</button>
          <button class="pill-btn" onclick="sendAction('microsteps', '&val=4')">4</button>
          <button class="pill-btn" onclick="sendAction('microsteps', '&val=8')">8</button>
          <button class="pill-btn active" onclick="sendAction('microsteps', '&val=16')">16</button>
          <button class="pill-btn" onclick="sendAction('microsteps', '&val=32')">32</button>
          <button class="pill-btn" onclick="sendAction('microsteps', '&val=64')">64</button>
          <button class="pill-btn" onclick="sendAction('microsteps', '&val=128')">128</button>
          <button class="pill-btn" onclick="sendAction('microsteps', '&val=256')">256</button>
        </div>
      </div>

      <!-- RMS Current Selector -->
      <div class="slider-group">
        <div class="slider-header">
          <span>RMS Motor Current</span>
          <span id="lbl-current-cur">600 mA</span>
        </div>
        <div class="pill-grid" id="current-pills">
          <button class="pill-btn" onclick="sendAction('current', '&val=300')">300mA</button>
          <button class="pill-btn" onclick="sendAction('current', '&val=500')">500mA</button>
          <button class="pill-btn active" onclick="sendAction('current', '&val=600')">600mA</button>
          <button class="pill-btn" onclick="sendAction('current', '&val=700')">700mA</button>
          <button class="pill-btn" onclick="sendAction('current', '&val=900')">900mA</button>
          <button class="pill-btn" onclick="sendAction('current', '&val=1100')">1100mA</button>
        </div>
      </div>

      <!-- Diagnostic Status Table -->
      <table class="diag-table">
        <tr><td>Silicon Version</td><td id="diag-ver">0x21 [TMC2209]</td></tr>
        <tr><td>UART Link</td><td id="diag-uart">CONNECTED</td></tr>
        <tr><td>Coil Target / Actual</td><td id="diag-coil">-- / --</td></tr>
        <tr><td>StallGuard / Stall</td><td id="diag-sg-row">--</td></tr>
        <tr><td>Thermal Status</td><td id="diag-thermal">NORMAL</td></tr>
        <tr><td>Standstill Status</td><td id="diag-standstill">STANDSTILL</td></tr>
        <tr><td>System Uptime</td><td id="diag-uptime">-- s</td></tr>
      </table>
    </div>

  </main>

  <footer>
    ESP32 TMC2209 Stepper Controller & INA226 Power Monitor &bull; Built with PlatformIO
  </footer>

</div>

<script>
let sliderDebounce = null;

async function fetchStatus() {
  try {
    const res = await fetch('/api/status');
    if (!res.ok) return;
    const data = await res.json();
    renderUI(data);
  } catch (err) {
    document.getElementById('badge-status').textContent = 'OFFLINE';
    document.getElementById('badge-status').parentElement.classList.remove('badge-online');
  }
}

function renderUI(data) {
  // Network badges
  document.getElementById('badge-status').textContent = 'ONLINE';
  document.getElementById('badge-status').parentElement.classList.add('badge-online');
  if (data.wifi) {
    document.getElementById('badge-ip').textContent = 'IP: ' + data.wifi.ip;
    document.getElementById('badge-rssi').textContent = 'RSSI: ' + data.wifi.rssi + ' dBm';
  }

  // INA226 Telemetry
  if (data.power) {
    document.getElementById('val-vbus').textContent = data.power.bus_voltage_v.toFixed(2);
    document.getElementById('val-current').textContent = data.power.current_ma.toFixed(1);
    document.getElementById('val-power').textContent = data.power.power_w.toFixed(2);
    document.getElementById('val-vshunt').textContent = data.power.shunt_voltage_mv.toFixed(2);
  }

  // Stepper State
  if (data.motor) {
    const m = data.motor;
    document.getElementById('lbl-steps-sec').textContent = m.steps_per_sec.toFixed(0) + ' steps/s (' + (m.rpm || 0).toFixed(0) + ' RPM)';
    document.getElementById('lbl-interval').textContent = (m.rpm || 0).toFixed(0) + ' RPM (' + m.step_interval_us + ' \u00B5s)';
    
    // Shaft animation
    const anim = document.getElementById('shaft-anim');
    anim.classList.remove('spin-fwd', 'spin-rev');
    if (m.mode === 1) anim.classList.add('spin-fwd');
    else if (m.mode === 2) anim.classList.add('spin-rev');
    else if (m.mode === 4) anim.classList.add('spin-fwd'); // Auto demo

    // Driver enable & chopper
    document.getElementById('lbl-driver-state').textContent = m.enabled ? 'ENABLED' : 'DISABLED';
    document.getElementById('lbl-chopper-desc').textContent = m.spread_cycle ? 'SpreadCycle (High Torque)' : 'StealthChop2 (Ultra-Silent)';

    // Microsteps
    document.getElementById('lbl-microstep-cur').textContent = '1/' + m.microsteps;
    document.querySelectorAll('#microstep-pills .pill-btn').forEach(btn => {
      btn.classList.toggle('active', parseInt(btn.textContent) === m.microsteps);
    });

    // Current
    document.getElementById('lbl-current-cur').textContent = m.rms_current_ma + ' mA';
    document.querySelectorAll('#current-pills .pill-btn').forEach(btn => {
      btn.classList.toggle('active', parseInt(btn.textContent) === m.rms_current_ma);
    });

    // Slider sync if not dragging
    if (!sliderDebounce) {
      document.getElementById('rng-speed').value = Math.min(2500, Math.max(10, Math.round(m.rpm || 60)));
    }

    // StallGuard & Lost Step state
    const isStalled = m.stall_detected;
    const sgVal = m.sg_result !== undefined ? m.sg_result : 0;
    document.getElementById('val-sg').textContent = sgVal;

    const stallBox = document.getElementById('stall-alert-box');
    const stallPill = document.getElementById('lbl-stall-state');
    if (isStalled) {
      stallBox.style.display = 'block';
      stallBox.innerHTML = '⚠️ STEP LOSS / STALL DETECTED at ' + (m.rpm || 0).toFixed(0) + ' RPM! (Rotor lost synchronization)';
      stallPill.textContent = 'STALL DETECTED';
      stallPill.style.color = '#f43f5e';
      stallPill.style.backgroundColor = 'rgba(244,63,94,0.2)';
    } else {
      stallBox.style.display = 'none';
      if (sgVal > 100) {
        stallPill.textContent = 'STRONG TORQUE';
        stallPill.style.color = '#34d399';
        stallPill.style.backgroundColor = 'rgba(16,185,129,0.15)';
      } else if (sgVal > 0) {
        stallPill.textContent = 'HIGH LOAD';
        stallPill.style.color = '#fbbf24';
        stallPill.style.backgroundColor = 'rgba(245,158,11,0.15)';
      } else {
        stallPill.textContent = m.mode === 0 ? 'STANDSTILL' : 'LOW MARGIN';
        stallPill.style.color = '#9ca3af';
        stallPill.style.backgroundColor = 'rgba(255,255,255,0.06)';
      }
    }
  }

  // Diagnostics
  if (data.diag) {
    document.getElementById('diag-ver').textContent = '0x' + data.diag.version.toString(16).toUpperCase() + ' [TMC2209]';
    document.getElementById('diag-uart').textContent = data.diag.uart_ok ? 'CONNECTED' : 'DISCONNECTED';
    if (data.motor) {
      document.getElementById('diag-coil').textContent = data.motor.rms_current_ma + 'mA / ' + (data.diag.reg_rms_ma || data.motor.rms_current_ma) + 'mA (CS ' + (data.diag.cs_actual || '--') + '/31)';
      document.getElementById('diag-sg-row').textContent = 'SG ' + (data.motor.sg_result || 0) + ' (' + (data.motor.stall_detected ? 'STALLED' : 'OK') + ')';
    }
    document.getElementById('diag-thermal').textContent = data.diag.ot ? 'SHUTDOWN' : (data.diag.otpw ? 'PRE-WARNING' : 'NORMAL');
    document.getElementById('diag-standstill').textContent = data.diag.standstill ? 'STANDSTILL' : 'RUNNING';
  }
  if (data.uptime_s !== undefined) {
    document.getElementById('diag-uptime').textContent = data.uptime_s + ' s';
  }
}

async function sendAction(action, extra = '') {
  try {
    const res = await fetch('/api/control?action=' + action + extra, { method: 'POST' });
    if (res.ok) {
      const data = await res.json();
      renderUI(data);
    }
  } catch (e) {
    console.error('Action error', e);
  }
}

function onSpeedSlider(val) {
  document.getElementById('lbl-interval').textContent = val + ' RPM';
  clearTimeout(sliderDebounce);
  sliderDebounce = setTimeout(() => {
    sliderDebounce = null;
    sendAction('rpm', '&val=' + val);
  }, 150);
}

// Start live polling every 500ms
setInterval(fetchStatus, 500);
fetchStatus();
</script>
</body>
</html>
)rawliteral";

// ======================= IMPLEMENTATION =======================
StepperWebUI::StepperWebUI(TMC2209Controller& stepper, INA226Monitor& powerMonitor, WiFiService& wifiService)
    : _stepper(stepper), _powerMonitor(powerMonitor), _wifiService(wifiService), _server(80) {
}

StepperWebUI::~StepperWebUI() {
}

bool StepperWebUI::begin() {
    return begin(Config());
}

bool StepperWebUI::begin(const Config& config) {
    _config = config;

    // Register HTTP endpoints
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/api/status", HTTP_GET, [this]() { handleApiStatus(); });
    _server.on("/api/control", HTTP_ANY, [this]() { handleApiControl(); });
    _server.onNotFound([this]() { handleNotFound(); });

    _server.begin(_config.port);
    _running = true;

    Serial.printf("[INIT] SUCCESS: Stepper WebUI server started on port %u\n", _config.port);
    if (_wifiService.isConnected()) {
        Serial.printf("[INIT] Open dashboard in browser: http://%s/\n",
                      _wifiService.getIP().toString().c_str());
    }
    return true;
}

void StepperWebUI::update() {
    if (_running) {
        _server.handleClient();
    }
}

void StepperWebUI::handleRoot() {
    _server.send_P(200, "text/html", INDEX_HTML);
}

void StepperWebUI::handleApiStatus() {
    // 1. Motor state
    uint8_t mode = (uint8_t)_stepper.getMode();
    const char* modeStr = "STOPPED";
    switch (mode) {
        case TMC2209Controller::MODE_CONTINUOUS_FWD: modeStr = "FORWARD"; break;
        case TMC2209Controller::MODE_CONTINUOUS_REV: modeStr = "REVERSE"; break;
        case TMC2209Controller::MODE_TARGET_STEPS:   modeStr = "TARGET STEPS"; break;
        case TMC2209Controller::MODE_AUTO_DEMO:      modeStr = "AUTO DEMO"; break;
        default: modeStr = "STOPPED"; break;
    }

    // 2. INA226 Telemetry (Cached 250ms)
    INA226Monitor::Telemetry p = _powerMonitor.read();

    // 3. Driver Diagnostics (Cached 250ms, zero loop jitter)
    TMC2209Controller::DiagTelemetry diag = _stepper.getCachedDiag();
    uint16_t regRms = _stepper.getRmsCurrent();
    uint8_t sgThrs = _stepper.getStallThreshold();

    // Build JSON buffer
    char json[900];
    snprintf(json, sizeof(json),
        "{"
        "\"motor\":{"
            "\"mode\":%u,"
            "\"mode_str\":\"%s\","
            "\"enabled\":%s,"
            "\"microsteps\":%u,"
            "\"rms_current_ma\":%u,"
            "\"step_interval_us\":%u,"
            "\"steps_per_sec\":%.1f,"
            "\"rpm\":%.1f,"
            "\"spread_cycle\":%s,"
            "\"sg_result\":%u,"
            "\"stall_detected\":%s,"
            "\"sg_threshold\":%u"
        "},"
        "\"power\":{"
            "\"valid\":%s,"
            "\"bus_voltage_v\":%.2f,"
            "\"shunt_voltage_mv\":%.2f,"
            "\"current_ma\":%.1f,"
            "\"power_w\":%.2f"
        "},"
        "\"diag\":{"
            "\"version\":%u,"
            "\"uart_ok\":%s,"
            "\"reg_rms_ma\":%u,"
            "\"cs_actual\":%u,"
            "\"ot\":%s,"
            "\"otpw\":%s,"
            "\"standstill\":%s"
        "},"
        "\"wifi\":{"
            "\"connected\":%s,"
            "\"ip\":\"%s\","
            "\"rssi\":%d"
        "},"
        "\"uptime_s\":%lu"
        "}",
        mode,
        modeStr,
        _stepper.isEnabled() ? "true" : "false",
        _stepper.getMicrosteps(),
        _stepper.getRmsCurrent(),
        _stepper.getStepInterval(),
        _stepper.getStepsPerSecond(),
        _stepper.getRPM(),
        _stepper.isSpreadCycle() ? "true" : "false",
        diag.sg,
        diag.stall ? "true" : "false",
        sgThrs,
        p.valid ? "true" : "false",
        p.busVoltage_V,
        p.shuntVoltage_mV,
        p.current_mA,
        p.power_W,
        diag.version,
        diag.uartOk ? "true" : "false",
        regRms,
        diag.cs,
        diag.ot ? "true" : "false",
        diag.otpw ? "true" : "false",
        diag.stst ? "true" : "false",
        _wifiService.isConnected() ? "true" : "false",
        _wifiService.getIP().toString().c_str(),
        _wifiService.getRSSI(),
        millis() / 1000
    );

    _server.send(200, "application/json", json);
}

void StepperWebUI::handleApiControl() {
    if (_server.hasArg("action")) {
        String action = _server.arg("action");

        if (action == "forward") {
            _stepper.moveForward();
        } else if (action == "reverse") {
            _stepper.moveReverse();
        } else if (action == "stop") {
            _stepper.stop();
        } else if (action == "revs") {
            float revs = _server.hasArg("val") ? _server.arg("val").toFloat() : 1.0f;
            bool fwd = (!_server.hasArg("dir") || _server.arg("dir") == "fwd");
            _stepper.moveRevolutions(revs, fwd);
        } else if (action == "autodemo") {
            _stepper.toggleAutoDemo();
        } else if (action == "toggle_enable") {
            _stepper.toggleEnable();
        } else if (action == "toggle_chopper") {
            _stepper.toggleSpreadCycle();
        } else if (action == "microsteps") {
            if (_server.hasArg("val")) {
                _stepper.setMicrosteps(_server.arg("val").toInt());
            } else {
                _stepper.cycleMicrosteps();
            }
        } else if (action == "current") {
            if (_server.hasArg("val")) {
                _stepper.setRmsCurrent(_server.arg("val").toInt());
            } else {
                _stepper.cycleRmsCurrent();
            }
        } else if (action == "interval") {
            if (_server.hasArg("val")) {
                _stepper.setStepInterval(_server.arg("val").toInt());
            }
        } else if (action == "rpm") {
            if (_server.hasArg("val")) {
                _stepper.setRPM(_server.arg("val").toFloat());
            }
        } else if (action == "rpm_delta") {
            if (_server.hasArg("val")) {
                float delta = _server.arg("val").toFloat();
                float newRpm = _stepper.getRPM() + delta;
                if (newRpm < 10.0f) newRpm = 10.0f;
                if (newRpm > 2500.0f) newRpm = 2500.0f;
                _stepper.setRPM(newRpm);
            }
        } else if (action == "speed_inc") {
            _stepper.setRPM(min(2500.0f, _stepper.getRPM() + 25.0f));
        } else if (action == "speed_dec") {
            _stepper.setRPM(max(10.0f, _stepper.getRPM() - 25.0f));
        } else if (action == "sgthrs") {
            if (_server.hasArg("val")) {
                _stepper.setStallThreshold(_server.arg("val").toInt());
            }
        }
    }

    // Immediately return updated status
    handleApiStatus();
}

void StepperWebUI::handleNotFound() {
    _server.send(404, "text/plain", "404: Not Found");
}
