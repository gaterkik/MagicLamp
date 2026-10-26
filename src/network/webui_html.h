#pragma once

#include <Arduino.h>

/**
 * @file webui_html.h
 * @brief Self-contained Single Page Application WebUI stored in Flash memory (PROGMEM).
 *
 * MagicLamp v0.9.0 WebUI:
 * - Modern Apple-inspired dark mode UI.
 * - Accordion UI for all 15 built-in native effects with individual NVS profiles.
 * - Checkboxes for button cycle inclusion (inCycle / cycle_mask).
 * - Dynamic parameter sliders (Brightness, Speed, Mic Sensitivity).
 * - Schedule settings (Dawn sunrise alarm & Dusk sleep timer) inside schedule card.
 * - Live ESP32-C6 Telemetry.
 * - Radio Mode switcher (Wi-Fi <-> Zigbee) with status indicator.
 * - GitHub Silent OTA and Direct Browser Firmware Upload.
 * - Wi-Fi network configuration.
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
      --bg: #070a13;
      --card-bg: #0f172a;
      --card-border: #1e293b;
      --card-hover: #172544;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --primary: #38bdf8;
      --primary-hover: #0ea5e9;
      --accent: #818cf8;
      --purple: #c084fc;
      --purple-bg: rgba(192, 132, 252, 0.15);
      --success: #10b981;
      --danger: #ef4444;
      --warning: #f59e0b;
      --radius: 14px;
      --shadow: 0 6px 24px -2px rgba(0, 0, 0, 0.55);
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
      max-width: 860px;
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
      width: 36px;
      height: 36px;
      border-radius: 10px;
      background: linear-gradient(135deg, #f59e0b, #ef4444, #8b5cf6);
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 20px;
      box-shadow: 0 0 14px rgba(245, 158, 11, 0.45);
    }
    h1 {
      font-size: 1.25rem;
      font-weight: 700;
      letter-spacing: -0.02em;
      background: linear-gradient(135deg, #38bdf8, #c084fc);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .header-badges {
      display: flex;
      align-items: center;
      gap: 8px;
      flex-wrap: wrap;
      justify-content: flex-end;
    }
    .badge {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      font-size: 0.78rem;
      padding: 5px 10px;
      border-radius: 9999px;
      font-weight: 600;
      border: 1px solid transparent;
    }
    .badge-time {
      background: rgba(56, 189, 248, 0.12);
      color: var(--primary);
      border-color: rgba(56, 189, 248, 0.3);
    }
    .badge-online {
      background: rgba(16, 185, 129, 0.15);
      color: var(--success);
      border-color: rgba(16, 185, 129, 0.3);
    }
    .badge-offline {
      background: rgba(239, 68, 68, 0.15);
      color: var(--danger);
      border-color: rgba(239, 68, 68, 0.3);
    }
    .badge-radio-wifi {
      background: rgba(6, 182, 212, 0.15);
      color: #06b6d4;
      border-color: rgba(6, 182, 212, 0.35);
    }
    .badge-radio-zigbee {
      background: var(--purple-bg);
      color: var(--purple);
      border-color: rgba(192, 132, 252, 0.4);
    }
    .dot {
      width: 7px;
      height: 7px;
      border-radius: 50%;
      background: currentColor;
    }

    .card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: var(--radius);
      padding: 20px;
      box-shadow: var(--shadow);
    }
    .card-title {
      font-size: 1.05rem;
      font-weight: 600;
      color: var(--text);
      margin-bottom: 14px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
    }

    /* Power & Radio Row */
    .control-row {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: space-between;
      gap: 14px;
      padding-bottom: 16px;
      border-bottom: 1px solid var(--card-border);
      margin-bottom: 16px;
    }
    .power-btn {
      background: #1e293b;
      border: 1px solid #334155;
      color: var(--text-muted);
      padding: 10px 22px;
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
      box-shadow: 0 0 16px rgba(16, 185, 129, 0.45);
    }
    .radio-switch-btn {
      background: #1e293b;
      border: 1px solid #334155;
      color: var(--text);
      padding: 9px 18px;
      border-radius: 9999px;
      font-size: 0.86rem;
      font-weight: 600;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 8px;
      transition: all 0.2s ease;
    }
    .radio-switch-btn:hover {
      background: #27354f;
      border-color: var(--primary);
    }
    .radio-switch-btn.zigbee-mode {
      border-color: var(--purple);
      color: var(--purple);
    }

    /* Accordion UI */
    .accordion-list {
      display: flex;
      flex-direction: column;
      gap: 10px;
    }
    .accordion-item {
      background: #131b2e;
      border: 1px solid #22304d;
      border-radius: 12px;
      overflow: hidden;
      transition: all 0.25s ease;
    }
    .accordion-item:hover {
      border-color: #334368;
    }
    .accordion-item.active {
      border-color: var(--primary);
      box-shadow: 0 0 14px rgba(56, 189, 248, 0.2);
    }
    .accordion-header {
      padding: 14px 16px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      cursor: pointer;
      user-select: none;
      gap: 10px;
    }
    .accordion-header-left {
      display: flex;
      align-items: center;
      gap: 12px;
      min-width: 0;
      flex: 1;
    }
    .accordion-num {
      font-size: 0.78rem;
      font-weight: 700;
      color: var(--primary);
      min-width: 26px;
    }
    .accordion-icon {
      font-size: 1.25rem;
      line-height: 1;
    }
    .accordion-text {
      display: flex;
      flex-direction: column;
      gap: 2px;
      min-width: 0;
    }
    .accordion-name {
      font-size: 0.95rem;
      font-weight: 600;
      color: var(--text);
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    .accordion-sub {
      font-size: 0.73rem;
      color: var(--text-muted);
    }
    .accordion-header-right {
      display: flex;
      align-items: center;
      gap: 14px;
      flex-shrink: 0;
    }

    /* InCycle Checkbox Custom Switch */
    .incycle-wrapper {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      cursor: pointer;
      padding: 4px 8px;
      border-radius: 8px;
      background: rgba(30, 41, 59, 0.7);
      border: 1px solid #27354f;
      transition: background 0.2s;
    }
    .incycle-wrapper:hover {
      background: #1e293b;
    }
    .incycle-label {
      font-size: 0.72rem;
      color: var(--text-muted);
      user-select: none;
    }
    .incycle-chk {
      appearance: none;
      -webkit-appearance: none;
      width: 16px;
      height: 16px;
      border: 2px solid #475569;
      border-radius: 4px;
      outline: none;
      cursor: pointer;
      position: relative;
      background: #0f172a;
      transition: all 0.15s;
    }
    .incycle-chk:checked {
      background: var(--success);
      border-color: var(--success);
    }
    .incycle-chk:checked::after {
      content: '';
      position: absolute;
      left: 4px;
      top: 1px;
      width: 4px;
      height: 8px;
      border: solid #fff;
      border-width: 0 2px 2px 0;
      transform: rotate(45deg);
    }

    .accordion-chevron {
      font-size: 0.75rem;
      color: var(--text-muted);
      transition: transform 0.25s ease;
    }
    .accordion-item.expanded .accordion-chevron {
      transform: rotate(180deg);
      color: var(--primary);
    }

    .accordion-body {
      max-height: 0;
      overflow: hidden;
      transition: max-height 0.35s ease;
      background: #0d1424;
      border-top: 1px solid transparent;
    }
    .accordion-item.expanded .accordion-body {
      max-height: 600px;
      border-top-color: #22304d;
    }
    .accordion-body-inner {
      padding: 16px;
      display: flex;
      flex-direction: column;
      gap: 14px;
    }

    /* Sliders */
    .slider-group {
      display: flex;
      flex-direction: column;
      gap: 6px;
    }
    .slider-header {
      display: flex;
      justify-content: space-between;
      font-size: 0.84rem;
      color: var(--text-muted);
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
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: var(--primary);
      cursor: pointer;
      box-shadow: 0 0 8px rgba(56, 189, 248, 0.6);
      transition: transform 0.1s;
    }
    input[type=range]::-webkit-slider-thumb:hover {
      transform: scale(1.15);
    }

    /* Schedule block inside card 1 */
    .schedule-box {
      background: #131c31;
      border: 1px solid #243555;
      border-radius: 10px;
      padding: 14px;
      display: flex;
      flex-direction: column;
      gap: 12px;
    }
    .sched-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .sched-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }
    .sched-item {
      background: #0b1120;
      padding: 10px;
      border-radius: 8px;
      border: 1px solid #1e293b;
    }
    .sched-label {
      font-size: 0.78rem;
      color: var(--text-muted);
      margin-bottom: 6px;
    }
    input[type=time] {
      background: #172138;
      border: 1px solid #334155;
      color: var(--text);
      padding: 6px 10px;
      border-radius: 6px;
      width: 100%;
      font-size: 0.95rem;
      outline: none;
    }
    select.sched-select {
      background: #172138;
      border: 1px solid #334155;
      color: var(--text);
      padding: 6px 10px;
      border-radius: 6px;
      width: 100%;
      font-size: 0.88rem;
      outline: none;
      margin-top: 4px;
      cursor: pointer;
    }
    select.sched-select:focus {
      border-color: var(--primary);
    }

    /* Telemetry Grid */
    .telem-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(130px, 1fr));
      gap: 10px;
    }
    .telem-item {
      background: #131b2e;
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

    /* Buttons */
    .btn {
      background: var(--primary);
      color: #070a13;
      border: none;
      padding: 9px 16px;
      border-radius: 8px;
      font-size: 0.86rem;
      font-weight: 600;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
      transition: background 0.2s;
    }
    .btn:hover {
      background: var(--primary-hover);
    }
    .btn-secondary {
      background: #1e293b;
      color: var(--text);
      border: 1px solid #334155;
    }
    .btn-secondary:hover {
      background: #27354f;
      border-color: var(--primary);
    }
    .btn-sm {
      padding: 5px 10px;
      font-size: 0.78rem;
      border-radius: 6px;
    }

    /* OTA & WiFi Forms */
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
      font-size: 0.84rem;
      color: var(--text-muted);
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
    .progress-bar-container {
      width: 100%;
      height: 8px;
      background: #1e293b;
      border-radius: 4px;
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
      gap: 6px;
      margin-bottom: 12px;
    }
    .form-group label {
      font-size: 0.8rem;
      color: var(--text-muted);
    }
    .form-group input {
      background: #131b2e;
      border: 1px solid var(--card-border);
      border-radius: 8px;
      padding: 9px 12px;
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
          <div style="font-size:0.75rem; color:var(--text-muted);">ESP32-C6 Interactive Light v0.9.0</div>
        </div>
      </div>
      <div class="header-badges">
        <div id="badge-clock" class="badge badge-time">
          <span>&#128339;</span>
          <span id="lamp-time">--:--:--</span>
        </div>
        <div id="badge-radio" class="badge badge-radio-wifi">
          <span class="dot"></span>
          <span id="header-radio-text">Wi-Fi</span>
        </div>
        <div id="conn-badge" class="badge badge-online">
          <span class="dot"></span>
          <span id="conn-text">Online</span>
        </div>
      </div>
    </header>

    <!-- LAMP POWER & RADIO MODULE -->
    <section class="card">
      <div class="card-title">
        <span>&#9881; Управление питанием и радио</span>
      </div>

      <div class="control-row">
        <div>
          <div style="font-weight:600; font-size:0.95rem;">Питание матрицы WS2812B</div>
          <div style="font-size:0.8rem; color:var(--text-muted);">Плавная штора включения (600 мс) и выключения</div>
        </div>
        <button id="btn-power" class="power-btn on" onclick="togglePower()">
          <span id="power-icon">&#9211;</span>
          <span id="power-text">ВКЛ</span>
        </button>
      </div>

      <div style="display:flex; flex-wrap:wrap; align-items:center; justify-content:space-between; gap:12px;">
        <div>
          <div style="font-weight:600; font-size:0.95rem;">Радиомодуль ESP32-C6</div>
          <div style="font-size:0.8rem; color:var(--text-muted);">
            Текущий протокол: <strong id="radio-mode-desc" style="color:var(--primary);">Wi-Fi 2.4 GHz</strong>
            <span style="display:block; font-size:0.72rem; color:var(--text-muted); margin-top:2px;">(Переключение: кнопка ниже или 4 клика по TTP223)</span>
          </div>
        </div>
        <button id="btn-radio-switch" class="radio-switch-btn" onclick="switchRadio()">
          <span id="radio-btn-icon">&#128246;</span>
          <span id="radio-btn-text">Переключить на Zigbee</span>
        </button>
      </div>

      <div style="margin-top:14px; padding-top:12px; border-top:1px solid var(--card-border);">
        <div style="font-weight:600; font-size:0.9rem; margin-bottom:6px;">Профиль совместимости Zigbee (Алиса):</div>
        <select id="sel-zb-profile" class="sched-select" onchange="onZbProfileChange(this.value)">
          <option value="0">Профиль 1: Tuya / Яндекс RGB+CCT (0x010D, TS0505B) [По умолчанию]</option>
          <option value="1">Профиль 2: Яндекс Станция Extended (0x010D, YNDX-00531)</option>
          <option value="2">Профиль 3: Philips Hue RGB (0x0102, LCT015)</option>
          <option value="3">Профиль 4: IKEA Диммер (0x0101, TRADFRI)</option>
        </select>
      </div>
    </section>

    <!-- GLOBAL INDEPENDENT SCHEDULE: DAWN & DUSK -->
    <section class="card">
      <div class="card-title">
        <span>&#9200; Будильник Рассвет и Таймер Отбой</span>
        <span id="sched-clock" style="font-size:0.85rem; font-weight:600; color:var(--primary); font-family:monospace;">--:--:-- GMT+2</span>
      </div>

      <!-- Индикатор активной фазы расписания -->
      <div id="sched-phase-badge" class="badge" style="background:rgba(56,189,248,0.12); color:var(--primary); border:1px solid rgba(56,189,248,0.3); padding:8px 12px; margin-bottom:14px; width:100%; border-radius:8px; display:flex; align-items:center; gap:8px;">
        <span class="dot" style="background:currentColor;"></span>
        <span id="sched-phase-text" style="font-size:0.84rem;">Ожидание расписания</span>
      </div>

      <div class="sched-grid">
        <!-- Рассвет -->
        <div class="sched-item">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;">
            <div class="sched-label" style="margin-bottom:0; font-weight:600; color:var(--text);">&#127749; Рассвет (Будильник)</div>
            <button id="btn-dawn-toggle" class="power-btn" style="padding:4px 12px; font-size:0.75rem;" onclick="toggleDawn(event)">
              <span id="dawn-status-text">ВЫКЛ</span>
            </button>
          </div>
          <div style="font-size:0.75rem; color:var(--text-muted); margin-bottom:4px;">Целевое время завершения:</div>
          <input type="time" id="sched-dawn-time" value="07:00" onchange="saveScheduleTimes()">
          <div style="margin-top:8px;">
            <div style="font-size:0.75rem; color:var(--text-muted);">Длительность разгорания:</div>
            <select id="sched-dawn-dur" class="sched-select" onchange="onDawnDurationChange(this.value)">
              <option value="5">5 минут</option>
              <option value="10">10 минут</option>
              <option value="15">15 минут</option>
              <option value="30">30 минут</option>
            </select>
          </div>
          <div style="font-size:0.72rem; color:var(--text-muted); margin-top:6px;">
            Включает лампу до 100% ровно к целевому времени, плавно набирая свет
          </div>
        </div>

        <!-- Отбой -->
        <div class="sched-item">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;">
            <div class="sched-label" style="margin-bottom:0; font-weight:600; color:var(--text);">&#127769; Отбой (Таймер сна)</div>
            <button id="btn-dusk-toggle" class="power-btn" style="padding:4px 12px; font-size:0.75rem;" onclick="toggleDusk(event)">
              <span id="dusk-status-text">ВЫКЛ</span>
            </button>
          </div>
          <div style="font-size:0.75rem; color:var(--text-muted); margin-bottom:4px;">Целевое время выключения:</div>
          <input type="time" id="sched-dusk-time" value="23:00" onchange="saveScheduleTimes()">
          <div style="margin-top:8px;">
            <div style="font-size:0.75rem; color:var(--text-muted);">Длительность затухания:</div>
            <select id="sched-dusk-dur" class="sched-select" onchange="onDuskDurationChange(this.value)">
              <option value="15">15 минут</option>
              <option value="30">30 минут</option>
              <option value="45">45 минут</option>
            </select>
          </div>
          <div style="font-size:0.72rem; color:var(--text-muted); margin-top:6px;">
            Переключает в янтарный свет 1900K (B = 0) и плавно гасит до полного выключения
          </div>
        </div>
      </div>
    </section>

    <!-- BUILT-IN EFFECTS ACCORDION -->
    <section class="card">
      <div class="card-title">
        <span>&#10024; Эффекты лампы</span>
        <span style="font-size:0.76rem; font-weight:normal; color:var(--text-muted);">2 клика кнопки циклируют отмеченные &#9745;</span>
      </div>

      <div class="accordion-list" id="effects-accordion">
        <!-- Rendered dynamically by JavaScript -->
      </div>
    </section>

    <!-- TELEMETRY -->
    <section class="card">
      <div class="card-title">
        <span>&#128202; Телеметрия ESP32-C6</span>
      </div>
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
          <div class="telem-label">Радиомодуль</div>
          <div class="telem-val" id="telem-radio">Wi-Fi</div>
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
          <div class="telem-label">Версия</div>
          <div class="telem-val" id="telem-ver">v0.9.7</div>
        </div>
      </div>
    </section>

    <!-- OTA UPDATES -->
    <section class="card">
      <div class="card-title">&#128640; Обновление прошивки (OTA)</div>
      
      <div class="ota-row">
        <div>
          <div class="ota-info">Текущая версия: <strong id="ota-cur-ver">v0.9.7</strong> | Последняя на GitHub: <strong id="ota-latest-ver">--</strong></div>
          <div class="ota-info" style="margin-top:4px;">Статус проверки: <span id="ota-status" style="color:var(--primary);">Idle</span></div>
        </div>
        <button id="btn-ota-check" class="btn btn-secondary" onclick="triggerGithubOta()">
          &#8635; Проверить на GitHub
        </button>
      </div>

      <div class="upload-box">
        <div style="font-size:0.84rem; font-weight:600;">Прямая прошивка бинарника firmware.bin</div>
        <div class="file-input-wrapper">
          <input type="file" id="ota-file" accept=".bin" style="font-size:0.85rem; color:var(--text-muted);">
          <button class="btn" onclick="uploadFirmware()">&#8679; Прошить во Flash</button>
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
        Текущее подключение: <strong id="wifi-cur-ssid" style="color:var(--text);">--</strong>
      </div>
      <div class="form-group">
        <label for="wifi-ssid">Имя сети (SSID):</label>
        <input type="text" id="wifi-ssid" placeholder="Введите имя Wi-Fi сети">
      </div>
      <div class="form-group">
        <label for="wifi-pass">Пароль сети:</label>
        <input type="password" id="wifi-pass" placeholder="Введите пароль">
      </div>
      <button class="btn" onclick="saveWifiCredentials()">Сохранить и переподключить</button>
      <div id="wifi-msg" class="form-msg"></div>
    </section>

    <!-- FLIGHT RECORDER / SYSTEM LOG -->
    <section class="card">
      <details style="cursor:pointer;" open>
        <summary class="card-title" style="margin-bottom:0; display:flex; justify-content:space-between; align-items:center;">
          <span>&#128220; Системный журнал (Бортовой самописец)</span>
          <span style="font-size:0.75rem; color:var(--text-muted); font-weight:normal;">Статус сессий</span>
        </summary>
        <div style="margin-top:14px; padding-top:12px; border-top:1px solid var(--card-border);">
          <div style="font-size:0.82rem; margin-bottom:8px;">
            <span style="color:var(--text-muted);">Последний статус Zigbee-сессии:</span>
            <div id="zb-session-log" style="font-family:monospace; font-size:0.8rem; background:rgba(0,0,0,0.35); padding:8px 10px; border-radius:6px; border:1px solid var(--card-border); margin-top:4px; color:var(--purple); word-break:break-word;">
              Загрузка...
            </div>
          </div>
          <div style="font-size:0.75rem; color:var(--text-muted);">
            Данные фиксируются в энергонезависимой памяти NVS при переходах между радиостеками Wi-Fi и Zigbee 3.0 (IEEE 802.15.4).
          </div>
        </div>
      </details>
    </section>

    <footer>
      MagicLamp ESP32-C6 &bull; Arduino Core 3.x &bull; FastLED RMT &bull; Zigbee &bull; WS2812B 16x16
    </footer>
  </div>

  <script>
    // 15 built-in native effects metadata
    const EFFECTS_META = [
      { id: 0, name: "Ночник", sub: "Nightlight (2700K)", icon: "🌙", dynamic: false, audio: false, schedule: false },
      { id: 1, name: "Рассвет / Закат", sub: "Dawn / Dusk (1900K amber)", icon: "🌅", dynamic: false, audio: false, schedule: false },
      { id: 2, name: "Костер", sub: "Fire Simulation 3D", icon: "🔥", dynamic: true, audio: false, schedule: false },
      { id: 3, name: "Шум Перлина", sub: "Perlin Noise 3D", icon: "🌊", dynamic: true, audio: false, schedule: false },
      { id: 4, name: "Игра «Жизнь»", sub: "Conway's Cellular Automata", icon: "🧬", dynamic: true, audio: false, schedule: false },
      { id: 5, name: "Аттрактор Лоренца", sub: "Lorenz Strange Attractor", icon: "🌀", dynamic: true, audio: false, schedule: false },
      { id: 6, name: "Метаболы", sub: "Metaballs 5-Spheres Field", icon: "🫧", dynamic: true, audio: false, schedule: false },
      { id: 7, name: "Реакция на звук", sub: "Audio Spectrum FFT 16-bands", icon: "🎵", dynamic: false, audio: true, schedule: false },
      { id: 8, name: "Светлячки", sub: "Fireflies 15-Particles Flocking", icon: "✨", dynamic: true, audio: false, schedule: false },
      { id: 9, name: "Северное сияние", sub: "Aurora Borealis Curtains", icon: "🌌", dynamic: true, audio: false, schedule: false },
      { id: 10, name: "Матрица", sub: "Matrix Rain Digital Cascade", icon: "💻", dynamic: true, audio: false, schedule: false },
      { id: 11, name: "Лава-лампа", sub: "Lava Lamp Thermal Convection", icon: "🏺", dynamic: true, audio: false, schedule: false },
      { id: 12, name: "Галактическая спираль", sub: "Vortex Spiral Arms", icon: "🪐", dynamic: true, audio: false, schedule: false },
      { id: 13, name: "Фейерверк", sub: "Fireworks Particle Explosion", icon: "🎆", dynamic: false, audio: false, schedule: false },
      { id: 14, name: "Киберпанк-пульс", sub: "Cyber Pulse Neon Shader", icon: "⚡", dynamic: true, audio: false, schedule: false }
    ];

    let currentEffectId = 0;
    let isPowered = true;
    let currentRadioMode = "wifi";
    let isDragging = false;
    let isScheduleEnabled = false;
    let cycleMask = 0x7FFF;
    let effectsProfiles = {};

    function formatUptime(seconds) {
      const h = Math.floor(seconds / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const s = seconds % 60;
      return `${h.toString().padStart(2, '0')}:${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
    }

    // Build the 16 accordion cards
    function renderAccordion() {
      const container = document.getElementById('effects-accordion');
      if (!container) return;
      container.innerHTML = '';

      EFFECTS_META.forEach(eff => {
        const item = document.createElement('div');
        item.className = 'accordion-item' + (eff.id === currentEffectId ? ' active expanded' : '');
        item.id = `eff-item-${eff.id}`;

        let dynamicControls = '';

        // 1. Individual Brightness Slider
        dynamicControls += `
          <div class="slider-group">
            <div class="slider-header">
              <span>Яркость</span>
              <span id="val-bright-${eff.id}" class="slider-val">128 (50%)</span>
            </div>
            <input type="range" id="slider-bright-${eff.id}" min="10" max="255" value="128"
                   oninput="onBrightnessInput(${eff.id}, this.value)"
                   onchange="onBrightnessChange(${eff.id}, this.value)">
          </div>
        `;

        // 1.5 Color Temperature Slider (Only for Nightlight #0: 1800K - 4000K)
        if (eff.id === 0) {
          const initKelvin = Math.round(1800 + (90 / 255) * 2200);
          dynamicControls += `
            <div class="slider-group">
              <div class="slider-header">
                <span>Температура света (1800K — 4000K)</span>
                <span id="val-temp-${eff.id}" class="slider-val">${initKelvin}K</span>
              </div>
              <input type="range" id="slider-temp-${eff.id}" min="0" max="255" value="90"
                     oninput="onTempInput(${eff.id}, this.value)"
                     onchange="onTempChange(${eff.id}, this.value)">
            </div>
          `;
        }

        // 2. Individual Speed Slider (Only for dynamic effects)
        if (eff.dynamic) {
          dynamicControls += `
            <div class="slider-group">
              <div class="slider-header">
                <span>Скорость анимации</span>
                <span id="val-speed-${eff.id}" class="slider-val">128</span>
              </div>
              <input type="range" id="slider-speed-${eff.id}" min="1" max="255" value="128"
                     oninput="onSpeedInput(${eff.id}, this.value)"
                     onchange="onSpeedChange(${eff.id}, this.value)">
            </div>
          `;
        }

        // 3. Individual Mic Sensitivity Slider (Only for AudioReactive & CyberPulse)
        if (eff.audio) {
          dynamicControls += `
            <div class="slider-group">
              <div class="slider-header">
                <span>Чувствительность микрофона</span>
                <span id="val-mic-${eff.id}" class="slider-val">128</span>
              </div>
              <input type="range" id="slider-mic-${eff.id}" min="1" max="255" value="128"
                     oninput="onMicInput(${eff.id}, this.value)"
                     onchange="onMicChange(${eff.id}, this.value)">
            </div>
          `;
        }

        item.innerHTML = `
          <div class="accordion-header" onclick="onCardClick(${eff.id})">
            <div class="accordion-header-left">
              <span class="accordion-num">#${eff.id}</span>
              <span class="accordion-icon">${eff.icon}</span>
              <div class="accordion-text">
                <span class="accordion-name">${eff.name}</span>
                <span class="accordion-sub">${eff.sub}</span>
              </div>
            </div>
            <div class="accordion-header-right">
              <label class="incycle-wrapper" onclick="event.stopPropagation()">
                <span class="incycle-label">В цикле</span>
                <input type="checkbox" id="chk-cycle-${eff.id}" class="incycle-chk" checked
                       onchange="onCycleToggle(${eff.id}, this.checked)">
              </label>
              <span class="accordion-chevron">&#9660;</span>
            </div>
          </div>
          <div class="accordion-body">
            <div class="accordion-body-inner">
              ${dynamicControls}
            </div>
          </div>
        `;
        container.appendChild(item);
      });
    }

    // Card click: activate effect and expand
    function onCardClick(id) {
      activateEffect(id, true);
    }

    function activateEffect(id, sendApi = true) {
      currentEffectId = id;

      // Update accordion expansion
      EFFECTS_META.forEach(e => {
        const el = document.getElementById(`eff-item-${e.id}`);
        if (!el) return;
        if (e.id === id) {
          el.classList.add('active');
          el.classList.add('expanded');
        } else {
          el.classList.remove('active');
          el.classList.remove('expanded');
        }
      });

      if (sendApi) {
        sendControl({ effect: id });
      }
    }

    // InCycle toggle checkbox
    function onCycleToggle(id, isChecked) {
      if (isChecked) {
        cycleMask |= (1 << id);
      } else {
        cycleMask &= ~(1 << id);
      }
      sendControl({ effect: id, inCycle: isChecked, cycle_mask: cycleMask });
    }

    // Slider inputs
    function onBrightnessInput(id, val) {
      isDragging = true;
      const el = document.getElementById(`val-bright-${id}`);
      if (el) el.innerText = `${val} (${Math.round((val / 255) * 100)}%)`;
    }
    function onBrightnessChange(id, val) {
      isDragging = false;
      sendControl({ effect: id, brightness: parseInt(val, 10) });
    }

    function onSpeedInput(id, val) {
      isDragging = true;
      const el = document.getElementById(`val-speed-${id}`);
      if (el) el.innerText = val;
    }
    function onSpeedChange(id, val) {
      isDragging = false;
      sendControl({ effect: id, speed: parseInt(val, 10) });
    }

    function onMicInput(id, val) {
      isDragging = true;
      const el = document.getElementById(`val-mic-${id}`);
      if (el) el.innerText = val;
    }
    function onMicChange(id, val) {
      isDragging = false;
      sendControl({ effect: id, micSensitivity: parseInt(val, 10) });
    }

    function onTempInput(id, val) {
      isDragging = true;
      const kelvin = Math.round(1800 + (val / 255) * 2200);
      const el = document.getElementById(`val-temp-${id}`);
      if (el) el.innerText = `${kelvin}K`;
    }
    function onTempChange(id, val) {
      isDragging = false;
      const v = parseInt(val, 10);
      sendControl({ effect: id, param1: v, colorTemp: v });
    }

    // Schedule actions (Independent Dawn Alarm and Dusk Sleep Timer)
    let isDawnEnabled = false;
    let isDuskEnabled = false;

    function onDawnDurationChange(val) {
      sendControl({ dawnDurationMin: parseInt(val, 10) });
    }

    function onDuskDurationChange(val) {
      sendControl({ duskDurationMin: parseInt(val, 10) });
    }

    function toggleDawn(e) {
      if (e) e.stopPropagation();
      isDawnEnabled = !isDawnEnabled;
      updateDawnUI(isDawnEnabled);
      sendControl({ dawnEnabled: isDawnEnabled });
    }

    function toggleDusk(e) {
      if (e) e.stopPropagation();
      isDuskEnabled = !isDuskEnabled;
      updateDuskUI(isDuskEnabled);
      sendControl({ duskEnabled: isDuskEnabled });
    }

    function updateDawnUI(enabled) {
      isDawnEnabled = enabled;
      const btn = document.getElementById('btn-dawn-toggle');
      const txt = document.getElementById('dawn-status-text');
      if (btn && txt) {
        btn.className = enabled ? 'power-btn on' : 'power-btn';
        txt.innerText = enabled ? 'ВКЛ' : 'ВЫКЛ';
      }
    }

    function updateDuskUI(enabled) {
      isDuskEnabled = enabled;
      const btn = document.getElementById('btn-dusk-toggle');
      const txt = document.getElementById('dusk-status-text');
      if (btn && txt) {
        btn.className = enabled ? 'power-btn on' : 'power-btn';
        txt.innerText = enabled ? 'ВКЛ' : 'ВЫКЛ';
      }
    }

    function saveScheduleTimes() {
      const dawnVal = document.getElementById('sched-dawn-time').value;
      const duskVal = document.getElementById('sched-dusk-time').value;
      sendControl({ dawnTime: dawnVal, duskTime: duskVal });
    }

    // Power toggle
    function togglePower() {
      sendControl({ power: !isPowered });
    }

    // Radio switch (Wi-Fi <-> Zigbee)
    async function switchRadio() {
      const btn = document.getElementById('btn-radio-switch');
      btn.disabled = true;
      const oldText = document.getElementById('radio-btn-text').innerText;
      document.getElementById('radio-btn-text').innerText = 'Переключение...';

      try {
        const res = await fetch('/api/radio/switch', { method: 'POST' });
        const data = await res.json();
        if (data.radio_mode) {
          updateRadioModeUI(data.radio_mode);
        }
      } catch (err) {
        console.error('Radio switch error:', err);
      } finally {
        setTimeout(() => {
          btn.disabled = false;
        }, 1500);
      }
    }

    async function onZbProfileChange(val) {
      try {
        await fetch('/api/radio/profile', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ profile: parseInt(val, 10) })
        });
      } catch (err) {
        console.error('Zb profile error:', err);
      }
    }

    function updateRadioModeUI(mode) {
      currentRadioMode = mode;
      const isZig = (mode === 'zigbee');

      const desc = document.getElementById('radio-mode-desc');
      if (desc) {
        desc.innerText = isZig ? 'Zigbee (802.15.4)' : 'Wi-Fi 2.4 GHz';
        desc.style.color = isZig ? 'var(--purple)' : 'var(--primary)';
      }

      const hBadge = document.getElementById('badge-radio');
      const hText = document.getElementById('header-radio-text');
      if (hBadge && hText) {
        hBadge.className = isZig ? 'badge badge-radio-zigbee' : 'badge badge-radio-wifi';
        hText.innerText = isZig ? 'Zigbee' : 'Wi-Fi';
      }

      const telemRadio = document.getElementById('telem-radio');
      if (telemRadio) {
        telemRadio.innerText = isZig ? 'Zigbee' : 'Wi-Fi';
        telemRadio.style.color = isZig ? 'var(--purple)' : 'var(--text)';
      }

      const btn = document.getElementById('btn-radio-switch');
      const btnIcon = document.getElementById('radio-btn-icon');
      const btnText = document.getElementById('radio-btn-text');
      if (btn && btnIcon && btnText) {
        if (isZig) {
          btn.className = 'radio-switch-btn';
          btnIcon.innerHTML = '&#128246;';
          btnText.innerText = 'Переключить на Wi-Fi';
        } else {
          btn.className = 'radio-switch-btn zigbee-mode';
          btnIcon.innerHTML = '&#9889;';
          btnText.innerText = 'Переключить на Zigbee';
        }
      }
    }

    // General sendControl API
    async function sendControl(payload) {
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

    // Periodic status poll
    async function fetchStatus() {
      try {
        const res = await fetch('/api/status');
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const data = await res.json();

        // Connection badge
        document.getElementById('conn-badge').className = 'badge badge-online';
        document.getElementById('conn-text').innerText = data.isAp ? 'SoftAP Mode' : 'Online';

        // Power state
        isPowered = data.power;
        const btnPower = document.getElementById('btn-power');
        if (data.power) {
          btnPower.className = 'power-btn on';
          document.getElementById('power-text').innerText = 'ВКЛ';
        } else {
          btnPower.className = 'power-btn';
          document.getElementById('power-text').innerText = 'ВЫКЛ';
        }

        // Radio mode & profile
        if (data.radio_mode && data.radio_mode !== currentRadioMode) {
          updateRadioModeUI(data.radio_mode);
        }

        if (data.zb_profile !== undefined) {
          const selZb = document.getElementById('sel-zb-profile');
          if (selZb && document.activeElement !== selZb) {
            selZb.value = String(data.zb_profile);
          }
        }

        // Active effect sync
        if (data.effect !== undefined && data.effect !== currentEffectId) {
          activateEffect(data.effect, false);
        }

        // Cycle mask & profiles sync
        if (data.cycle_mask !== undefined) {
          cycleMask = data.cycle_mask;
        }

        if (data.effects && Array.isArray(data.effects)) {
          data.effects.forEach(eff => {
            const chk = document.getElementById(`chk-cycle-${eff.index}`);
            if (chk && eff.inCycle !== undefined) {
              chk.checked = !!eff.inCycle;
            }

            if (!isDragging) {
              const sBright = document.getElementById(`slider-bright-${eff.index}`);
              if (sBright && eff.brightness !== undefined) {
                sBright.value = eff.brightness;
                const txt = document.getElementById(`val-bright-${eff.index}`);
                if (txt) txt.innerText = `${eff.brightness} (${Math.round((eff.brightness / 255) * 100)}%)`;
              }

              const sSpeed = document.getElementById(`slider-speed-${eff.index}`);
              if (sSpeed && eff.speed !== undefined) {
                sSpeed.value = eff.speed;
                const txt = document.getElementById(`val-speed-${eff.index}`);
                if (txt) txt.innerText = eff.speed;
              }

              const sMic = document.getElementById(`slider-mic-${eff.index}`);
              if (sMic && eff.sensitivity !== undefined) {
                sMic.value = eff.sensitivity;
                const txt = document.getElementById(`val-mic-${eff.index}`);
                if (txt) txt.innerText = eff.sensitivity;
              }

              if (eff.index === 0) {
                const sTemp = document.getElementById(`slider-temp-0`);
                if (sTemp && eff.param1 !== undefined) {
                  sTemp.value = eff.param1;
                  const txt = document.getElementById(`val-temp-0`);
                  if (txt) {
                    const kelvin = Math.round(1800 + (eff.param1 / 255) * 2200);
                    txt.innerText = `${kelvin}K`;
                  }
                }
              }
            }
          });
        }

        // Schedule sync
        const curTimeStr = data.currentTime || data.time;
        if (curTimeStr) {
          const clockEl = document.getElementById('sched-clock');
          if (clockEl) clockEl.innerText = `${curTimeStr} GMT+2`;
          const lampTimeEl = document.getElementById('lamp-time');
          if (lampTimeEl) lampTimeEl.innerText = curTimeStr;
        }

        if (data.dawnDurationMin !== undefined) {
          const dDawnDur = document.getElementById('sched-dawn-dur');
          if (dDawnDur && document.activeElement !== dDawnDur) {
            dDawnDur.value = String(data.dawnDurationMin);
          }
        }
        if (data.duskDurationMin !== undefined) {
          const dDuskDur = document.getElementById('sched-dusk-dur');
          if (dDuskDur && document.activeElement !== dDuskDur) {
            dDuskDur.value = String(data.duskDurationMin);
          }
        }

        if (data.scheduleStatus) {
          const pBadge = document.getElementById('sched-phase-badge');
          const pText = document.getElementById('sched-phase-text');
          if (pText) pText.innerText = data.scheduleStatus;
          if (pBadge) {
            if (data.scheduleStatus.includes('Рассвет')) {
              pBadge.style.background = 'rgba(245, 158, 11, 0.15)';
              pBadge.style.color = '#f59e0b';
              pBadge.style.borderColor = 'rgba(245, 158, 11, 0.4)';
            } else if (data.scheduleStatus.includes('Отбой') || data.scheduleStatus.includes('Заснуть') || data.scheduleStatus.includes('Таймер сна')) {
              pBadge.style.background = 'rgba(168, 85, 247, 0.15)';
              pBadge.style.color = '#a855f7';
              pBadge.style.borderColor = 'rgba(168, 85, 247, 0.4)';
            } else {
              pBadge.style.background = 'rgba(56, 189, 248, 0.12)';
              pBadge.style.color = 'var(--primary)';
              pBadge.style.borderColor = 'rgba(56, 189, 248, 0.3)';
            }
          }
        }

        if (data.dawnEnabled !== undefined) {
          updateDawnUI(data.dawnEnabled);
        }
        if (data.duskEnabled !== undefined) {
          updateDuskUI(data.duskEnabled);
        }
        const dawnInp = document.getElementById('sched-dawn-time');
        if (dawnInp && document.activeElement !== dawnInp && data.dawnTime) {
          dawnInp.value = data.dawnTime;
        }
        const duskInp = document.getElementById('sched-dusk-time');
        if (duskInp && document.activeElement !== duskInp && data.duskTime) {
          duskInp.value = data.duskTime;
        }

        // Telemetry update
        document.getElementById('telem-temp').innerText = (data.cpuTemp !== undefined) ? data.cpuTemp.toFixed(1) : '--';
        document.getElementById('telem-rssi').innerText = (data.rssi !== undefined) ? data.rssi : '--';
        document.getElementById('telem-ip').innerText = data.ip || '--';
        document.getElementById('telem-uptime').innerText = formatUptime(data.uptime || 0);
        document.getElementById('telem-heap').innerText = data.freeHeap || '--';
        document.getElementById('telem-ver').innerText = 'v' + (data.version || '0.9.7');

        // OTA info
        document.getElementById('ota-cur-ver').innerText = 'v' + (data.version || '0.9.7');
        document.getElementById('ota-latest-ver').innerText = data.latestVersion ? ('v' + data.latestVersion) : 'не проверялась';
        document.getElementById('ota-status').innerText = data.otaStatus || 'Idle';

        // Flight recorder / Zigbee session log
        if (data.last_zb_log !== undefined) {
          const zbLogEl = document.getElementById('zb-session-log');
          if (zbLogEl) zbLogEl.innerText = data.last_zb_log;
        }

        // WiFi current SSID
        document.getElementById('wifi-cur-ssid').innerText = data.ssid || (data.isAp ? 'MagicLamp-Setup' : 'Не подключено');

      } catch (err) {
        document.getElementById('conn-badge').className = 'badge badge-offline';
        document.getElementById('conn-text').innerText = 'Disconnected';
      }
    }

    // OTA operations
    async function triggerGithubOta() {
      const btn = document.getElementById('btn-ota-check');
      btn.disabled = true;
      btn.innerText = 'Запрос...';
      try {
        await fetch('/api/ota/check', { method: 'POST' });
        document.getElementById('ota-status').innerText = 'Triggered GitHub check';
      } catch (e) {
        console.error('OTA error:', e);
      }
      setTimeout(() => {
        btn.disabled = false;
        btn.innerHTML = '&#8635; Проверить на GitHub';
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

    // Initialize UI
    renderAccordion();
    fetchStatus();
    setInterval(fetchStatus, 1500);
  </script>
</body>
</html>
)rawliteral";
