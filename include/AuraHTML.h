#pragma once

const char AURA_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AURA - Autonomous Universal Reactive Agent</title>
<style>
  :root {
    --bg: #0d1117; --bg2: #161b22; --bg3: #21262d;
    --border: #30363d; --accent: #58a6ff; --accent2: #3fb950;
    --warn: #d29922; --err: #f85149; --text: #e6edf3; --muted: #8b949e;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: var(--bg); color: var(--text); font-family: 'Segoe UI', system-ui, sans-serif; min-height: 100vh; }
  
  .header { background: var(--bg2); border-bottom: 1px solid var(--border); padding: 16px 24px; display: flex; align-items: center; gap: 16px; }
  .logo { font-size: 24px; font-weight: 700; color: var(--accent); letter-spacing: 4px; }
  .logo span { color: var(--accent2); }
  .version { font-size: 12px; color: var(--muted); background: var(--bg3); padding: 2px 8px; border-radius: 20px; }
  .status-bar { margin-left: auto; display: flex; gap: 16px; align-items: center; }
  .status-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--err); animation: pulse 2s infinite; }
  .status-dot.online { background: var(--accent2); }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.4} }
  
  .tabs { display: flex; background: var(--bg2); border-bottom: 1px solid var(--border); padding: 0 24px; overflow-x: auto; }
  .tab { padding: 14px 20px; cursor: pointer; border-bottom: 2px solid transparent; color: var(--muted); font-size: 14px; white-space: nowrap; transition: all .2s; }
  .tab:hover { color: var(--text); }
  .tab.active { color: var(--accent); border-bottom-color: var(--accent); }
  
  .content { max-width: 900px; margin: 0 auto; padding: 24px; }
  .panel { display: none; }
  .panel.active { display: block; }
  
  h2 { font-size: 18px; margin-bottom: 20px; color: var(--text); }
  h3 { font-size: 14px; color: var(--muted); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 16px; }
  
  .card { background: var(--bg2); border: 1px solid var(--border); border-radius: 8px; padding: 20px; margin-bottom: 16px; }
  .card-title { font-size: 16px; font-weight: 600; margin-bottom: 16px; display: flex; align-items: center; gap: 8px; }
  .card-title .icon { font-size: 18px; }
  
  .form-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
  .form-group { display: flex; flex-direction: column; gap: 6px; }
  .form-group.full { grid-column: 1 / -1; }
  label { font-size: 13px; color: var(--muted); }
  input, select, textarea {
    background: var(--bg3); border: 1px solid var(--border); border-radius: 6px;
    color: var(--text); padding: 8px 12px; font-size: 14px; width: 100%;
    transition: border-color .2s;
  }
  input:focus, select:focus, textarea:focus { outline: none; border-color: var(--accent); }
  textarea { resize: vertical; min-height: 80px; font-family: inherit; }
  
  .btn { padding: 8px 16px; border-radius: 6px; border: none; cursor: pointer; font-size: 14px; font-weight: 500; transition: all .2s; }
  .btn-primary { background: var(--accent); color: #0d1117; }
  .btn-primary:hover { opacity: .85; }
  .btn-success { background: var(--accent2); color: #0d1117; }
  .btn-danger  { background: var(--err); color: white; }
  .btn-ghost   { background: transparent; color: var(--muted); border: 1px solid var(--border); }
  .btn-ghost:hover { background: var(--bg3); color: var(--text); }
  .btn-sm { padding: 4px 10px; font-size: 12px; }
  
  .badge { display: inline-block; padding: 2px 8px; border-radius: 20px; font-size: 11px; font-weight: 600; }
  .badge-green { background: rgba(63,185,80,.2); color: var(--accent2); }
  .badge-blue  { background: rgba(88,166,255,.2); color: var(--accent); }
  .badge-red   { background: rgba(248,81,73,.2); color: var(--err); }
  
  .sensor-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 12px; }
  .sensor-card { background: var(--bg3); border: 1px solid var(--border); border-radius: 8px; padding: 16px; }
  .sensor-name { font-size: 12px; color: var(--muted); margin-bottom: 4px; }
  .sensor-value { font-size: 28px; font-weight: 700; color: var(--accent); }
  .sensor-unit  { font-size: 14px; color: var(--muted); }
  
  .chat-container { display: flex; flex-direction: column; height: 60vh; }
  .chat-messages { flex: 1; overflow-y: auto; padding: 16px; background: var(--bg3); border-radius: 8px 8px 0 0; border: 1px solid var(--border); }
  .chat-input-row { display: flex; gap: 8px; padding: 12px; background: var(--bg2); border: 1px solid var(--border); border-top: none; border-radius: 0 0 8px 8px; }
  .chat-input { flex: 1; }
  .msg { margin-bottom: 12px; }
  .msg-user { text-align: right; }
  .msg-bubble { display: inline-block; padding: 8px 12px; border-radius: 8px; max-width: 80%; font-size: 14px; line-height: 1.5; }
  .msg-user .msg-bubble  { background: var(--accent); color: #0d1117; border-radius: 8px 8px 2px 8px; }
  .msg-aura .msg-bubble  { background: var(--bg2); border: 1px solid var(--border); border-radius: 8px 8px 8px 2px; }
  .msg-name { font-size: 11px; color: var(--muted); margin-bottom: 4px; }
  
  .actuator-row { display: flex; align-items: center; gap: 12px; padding: 12px; background: var(--bg3); border-radius: 6px; margin-bottom: 8px; }
  .actuator-name { flex: 1; font-size: 14px; }
  .toggle { position: relative; width: 44px; height: 24px; }
  .toggle input { opacity: 0; width: 0; height: 0; }
  .slider { position: absolute; inset: 0; background: var(--border); border-radius: 24px; cursor: pointer; transition: .2s; }
  .slider:before { content: ""; position: absolute; width: 18px; height: 18px; left: 3px; top: 3px; background: white; border-radius: 50%; transition: .2s; }
  input:checked + .slider { background: var(--accent2); }
  input:checked + .slider:before { transform: translateX(20px); }
  
  .io-row { display: grid; grid-template-columns: 2fr 1fr 1fr 80px 40px; gap: 8px; align-items: center; margin-bottom: 8px; }
  .io-row.header { font-size: 12px; color: var(--muted); margin-bottom: 4px; }
  
  .alert { padding: 10px 14px; border-radius: 6px; font-size: 13px; margin-bottom: 16px; }
  .alert-info    { background: rgba(88,166,255,.1); border: 1px solid rgba(88,166,255,.3); color: var(--accent); }
  .alert-success { background: rgba(63,185,80,.1); border: 1px solid rgba(63,185,80,.3); color: var(--accent2); }
  .alert-warning { background: rgba(210,153,34,.1); border: 1px solid rgba(210,153,34,.3); color: var(--warn); }
  
  .sysinfo { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
  .sysinfo-item { background: var(--bg3); border-radius: 6px; padding: 12px 16px; }
  .sysinfo-label { font-size: 11px; color: var(--muted); text-transform: uppercase; letter-spacing: 1px; }
  .sysinfo-val   { font-size: 20px; font-weight: 600; color: var(--text); margin-top: 4px; }
  
  .provider-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(130px, 1fr)); gap: 10px; margin-bottom: 16px; }
  .provider-btn { padding: 10px; border: 2px solid var(--border); border-radius: 8px; cursor: pointer; text-align: center; font-size: 13px; transition: all .2s; background: var(--bg3); color: var(--text); }
  .provider-btn:hover { border-color: var(--accent); }
  .provider-btn.active { border-color: var(--accent); background: rgba(88,166,255,.1); color: var(--accent); }
  .provider-icon { font-size: 24px; display: block; margin-bottom: 4px; }
  
  @media (max-width: 600px) {
    .form-grid { grid-template-columns: 1fr; }
    .io-row { grid-template-columns: 1fr 1fr; }
  }
</style>
</head>
<body>

<div class="header">
  <div class="logo">A<span>U</span>RA</div>
  <div class="version" id="version">v1.0.0</div>
  <div class="status-bar">
    <span id="ip-label" style="font-size:13px;color:var(--muted)">--</span>
    <div class="status-dot" id="status-dot"></div>
    <span id="status-text" style="font-size:13px">Connecting...</span>
  </div>
</div>

<div class="tabs">
  <div class="tab active" onclick="showTab('dashboard')">📊 Dashboard</div>
  <div class="tab" onclick="showTab('chat')">💬 Chat</div>
  <div class="tab" onclick="showTab('io')">🔌 I/O Config</div>
  <div class="tab" onclick="showTab('llm')">🤖 LLM</div>
  <div class="tab" onclick="showTab('telegram')">✈️ Telegram</div>
  <div class="tab" onclick="showTab('wifi')">📶 WiFi</div>
  <div class="tab" onclick="showTab('system')">⚙️ System</div>
</div>

<!-- ────────────────── DASHBOARD ────────────────── -->
<div id="panel-dashboard" class="content panel active">
  <h2>Live Dashboard</h2>
  <div id="sensor-cards" class="sensor-grid">
    <div class="sensor-card"><div class="sensor-name">Loading...</div><div class="sensor-value">--</div></div>
  </div>
  <div style="height:24px"></div>
  <h2>Actuators</h2>
  <div id="actuator-controls"></div>
</div>

<!-- ────────────────── CHAT ────────────────── -->
<div id="panel-chat" class="content panel">
  <h2>Chat with AURA</h2>
  <div class="chat-container">
    <div class="chat-messages" id="chat-messages">
      <div class="msg msg-aura">
        <div class="msg-name">AURA</div>
        <div class="msg-bubble">Hello! I'm AURA. I can read your sensors, control your actuators, and answer questions. How can I help?</div>
      </div>
    </div>
    <div class="chat-input-row">
      <input type="text" class="chat-input" id="chat-input" placeholder="Ask AURA anything..." onkeydown="if(event.key==='Enter')sendChat()">
      <button class="btn btn-primary" onclick="sendChat()">Send</button>
      <button class="btn btn-ghost btn-sm" onclick="clearChat()" title="Clear history">🗑</button>
    </div>
  </div>
</div>

<!-- ────────────────── I/O CONFIG ────────────────── -->
<div id="panel-io" class="content panel">
  <h2>Sensors & Actuators</h2>
  
  <div class="card">
    <div class="card-title"><span class="icon">📡</span> Sensors (up to 8)</div>
    <div class="io-row header">
      <span>Name</span><span>Type</span><span>Pin / Addr</span><span>Unit</span><span></span>
    </div>
    <div id="sensor-config-rows"></div>
    <button class="btn btn-ghost btn-sm" onclick="scanI2C()">🔍 Scan I2C Bus</button>
    <span id="i2c-result" style="font-size:12px;color:var(--muted);margin-left:12px"></span>
  </div>

  <div class="card">
    <div class="card-title"><span class="icon">⚡</span> Actuators (up to 8)</div>
    <div class="io-row header">
      <span>Name</span><span>Type</span><span>Pin</span><span></span><span></span>
    </div>
    <div id="actuator-config-rows"></div>
  </div>

  <button class="btn btn-success" onclick="saveIO()">💾 Save I/O Configuration</button>
</div>

<!-- ────────────────── LLM CONFIG ────────────────── -->
<div id="panel-llm" class="content panel">
  <h2>LLM Configuration</h2>
  <div class="card">
    <div class="card-title"><span class="icon">🤖</span> Provider</div>
    <div class="provider-grid">
      <div class="provider-btn active" onclick="selectProvider('openai')"    id="prov-openai">    <span class="provider-icon">🟢</span>OpenAI</div>
      <div class="provider-btn"        onclick="selectProvider('gemini')"    id="prov-gemini">    <span class="provider-icon">💎</span>Gemini</div>
      <div class="provider-btn"        onclick="selectProvider('anthropic')" id="prov-anthropic"> <span class="provider-icon">🟠</span>Anthropic</div>
      <div class="provider-btn"        onclick="selectProvider('groq')"      id="prov-groq">      <span class="provider-icon">⚡</span>Groq</div>
      <div class="provider-btn"        onclick="selectProvider('openrouter')"id="prov-openrouter"><span class="provider-icon">🌐</span>OpenRouter</div>
      <div class="provider-btn"        onclick="selectProvider('ollama')"    id="prov-ollama">    <span class="provider-icon">🦙</span>Ollama</div>
    </div>

    <div class="form-grid">
      <div class="form-group">
        <label>API Key</label>
        <input type="password" id="llm_api_key" placeholder="sk-...">
      </div>
      <div class="form-group">
        <label>Model</label>
        <input type="text" id="llm_model" placeholder="gpt-4o-mini">
      </div>
      <div class="form-group">
        <label>Base URL (optional / Ollama)</label>
        <input type="text" id="llm_base_url" placeholder="http://192.168.1.100:11434">
      </div>
      <div class="form-group">
        <label>Max Tokens</label>
        <input type="number" id="llm_max_tokens" value="512" min="64" max="4096">
      </div>
      <div class="form-group">
        <label>Temperature (0.0 – 2.0)</label>
        <input type="number" id="llm_temperature" value="0.7" min="0" max="2" step="0.1">
      </div>
      <div class="form-group full">
        <label>System Prompt / Agent Personality</label>
        <textarea id="system_prompt" rows="4">You are AURA, an intelligent IoT assistant. You can read sensors and control actuators. Be concise and helpful.</textarea>
      </div>
    </div>
    <br>
    <button class="btn btn-primary" onclick="saveLLM()">💾 Save LLM Settings</button>
  </div>

  <div class="alert alert-info">
    💡 <strong>Tip:</strong> Groq provides very fast free inference. OpenRouter gives access to 200+ models with a single API key.
  </div>
</div>

<!-- ────────────────── TELEGRAM ────────────────── -->
<div id="panel-telegram" class="content panel">
  <h2>Telegram Bot</h2>
  <div class="card">
    <div class="card-title"><span class="icon">✈️</span> Bot Settings</div>
    <div class="alert alert-info" style="margin-bottom:16px">
      Create a bot via <strong>@BotFather</strong> on Telegram. Send <code>/newbot</code>, get your token, then add it here.
    </div>
    <div class="form-grid">
      <div class="form-group full">
        <label>Bot Token (from BotFather)</label>
        <input type="password" id="telegram_token" placeholder="1234567890:AAEXAMPLE...">
      </div>
      <div class="form-group full">
        <label>Allowed Chat ID (leave blank to allow all, or enter your chat ID to restrict)</label>
        <input type="text" id="telegram_chat_id" placeholder="Your Telegram user/group ID">
      </div>
    </div>
    <br>
    <div style="display:flex;gap:8px;flex-wrap:wrap">
      <button class="btn btn-primary" onclick="saveTelegram()">💾 Save & Connect</button>
      <button class="btn btn-ghost"   onclick="getChatId()">🆔 How to get my Chat ID</button>
    </div>
  </div>
  <div id="telegram-help" style="display:none" class="card">
    <div class="card-title">Getting Your Chat ID</div>
    <ol style="padding-left:20px;line-height:2;font-size:14px;color:var(--muted)">
      <li>Add your bot to Telegram (search for its username)</li>
      <li>Send it any message like <code>/start</code></li>
      <li>Visit: <code>https://api.telegram.org/bot&lt;TOKEN&gt;/getUpdates</code></li>
      <li>Look for <code>"chat":{"id": 123456789}</code> — that number is your Chat ID</li>
    </ol>
  </div>
</div>

<!-- ────────────────── WIFI ────────────────── -->
<div id="panel-wifi" class="content panel">
  <h2>WiFi Settings</h2>
  <div class="card">
    <div class="card-title"><span class="icon">📶</span> Network</div>
    <div class="alert alert-warning">
      Changing WiFi settings will restart AURA to apply the new connection.
    </div>
    <div class="form-grid">
      <div class="form-group">
        <label>SSID (Network Name)</label>
        <input type="text" id="wifi_ssid" placeholder="YourWiFiName">
      </div>
      <div class="form-group">
        <label>Password</label>
        <input type="password" id="wifi_pass" placeholder="YourWiFiPassword">
      </div>
    </div>
    <br>
    <button class="btn btn-primary" onclick="saveWiFi()">💾 Save & Restart</button>
  </div>
</div>

<!-- ────────────────── SYSTEM ────────────────── -->
<div id="panel-system" class="content panel">
  <h2>System</h2>
  <div class="sysinfo" id="sysinfo">
    <div class="sysinfo-item"><div class="sysinfo-label">Version</div><div class="sysinfo-val" id="si-version">--</div></div>
    <div class="sysinfo-item"><div class="sysinfo-label">Free Heap</div><div class="sysinfo-val" id="si-heap">--</div></div>
    <div class="sysinfo-item"><div class="sysinfo-label">Uptime</div><div class="sysinfo-val" id="si-uptime">--</div></div>
    <div class="sysinfo-item"><div class="sysinfo-label">WiFi RSSI</div><div class="sysinfo-val" id="si-rssi">--</div></div>
  </div>
  <br>
  <div style="display:flex;gap:8px">
    <button class="btn btn-primary" onclick="loadSysInfo()">🔄 Refresh</button>
    <button class="btn btn-danger"  onclick="restartAura()">♻️ Restart AURA</button>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);

