#pragma once

#include <Arduino.h>

/**
 * @file webui_html.h
 * @brief Self-contained SPA WebUI stored in Flash memory (PROGMEM).
 *
 * Fully offline, no external CDNs, dark mode, responsive for mobile & desktop.
 * Includes Lamp Controls, 10 Effect Selection Grid, Live Telemetry,
 * GitHub Silent OTA / Local Flash Upload, and Wi-Fi Configuration.
 */

const char WEBUI_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>MagicLamp ESP32-C6</title>
  <link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Cdefs%3E%3Cfilter id='g' x='-20%25' y='-20%25' width='140%25' height='140%25'%3E%3CfeGaussianBlur stdDeviation='1.5' result='b'/%3E%3CfeMerge%3E%3CfeMergeNode in='b'/%3E%3CfeMergeNode in='SourceGraphic'/%3E%3C/feMerge%3E%3C/filter%3E%3C/defs%3E%3Crect width='32' height='32' rx='6' fill='%23090d16'/%3E%3Cpath d='M16 4 C11 4 8 8 8 13 C8 17 11 20 12 22 L20 22 C21 20 24 17 24 13 C24 8 21 4 16 4 Z' fill='none' stroke='%2338bdf8' stroke-width='2' filter='url(%23g)'/%3E%3Cpath d='M13 13 Q16 7 19 13 Q16 19 13 13' fill='%23818cf8' opacity='0.8' filter='url(%23g)'/%3E%3Crect x='13' y='23' width='6' height='2' rx='1' fill='%2338bdf8'/%3E%3Crect x='14' y='26' width='4' height='2' rx='1' fill='%2394a3b8'/%3E%3C/svg%3E">
  <style>
    :root {
      --bg: #090d16;
      --card-bg: #131b2e;
      --card-border: #22304d;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --primary: #38bdf8;
      --primary-hover: #0ea5e9;
      --accent: #818cf8;
      --success: #10b981;
      --danger: #ef4444;
      --warning: #f59e0b;
      --radius: 12px;
      --shadow: 0 4px 20px -2px rgba(0, 0, 0, 0.5);
    }
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      -webkit-tap-highlight-color: transparent;
    }
    body {
      background-color: var(--bg);
      color: var(--text);
      min-height: 100vh;
      padding: 16px;
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    .container {
      width: 100%;
      max-width: 840px;
      display: flex;
      flex-direction: column;
      gap: 16px;
    }
    header {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: var(--radius);
      padding: 16px 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: var(--shadow);
    }
    .brand {
      display: flex;
      align-items: center;
      gap: 12px;
    }
    .logo-icon {
      width: 32px;
      height: 32px;
      border-radius: 8px;
      background: linear-gradient(135deg, #f59e0b, #ef4444, #8b5cf6);
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 18px;
      box-shadow: 0 0 12px rgba(245, 158, 11, 0.4);
    }
    h1 {
      font-size: 1.25rem;
      font-weight: 700;
      letter-spacing: -0.02em;
      background: linear-gradient(135deg, #38bdf8, #c084fc);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .status-badge {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      font-size: 0.8rem;
      padding: 4px 10px;
      border-radius: 9999px;
      background: rgba(16, 185, 129, 0.15);
      color: var(--success);
      border: 1px solid rgba(16, 185, 129, 0.3);
      font-weight: 500;
    }
    .status-badge.offline {
      background: rgba(239, 68, 68, 0.15);
      color: var(--danger);
      border-color: rgba(239, 68, 68, 0.3);
    }
    .dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: currentColor;
    }

    .card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: var(--radius);
      padding: 18px;
      box-shadow: var(--shadow);
    }
    .card-title {
      font-size: 1rem;
      font-weight: 600;
      color: var(--text);
      margin-bottom: 14px;
      display: flex;
      align-items: center;
      gap: 8px;
    }

    /* Controls */
    .power-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 8px 0 16px 0;
      border-bottom: 1px solid var(--card-border);
      margin-bottom: 16px;
    }
    .power-btn {
      background: #1e293b;
      border: 1px solid #334155;
      color: var(--text-muted);
      padding: 10px 24px;
      border-radius: 9999px;
      font-size: 0.95rem;
      font-weight: 600;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 8px;
      transition: all 0.2s ease;
    }
    .power-btn.on {
      background: linear-gradient(135deg, #10b981, #059669);
      border-color: #10b981;
      color: #fff;
      box-shadow: 0 0 15px rgba(16, 185, 129, 0.4);
    }

    .slider-group {
      margin-bottom: 14px;
    }
    .slider-header {
      display: flex;
      justify-content: space-between;
      font-size: 0.85rem;
      color: var(--text-muted);
      margin-bottom: 6px;
    }
    .slider-val {
      color: var(--primary);
      font-weight: 600;
    }
    input[type=range] {
      -webkit-appearance: none;
      width: 100%;
      height: 8px;
      border-radius: 4px;
      background: #1e293b;
      outline: none;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 22px;
      height: 22px;
      border-radius: 50%;
      background: var(--primary);
      cursor: pointer;
      box-shadow: 0 0 8px rgba(56, 189, 248, 0.6);
      transition: transform 0.1s;
    }
    input[type=range]::-webkit-slider-thumb:hover {
      transform: scale(1.15);
    }

    /* Effects Grid */
    .effects-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(140px, 1fr));
      gap: 10px;
    }
    .effect-btn {
      background: #172138;
      border: 1px solid #253352;
      border-radius: 10px;
      padding: 12px 10px;
      text-align: left;
      cursor: pointer;
      transition: all 0.2s ease;
      display: flex;
      flex-direction: column;
      gap: 4px;
      position: relative;
      overflow: hidden;
    }
    .effect-btn:hover {
      background: #1d2b49;
      border-color: #3b82f6;
    }
    .effect-btn.active {
      background: linear-gradient(135deg, rgba(56, 189, 248, 0.2), rgba(129, 140, 248, 0.2));
      border-color: var(--primary);
      box-shadow: 0 0 12px rgba(56, 189, 248, 0.35);
    }
    .effect-num {
      font-size: 0.75rem;
      font-weight: 700;
      color: var(--primary);
    }
    .effect-name {
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text);
      line-height: 1.2;
    }
    .effect-sub {
      font-size: 0.7rem;
      color: var(--text-muted);
    }

    /* Telemetry Grid */
    .telem-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(130px, 1fr));
      gap: 10px;
    }
    .telem-item {
      background: #172138;
      border: 1px solid #22304d;
      border-radius: 10px;
      padding: 10px 12px;
    }
    .telem-label {
      font-size: 0.72rem;
      color: var(--text-muted);
      margin-bottom: 4px;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }
    .telem-val {
      font-size: 0.95rem;
      font-weight: 700;
      color: var(--text);
    }

    /* Effect Store Cards */
    .store-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(240px, 1fr));
      gap: 12px;
    }
    .store-card {
      background: #172138;
      border: 1px solid #22304d;
      border-radius: 10px;
      padding: 12px;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      gap: 8px;
    }
    .store-header {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 6px;
    }
    .store-name {
      font-weight: 700;
      font-size: 0.92rem;
      color: var(--text);
    }
    .store-badge {
      font-size: 0.68rem;
      padding: 2px 6px;
      border-radius: 4px;
      font-weight: 600;
      white-space: nowrap;
    }
    .store-badge.installed {
      background: rgba(16, 185, 129, 0.2);
      color: var(--success);
      border: 1px solid rgba(16, 185, 129, 0.4);
    }
    .store-badge.available {
      background: rgba(56, 189, 248, 0.2);
      color: var(--primary);
      border: 1px solid rgba(56, 189, 248, 0.4);
    }
    .store-desc {
      font-size: 0.78rem;
      color: var(--text-muted);
      line-height: 1.35;
    }
    .store-meta {
      font-size: 0.72rem;
      color: var(--text-muted);
      display: flex;
      justify-content: space-between;
      border-top: 1px solid #22304d;
      padding-top: 6px;
    }
    .store-actions {
      display: flex;
      gap: 6px;
    }
    .btn-sm {
      padding: 6px 10px;
      font-size: 0.78rem;
      border-radius: 6px;
    }
    .btn-danger {
      background: rgba(239, 68, 68, 0.2);
      color: var(--danger);
      border: 1px solid rgba(239, 68, 68, 0.4);
    }
    .btn-danger:hover {
      background: var(--danger);
      color: #fff;
    }

    /* OTA & WiFi Forms */
    .btn {
      background: var(--primary);
      color: #0b0f19;
      border: none;
      padding: 10px 18px;
      border-radius: 8px;
      font-size: 0.88rem;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.2s;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
    }
    .btn:hover {
      background: var(--primary-hover);
    }
    .btn-secondary {
      background: #253352;
      color: var(--text);
    }
    .btn-secondary:hover {
      background: #334368;
    }
    .ota-row {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      margin-bottom: 14px;
      padding-bottom: 14px;
      border-bottom: 1px solid var(--card-border);
    }
    .ota-info {
      font-size: 0.85rem;
      color: var(--text-muted);
    }
    .ota-info strong {
      color: var(--text);
    }
    .upload-box {
      display: flex;
      flex-direction: column;
      gap: 10px;
    }
    .file-input-wrapper {
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
    }
    input[type=file] {
      font-size: 0.85rem;
      color: var(--text-muted);
    }
    .progress-bar-container {
      width: 100%;
      height: 10px;
      background: #1e293b;
      border-radius: 5px;
      overflow: hidden;
      display: none;
      margin-top: 6px;
    }
    .progress-bar {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, var(--primary), var(--accent));
      transition: width 0.2s;
    }
    .form-group {
      display: flex;
      flex-direction: column;
      gap: 8px;
      margin-bottom: 12px;
    }
    .form-group label {
      font-size: 0.8rem;
      color: var(--text-muted);
    }
    .form-group input {
      background: #172138;
      border: 1px solid var(--card-border);
      border-radius: 8px;
      padding: 10px 14px;
      color: var(--text);
      font-size: 0.9rem;
      outline: none;
    }
    .form-group input:focus {
      border-color: var(--primary);
    }
    .form-msg {
      font-size: 0.82rem;
      margin-top: 6px;
    }
    footer {
      text-align: center;
      font-size: 0.75rem;
      color: var(--text-muted);
      margin-top: 8px;
      margin-bottom: 16px;
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="brand">
        <div class="logo-icon">&#9733;</div>
        <div>
          <h1>MagicLamp</h1>
          <div style="font-size:0.75rem; color:var(--text-muted);">ESP32-C6 Interactive Light</div>
        </div>
      </div>
      <div id="conn-badge" class="status-badge">
        <span class="dot"></span>
        <span id="conn-text">Online</span>
      </div>
    </header>

    <!-- LAMP CONTROLS -->
    <section class="card">
      <div class="card-title">&#9881; Управление лампой</div>
      
      <div class="power-row">
        <div>
          <div style="font-weight:600;">Питание матрицы</div>
          <div style="font-size:0.8rem; color:var(--text-muted);">Плавный фейд включения/выключения</div>
        </div>
        <button id="btn-power" class="power-btn on" onclick="togglePower()">
          <span id="power-icon">&#9211;</span>
          <span id="power-text">ВКЛ</span>
        </button>
      </div>

      <div class="slider-group">
        <div class="slider-header">
          <span>Яркость матрицы</span>
          <span id="val-bright-text" class="slider-val">128 (50%)</span>
        </div>
        <input type="range" id="slider-bright" min="10" max="255" value="128"
               oninput="onBrightnessInput(this.value)" onchange="sendControl({brightness: parseInt(this.value)})">
      </div>

      <div class="slider-group">
        <div class="slider-header">
          <span>Скорость анимации</span>
          <span id="val-speed-text" class="slider-val">128</span>
        </div>
        <input type="range" id="slider-speed" min="1" max="255" value="128"
               oninput="onSpeedInput(this.value)" onchange="sendControl({speed: parseInt(this.value)})">
      </div>

      <div class="slider-group">
        <div class="slider-header">
          <span>Чувствительность микрофона</span>
          <span id="val-mic-text" class="slider-val">128</span>
        </div>
        <input type="range" id="slider-mic" min="1" max="255" value="128"
               oninput="onMicInput(this.value)" onchange="sendControl({micSensitivity: parseInt(this.value)})">
      </div>
    </section>

    <!-- EFFECTS SELECTION -->
    <section class="card">
      <div class="card-title">&#10024; Выбор эффекта (10 базовых режимов)</div>
      <div class="effects-grid" id="effects-list">
        <button class="effect-btn" data-id="0" onclick="setEffect(0)">
          <span class="effect-num">#0</span>
          <span class="effect-name">Ночник</span>
          <span class="effect-sub">Nightlight (2700K)</span>
        </button>
        <button class="effect-btn" data-id="1" onclick="setEffect(1)">
          <span class="effect-num">#1</span>
          <span class="effect-name">Рассвет / Закат</span>
          <span class="effect-sub">Dawn / Dusk</span>
        </button>
        <button class="effect-btn" data-id="2" onclick="setEffect(2)">
          <span class="effect-num">#2</span>
          <span class="effect-name">Костер</span>
          <span class="effect-sub">Fire Simulation</span>
        </button>
        <button class="effect-btn" data-id="3" onclick="setEffect(3)">
          <span class="effect-num">#3</span>
          <span class="effect-name">Шум Перлина</span>
          <span class="effect-sub">Perlin Noise 3D</span>
        </button>
        <button class="effect-btn" data-id="4" onclick="setEffect(4)">
          <span class="effect-num">#4</span>
          <span class="effect-name">Игра «Жизнь»</span>
          <span class="effect-sub">Conway's Life</span>
        </button>
        <button class="effect-btn" data-id="5" onclick="setEffect(5)">
          <span class="effect-num">#5</span>
          <span class="effect-name">Аттрактор Лоренца</span>
          <span class="effect-sub">Lorenz Attractor</span>
        </button>
        <button class="effect-btn" data-id="6" onclick="setEffect(6)">
          <span class="effect-num">#6</span>
          <span class="effect-name">Метаболы</span>
          <span class="effect-sub">Metaballs 5-spheres</span>
        </button>
        <button class="effect-btn" data-id="7" onclick="setEffect(7)">
          <span class="effect-num">#7</span>
          <span class="effect-name">Песочные часы</span>
          <span class="effect-sub">Sand Hourglass</span>
        </button>
        <button class="effect-btn" data-id="8" onclick="setEffect(8)">
          <span class="effect-num">#8</span>
          <span class="effect-name">Реакция на звук</span>
          <span class="effect-sub">Audio Spectrum FFT</span>
        </button>
        <button class="effect-btn" data-id="9" onclick="setEffect(9)">
          <span class="effect-num">#9</span>
          <span class="effect-name">Светлячки</span>
          <span class="effect-sub">Fireflies 15-particles</span>
        </button>
      </div>
    </section>

    <!-- GITHUB EFFECT STORE -->
    <section class="card">
      <div class="card-title" style="display:flex; justify-content:space-between; align-items:center;">
        <span>&#128722; Магазин эффектов (GitHub Effect Store)</span>
        <button class="btn btn-secondary btn-sm" onclick="loadStoreCatalog()">&#8635; Обновить витрину</button>
      </div>

      <div style="margin-bottom:14px;">
        <div style="display:flex; justify-content:space-between; font-size:0.8rem; color:var(--text-muted); margin-bottom:4px;">
          <span>Память LittleFS:</span>
          <span id="store-fs-info">Загрузка...</span>
        </div>
        <div class="progress-bar-container" style="display:block; margin-top:0;">
          <div class="progress-bar" id="store-fs-bar" style="width:0%;"></div>
        </div>
      </div>

      <div id="store-catalog-list" class="store-grid">
        <div style="font-size:0.85rem; color:var(--text-muted); padding:10px;">Загрузка каталога с GitHub...</div>
      </div>
      <div id="store-status-msg" style="font-size:0.8rem; margin-top:8px;"></div>
    </section>

    <!-- TELEMETRY -->
    <section class="card">
      <div class="card-title">&#128202; Телеметрия ESP32-C6</div>
      <div class="telem-grid">
        <div class="telem-item">
          <div class="telem-label">CPU Temp</div>
          <div class="telem-val"><span id="telem-temp">--</span> &deg;C</div>
        </div>
        <div class="telem-item">
          <div class="telem-label">Wi-Fi RSSI</div>
          <div class="telem-val"><span id="telem-rssi">--</span> dBm</div>
        </div>
        <div class="telem-item">
          <div class="telem-label">IP Адрес</div>
          <div class="telem-val" style="font-size:0.85rem;" id="telem-ip">--</div>
        </div>
        <div class="telem-item">
          <div class="telem-label">Uptime</div>
          <div class="telem-val" id="telem-uptime">--</div>
        </div>
        <div class="telem-item">
          <div class="telem-label">Свободная RAM</div>
          <div class="telem-val"><span id="telem-heap">--</span> KB</div>
        </div>
        <div class="telem-item">
          <div class="telem-label">Гироскоп GY-521</div>
          <div class="telem-val" id="telem-imu">--</div>
        </div>
        <div class="telem-item">
          <div class="telem-label">Версия</div>
          <div class="telem-val" id="telem-ver">v--</div>
        </div>
      </div>
    </section>

    <!-- OTA UPDATES -->
    <section class="card">
      <div class="card-title">&#128640; Обновление прошивки (OTA)</div>
      
      <!-- GitHub Cloud OTA -->
      <div class="ota-row">
        <div>
          <div class="ota-info">Текущая версия: <strong id="ota-cur-ver">v--</strong> | Последняя на GitHub: <strong id="ota-latest-ver">--</strong></div>
          <div class="ota-info" style="margin-top:4px;">Статус проверки: <span id="ota-status" style="color:var(--primary);">Idle</span></div>
        </div>
        <button id="btn-ota-check" class="btn btn-secondary" onclick="triggerGithubOta()">
          &#8635; Проверить и обновить с GitHub
        </button>
      </div>

      <!-- Local File Upload -->
      <div class="upload-box">
        <div style="font-size:0.85rem; font-weight:600;">Прямая прошивка файла firmware.bin</div>
        <div class="file-input-wrapper">
          <input type="file" id="ota-file" accept=".bin">
          <button class="btn" onclick="uploadFirmware()">&#8679; Прошить</button>
        </div>
        <div id="ota-progress-box" class="progress-bar-container">
          <div id="ota-progress" class="progress-bar"></div>
        </div>
        <div id="ota-upload-msg" class="form-msg"></div>
      </div>
    </section>

    <!-- WIFI CONFIG -->
    <section class="card">
      <div class="card-title">&#128246; Настройка Wi-Fi сети</div>
      <div style="font-size:0.82rem; color:var(--text-muted); margin-bottom:12px;">
        Текущая сеть: <strong id="wifi-cur-ssid" style="color:var(--text);">--</strong>
      </div>
      <div class="form-group">
        <label for="wifi-ssid">Имя сети (SSID):</label>
        <input type="text" id="wifi-ssid" placeholder="Введите имя Wi-Fi">
      </div>
      <div class="form-group">
        <label for="wifi-pass">Пароль сети:</label>
        <input type="password" id="wifi-pass" placeholder="Введите пароль">
      </div>
      <button class="btn" onclick="saveWifiCredentials()">Сохранить и подключить</button>
      <div id="wifi-msg" class="form-msg"></div>
    </section>

    <footer>
      MagicLamp ESP32-C6 &bull; Arduino Core 3.x &bull; FreeRTOS &bull; WS2812B 16x16
    </footer>
  </div>

  <script>
    let isPowered = true;
    let isDragging = false;
    let lastStatusTime = 0;
    let installedEffects = [];
    let cachedEffectsList = [];

    function formatUptime(seconds) {
      const h = Math.floor(seconds / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const s = seconds % 60;
      return `${h.toString().padStart(2, '0')}:${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
    }

    function onBrightnessInput(val) {
      isDragging = true;
      const pct = Math.round((val / 255) * 100);
      document.getElementById('val-bright-text').innerText = `${val} (${pct}%)`;
    }

    function onSpeedInput(val) {
      isDragging = true;
      document.getElementById('val-speed-text').innerText = val;
    }

    function onMicInput(val) {
      isDragging = true;
      document.getElementById('val-mic-text').innerText = val;
    }

    async function sendControl(payload) {
      isDragging = false;
      try {
        await fetch('/api/control', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
      } catch (e) {
        console.error('Control error:', e);
      }
    }

    function togglePower() {
      sendControl({ power: !isPowered });
    }

    function setEffect(id) {
      sendControl({ effect: id });
    }

    async function triggerGithubOta() {
      const btn = document.getElementById('btn-ota-check');
      btn.disabled = true;
      btn.innerText = 'Запрос...';
      try {
        await fetch('/api/ota/check', { method: 'POST' });
        document.getElementById('ota-status').innerText = 'Triggered GitHub check';
      } catch (e) {
        console.error('OTA trigger error:', e);
      }
      setTimeout(() => {
        btn.disabled = false;
        btn.innerHTML = '&#8635; Проверить и обновить с GitHub';
      }, 3000);
    }

    function uploadFirmware() {
      const fileInput = document.getElementById('ota-file');
      const file = fileInput.files[0];
      const msg = document.getElementById('ota-upload-msg');
      const progBox = document.getElementById('ota-progress-box');
      const progBar = document.getElementById('ota-progress');

      if (!file) {
        msg.style.color = 'var(--danger)';
        msg.innerText = 'Пожалуйста, выберите файл .bin для прошивки!';
        return;
      }

      const formData = new FormData();
      formData.append('update', file);

      progBox.style.display = 'block';
      progBar.style.width = '0%';
      msg.style.color = 'var(--text-muted)';
      msg.innerText = 'Загрузка прошивки...';

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/api/ota/upload', true);

      xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
          const pct = Math.round((e.loaded / e.total) * 100);
          progBar.style.width = pct + '%';
          msg.innerText = `Загрузка: ${pct}% (${Math.round(e.loaded/1024)} KB / ${Math.round(e.total/1024)} KB)`;
        }
      };

      xhr.onload = function() {
        if (xhr.status === 200) {
          progBar.style.width = '100%';
          msg.style.color = 'var(--success)';
          msg.innerHTML = '<strong>Успешно! Прошивка записана.</strong> Лампа перезагружается (подождите ~10 сек)...';
          setTimeout(() => location.reload(), 12000);
        } else {
          msg.style.color = 'var(--danger)';
          msg.innerText = 'Ошибка при прошивке: ' + xhr.responseText;
        }
      };

      xhr.onerror = function() {
        msg.style.color = 'var(--danger)';
        msg.innerText = 'Сбой сетевого соединения при загрузке.';
      };

      xhr.send(formData);
    }

    async function saveWifiCredentials() {
      const ssid = document.getElementById('wifi-ssid').value.trim();
      const pass = document.getElementById('wifi-pass').value;
      const msg = document.getElementById('wifi-msg');

      if (!ssid) {
        msg.style.color = 'var(--danger)';
        msg.innerText = 'Имя сети (SSID) не может быть пустым!';
        return;
      }

      msg.style.color = 'var(--text-muted)';
      msg.innerText = 'Сохранение настроек Wi-Fi...';

      try {
        const res = await fetch('/api/wifi', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ ssid: ssid, password: pass })
        });
        const data = await res.json();
        msg.style.color = 'var(--success)';
        msg.innerText = data.message || 'Настройки сохранены! Лампа переподключается.';
      } catch (e) {
        msg.style.color = 'var(--danger)';
        msg.innerText = 'Ошибка сохранения Wi-Fi: ' + e;
      }
    }

    async function fetchStatus() {
      try {
        const res = await fetch('/api/status');
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const data = await res.json();

        // Update connection badge
        document.getElementById('conn-badge').className = 'status-badge';
        document.getElementById('conn-text').innerText = data.isAp ? 'SoftAP Mode' : 'Online';

        // Update Power State
        isPowered = data.power;
        const btnPower = document.getElementById('btn-power');
        if (data.power) {
          btnPower.className = 'power-btn on';
          document.getElementById('power-text').innerText = 'ВКЛ';
        } else {
          btnPower.className = 'power-btn';
          document.getElementById('power-text').innerText = 'ВЫКЛ';
        }

        // Update Sliders if user isn't currently dragging them
        if (!isDragging) {
          const sBright = document.getElementById('slider-bright');
          sBright.value = data.brightness;
          const pct = Math.round((data.brightness / 255) * 100);
          document.getElementById('val-bright-text').innerText = `${data.brightness} (${pct}%)`;

          const sSpeed = document.getElementById('slider-speed');
          sSpeed.value = data.speed;
          document.getElementById('val-speed-text').innerText = data.speed;

          const sMic = document.getElementById('slider-mic');
          sMic.value = data.micSensitivity;
          document.getElementById('val-mic-text').innerText = data.micSensitivity;
        }

        // Dynamic Effect Buttons Update
        if (data.effects && JSON.stringify(data.effects) !== JSON.stringify(cachedEffectsList)) {
          cachedEffectsList = data.effects;
          const listContainer = document.getElementById('effects-list');
          listContainer.innerHTML = '';
          data.effects.forEach(e => {
            const btn = document.createElement('button');
            btn.className = 'effect-btn' + (e.index === data.effect ? ' active' : '');
            btn.setAttribute('data-id', e.index);
            btn.onclick = () => setEffect(e.index);
            btn.innerHTML = `
              <span class="effect-num">#${e.index}</span>
              <span class="effect-name">${e.name}</span>
              <span class="effect-sub">${e.index < 10 ? 'Базовый' : 'Custom .eff'}</span>
            `;
            listContainer.appendChild(btn);
          });
        } else {
          // Highlight Active Effect
          const btns = document.querySelectorAll('.effect-btn');
          btns.forEach(b => {
            if (parseInt(b.getAttribute('data-id')) === data.effect) {
              b.classList.add('active');
            } else {
              b.classList.remove('active');
            }
          });
        }

        // Telemetry Update
        document.getElementById('telem-temp').innerText = (data.cpuTemp !== undefined) ? data.cpuTemp.toFixed(1) : '--';
        document.getElementById('telem-rssi').innerText = (data.rssi !== undefined) ? data.rssi : '--';
        document.getElementById('telem-ip').innerText = data.ip || '--';
        document.getElementById('telem-uptime').innerText = formatUptime(data.uptime || 0);
        document.getElementById('telem-heap').innerText = data.freeHeap || '--';
        document.getElementById('telem-imu').innerText = data.imuStatus || '--';
        document.getElementById('telem-ver').innerText = 'v' + (data.version || '--');

        // OTA Info
        document.getElementById('ota-cur-ver').innerText = 'v' + (data.version || '--');
        document.getElementById('ota-latest-ver').innerText = data.latestVersion ? ('v' + data.latestVersion) : 'не проверялась';
        document.getElementById('ota-status').innerText = data.otaStatus || 'Idle';

        // WiFi current SSID
        document.getElementById('wifi-cur-ssid').innerText = data.ssid || (data.isAp ? 'MagicLamp-Setup' : 'Не подключено');

      } catch (err) {
        document.getElementById('conn-badge').className = 'status-badge offline';
        document.getElementById('conn-text').innerText = 'Disconnected';
      }
    }

    // Effect Store Operations
    async function fetchInstalled() {
      try {
        const res = await fetch('/api/store/installed');
        if (!res.ok) return;
        const data = await res.json();
        installedEffects = data.installed || [];
        if (data.totalBytes > 0) {
          const usedKb = Math.round(data.usedBytes / 1024);
          const totalKb = Math.round(data.totalBytes / 1024);
          const pct = Math.min(100, Math.round((data.usedBytes / data.totalBytes) * 100));
          document.getElementById('store-fs-info').innerText = `${usedKb} KB / ${totalKb} KB (${pct}%)`;
          document.getElementById('store-fs-bar').style.width = pct + '%';
        }
      } catch (e) {
        console.error('Installed effects fetch error:', e);
      }
    }

    async function loadStoreCatalog() {
      const msg = document.getElementById('store-status-msg');
      const container = document.getElementById('store-catalog-list');
      msg.innerText = '';
      container.innerHTML = '<div style="font-size:0.85rem; color:var(--text-muted); padding:10px;">Загрузка каталога с GitHub...</div>';

      await fetchInstalled();

      let manifest = null;
      try {
        const ghRes = await fetch('https://raw.githubusercontent.com/gaterkik/MagicLamp/main/store/manifest.json', { cache: 'no-cache' });
        if (ghRes.ok) {
          manifest = await ghRes.json();
        }
      } catch (err) {
        console.log('Direct GitHub fetch failed, trying local proxy:', err);
      }

      if (!manifest) {
        try {
          const localRes = await fetch('/api/store/catalog');
          if (localRes.ok) {
            manifest = await localRes.json();
          }
        } catch (err) {
          console.error('Proxy catalog fetch failed:', err);
        }
      }

      if (!manifest || !manifest.effects || manifest.effects.length === 0) {
        container.innerHTML = '<div style="font-size:0.85rem; color:var(--danger); padding:10px;">Каталог эффектов временно недоступен. Проверьте интернет или повторите попытку.</div>';
        return;
      }

      container.innerHTML = '';
      manifest.effects.forEach(eff => {
        const isInst = installedEffects.some(ie => ie.id === eff.id);
        const card = document.createElement('div');
        card.className = 'store-card';
        card.innerHTML = `
          <div class="store-header">
            <span class="store-name">${eff.name}</span>
            <span class="store-badge ${isInst ? 'installed' : 'available'}">${isInst ? 'Установлен' : 'Доступен'}</span>
          </div>
          <div class="store-desc">${eff.description || ''}</div>
          <div class="store-meta">
            <span>v${eff.version || '1.0'} &bull; ${eff.author || 'MagicLamp'}</span>
            <span>${eff.size || 256} Б</span>
          </div>
          <div class="store-actions">
            ${isInst
              ? `<button class="btn btn-sm" onclick="activateCustomEffect('${eff.name}')">&#9654; Включить</button>
                 <button class="btn btn-sm btn-danger" onclick="deleteStoreEffect('${eff.id}')">&#128465; Удалить</button>`
              : `<button class="btn btn-sm" onclick="installStoreEffect('${eff.id}', '${eff.url || ''}')">&#128229; Скачать в лампу</button>`
            }
          </div>
        `;
        container.appendChild(card);
      });
    }

    function activateCustomEffect(effectName) {
      if (cachedEffectsList) {
        const found = cachedEffectsList.find(e => e.name === effectName);
        if (found) {
          setEffect(found.index);
          return;
        }
      }
      sendControl({});
    }

    async function installStoreEffect(id, url) {
      const msg = document.getElementById('store-status-msg');
      msg.style.color = 'var(--text-muted)';
      msg.innerText = `Скачивание эффекта '${id}' в LittleFS...`;
      try {
        const res = await fetch('/api/store/install', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id: id, url: url })
        });
        const data = await res.json();
        if (res.ok) {
          msg.style.color = 'var(--success)';
          msg.innerText = `Эффект '${id}' успешно загружен и активирован!`;
          await fetchStatus();
          await loadStoreCatalog();
        } else {
          msg.style.color = 'var(--danger)';
          msg.innerText = 'Ошибка: ' + (data.message || 'Сбой установки');
        }
      } catch (e) {
        msg.style.color = 'var(--danger)';
        msg.innerText = 'Сбой сети: ' + e;
      }
    }

    async function deleteStoreEffect(id) {
      if (!confirm(`Удалить эффект '${id}' из LittleFS памяти лампы?`)) return;
      const msg = document.getElementById('store-status-msg');
      msg.style.color = 'var(--text-muted)';
      msg.innerText = `Удаление '${id}'...`;
      try {
        const res = await fetch('/api/store/delete', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id: id })
        });
        const data = await res.json();
        if (res.ok) {
          msg.style.color = 'var(--success)';
          msg.innerText = `Эффект '${id}' удален.`;
          await fetchStatus();
          await loadStoreCatalog();
        } else {
          msg.style.color = 'var(--danger)';
          msg.innerText = 'Ошибка: ' + (data.message || 'Сбой удаления');
        }
      } catch (e) {
        msg.style.color = 'var(--danger)';
        msg.innerText = 'Сбой сети: ' + e;
      }
    }

    // Initial fetch & polling
    fetchStatus();
    setInterval(fetchStatus, 1500);
    loadStoreCatalog();
  </script>
</body>
</html>
)rawliteral";