// ── Tab Navigation ─────────────────────────────────────────────
function showTab(name) {
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  $('panel-' + name).classList.add('active');
  event.target.classList.add('active');
  if (name === 'dashboard') loadDashboard();
  if (name === 'system') loadSysInfo();
  if (name === 'io') loadIOConfig();
  if (name === 'llm') loadSettings();
  if (name === 'telegram') loadSettings();
  if (name === 'wifi') loadSettings();
}

// ── API Helpers ────────────────────────────────────────────────
async function api(url, method='GET', body=null) {
  const opts = { method, headers: {'Content-Type':'application/json'} };
  if (body) opts.body = JSON.stringify(body);
  const r = await fetch(url, opts);
  return r.json();
}

function toast(msg, ok=true) {
  const d = document.createElement('div');
  d.className = 'alert ' + (ok ? 'alert-success' : 'alert-warning');
  d.style.cssText = 'position:fixed;bottom:20px;right:20px;z-index:999;min-width:200px';
  d.textContent = msg;
  document.body.appendChild(d);
  setTimeout(() => d.remove(), 3000);
}

// ── Dashboard ──────────────────────────────────────────────────
async function loadDashboard() {
  const data = await api('/api/sensors');
  const sc = $('sensor-cards');
  const ac = $('actuator-controls');

  if (!data.sensors || data.sensors.length === 0) {
    sc.innerHTML = '<div class="sensor-card"><div class="sensor-name">No sensors configured</div><div class="sensor-value" style="font-size:14px">Go to I/O Config tab</div></div>';
  } else {
    sc.innerHTML = data.sensors.map(s => `
      <div class="sensor-card">
        <div class="sensor-name">${s.name}</div>
        <div class="sensor-value">${s.str} <span class="sensor-unit">${s.unit || ''}</span></div>
      </div>`).join('');
  }

  if (!data.actuators || data.actuators.length === 0) {
    ac.innerHTML = '<div style="color:var(--muted);font-size:14px">No actuators configured</div>';
  } else {
    ac.innerHTML = data.actuators.map((a,i) => `
      <div class="actuator-row">
        <div class="actuator-name">${a.name} <span class="badge ${a.state?'badge-green':'badge-red'}">${a.state?'ON':'OFF'}</span></div>
        ${a.type==='pwm' ? `<input type="range" min="0" max="255" value="${a.value}" style="flex:1" oninput="setPWM('${a.name}',this.value)">
          <span style="font-size:12px;width:30px">${a.value}</span>` : ''}
        <label class="toggle"><input type="checkbox" ${a.state?'checked':''} onchange="toggleActuator('${a.name}',this.checked)"><span class="slider"></span></label>
      </div>`).join('');
  }
}

async function toggleActuator(name, state) {
  await api('/api/actuator', 'POST', {name, state});
  loadDashboard();
}
async function setPWM(name, val) {
  await api('/api/actuator', 'POST', {name, state: val>0, pwm: parseInt(val)});
}

// ── Chat ───────────────────────────────────────────────────────
async function sendChat() {
  const input = $('chat-input');
  const msg = input.value.trim();
  if (!msg) return;
  input.value = '';

  appendChat('user', msg);
  appendChat('aura', '...');
  const msgs = $('chat-messages');
  const dots = msgs.lastChild;

  const data = await api('/api/chat', 'POST', {message: msg});
  dots.remove();
  appendChat('aura', data.reply || 'No response');
}

function appendChat(from, text) {
  const msgs = $('chat-messages');
  const div = document.createElement('div');
  div.className = 'msg msg-' + from;
  div.innerHTML = `<div class="msg-name">${from === 'user' ? 'You' : 'AURA'}</div>
    <div class="msg-bubble">${text}</div>`;
  msgs.appendChild(div);
  msgs.scrollTop = msgs.scrollHeight;
}

async function clearChat() {
  await api('/api/chat', 'POST', {message:'/clear'});
  $('chat-messages').innerHTML = '<div class="msg msg-aura"><div class="msg-name">AURA</div><div class="msg-bubble">History cleared. How can I help?</div></div>';
}

// ── I/O Config ─────────────────────────────────────────────────
const sensorTypes = ['analog','digital','i2c_raw','voltage','dht11','dht22','ds18b20','mock'];
const actuatorTypes = ['digital','pwm','i2c'];

function renderIORows(sensors, actuators) {
  const sr = $('sensor-config-rows');
  sr.innerHTML = Array.from({length:8}, (_,i) => {
    const s = (sensors && sensors[i]) ? sensors[i] : {};
    return `<div class="io-row">
      <input type="text" value="${s.name||''}" id="s-name-${i}" placeholder="e.g. Temperature">
      <select id="s-type-${i}">${sensorTypes.map(t=>`<option ${s.type===t?'selected':''}>${t}</option>`).join('')}</select>
      <input type="text" value="${s.pin>=0?s.pin:(s.i2c_addr||'')}" id="s-pin-${i}" placeholder="pin/addr">
      <input type="text" value="${s.unit||''}" id="s-unit-${i}" placeholder="°C">
      <label class="toggle"><input type="checkbox" id="s-en-${i}" ${s.enabled?'checked':''}><span class="slider"></span></label>
    </div>`;
  }).join('');

  const ar = $('actuator-config-rows');
  ar.innerHTML = Array.from({length:8}, (_,i) => {
    const a = (actuators && actuators[i]) ? actuators[i] : {};
    return `<div class="io-row">
      <input type="text" value="${a.name||''}" id="a-name-${i}" placeholder="e.g. LED">
      <select id="a-type-${i}">${actuatorTypes.map(t=>`<option ${a.type===t?'selected':''}>${t}</option>`).join('')}</select>
      <input type="number" value="${a.pin>=0?a.pin:''}" id="a-pin-${i}" placeholder="GPIO">
      <span></span>
      <label class="toggle"><input type="checkbox" id="a-en-${i}" ${a.enabled?'checked':''}><span class="slider"></span></label>
    </div>`;
  }).join('');
}

async function loadIOConfig() {
  const data = await api('/api/sensors');
  renderIORows(data.sensors || [], data.actuators || []);
}

async function saveIO() {
  const sensors = Array.from({length:8}, (_,i) => ({
    enabled: $(`s-en-${i}`).checked,
    name: $(`s-name-${i}`).value,
    type: $(`s-type-${i}`).value,
    pin: parseInt($(`s-pin-${i}`).value) || -1,
    i2c_addr: $(`s-pin-${i}`).value,
    unit: $(`s-unit-${i}`).value
  }));
  const actuators = Array.from({length:8}, (_,i) => ({
    enabled: $(`a-en-${i}`).checked,
    name: $(`a-name-${i}`).value,
    type: $(`a-type-${i}`).value,
    pin: parseInt($(`a-pin-${i}`).value) || -1
  }));
  const r = await api('/api/sensors', 'POST', {sensors, actuators});
  toast(r.ok ? '✅ I/O config saved!' : '❌ Failed to save', r.ok);
}

async function scanI2C() {
  $('i2c-result').textContent = 'Scanning...';
  const data = await api('/api/i2cscan');
  $('i2c-result').textContent = data.devices.length > 0
    ? 'Found: ' + data.devices.join(', ')
    : 'No devices found';
}

// ── Settings ───────────────────────────────────────────────────
async function loadSettings() {
  const s = await api('/api/settings');
  if ($('llm_model'))       $('llm_model').value       = s.llm_model || '';
  if ($('llm_api_key'))     $('llm_api_key').value     = s.llm_api_key || '';
  if ($('llm_base_url'))    $('llm_base_url').value    = s.llm_base_url || '';
  if ($('llm_max_tokens'))  $('llm_max_tokens').value  = s.llm_max_tokens || 512;
  if ($('llm_temperature')) $('llm_temperature').value = s.llm_temperature || 0.7;
  if ($('system_prompt'))   $('system_prompt').value   = s.system_prompt || '';
  if ($('telegram_token'))  $('telegram_token').value  = s.telegram_token || '';
  if ($('telegram_chat_id'))$('telegram_chat_id').value= s.telegram_chat_id || '';
  if ($('wifi_ssid'))       $('wifi_ssid').value       = s.wifi_ssid || '';
  if (s.llm_provider)       selectProvider(s.llm_provider, false);
}

function selectProvider(p, save=true) {
  document.querySelectorAll('.provider-btn').forEach(b => b.classList.remove('active'));
  const el = $('prov-' + p);
  if (el) el.classList.add('active');
  const models = {
    openai: 'gpt-4o-mini', gemini: 'gemini-1.5-flash',
    anthropic: 'claude-haiku-4-5-20251001', groq: 'llama-3.3-70b-versatile',
    openrouter: 'openai/gpt-4o-mini', ollama: 'llama3.2'
  };
  if (save && $('llm_model')) $('llm_model').value = models[p] || '';
}

async function saveLLM() {
  const provider = document.querySelector('.provider-btn.active')?.textContent.trim().toLowerCase() || 'openai';
  const r = await api('/api/settings', 'POST', {
    llm_provider:  provider,
    llm_model:     $('llm_model').value,
    llm_api_key:   $('llm_api_key').value,
    llm_base_url:  $('llm_base_url').value,
    llm_max_tokens:parseInt($('llm_max_tokens').value),
    llm_temperature:parseFloat($('llm_temperature').value),
    system_prompt: $('system_prompt').value
  });
  toast(r.ok ? '✅ LLM settings saved!' : '❌ ' + r.msg, r.ok);
}

async function saveTelegram() {
  const r = await api('/api/settings', 'POST', {
    telegram_token:   $('telegram_token').value,
    telegram_chat_id: $('telegram_chat_id').value
  });
  toast(r.ok ? '✅ Telegram settings saved! Restart to apply.' : '❌ Failed', r.ok);
}

function getChatId() { $('telegram-help').style.display = $('telegram-help').style.display === 'none' ? 'block' : 'none'; }

async function saveWiFi() {
  const r = await api('/api/settings', 'POST', {
    wifi_ssid: $('wifi_ssid').value,
    wifi_pass: $('wifi_pass').value
  });
  if (r.ok) {
    toast('✅ WiFi saved, restarting...');
    setTimeout(() => api('/api/restart', 'POST'), 1000);
  }
}

// ── System Info ────────────────────────────────────────────────
async function loadSysInfo() {
  const s = await api('/api/sysinfo');
  $('si-version').textContent = s.version || '--';
  $('si-heap').textContent    = s.heap ? (s.heap/1024).toFixed(1) + ' KB' : '--';
  $('si-uptime').textContent  = s.uptime_s ? formatUptime(s.uptime_s) : '--';
  $('si-rssi').textContent    = s.wifi_rssi ? s.wifi_rssi + ' dBm' : '--';
  $('version').textContent    = 'v' + (s.version || '?');
  $('ip-label').textContent   = s.ip || '';
  $('status-dot').className   = 'status-dot online';
  $('status-text').textContent= 'Online';
}

function formatUptime(s) {
  const h = Math.floor(s/3600), m = Math.floor((s%3600)/60);
  return h > 0 ? `${h}h ${m}m` : `${m}m ${s%60}s`;
}

async function restartAura() {
  if (!confirm('Restart AURA?')) return;
  await api('/api/restart', 'POST');
  toast('Restarting...');
}

// ── Init ────────────────────────────────────────────────────────
loadDashboard();
loadSysInfo();
setInterval(loadDashboard, 5000);
setInterval(loadSysInfo,   15000);
</script>
</body>
</html>
)=====";
