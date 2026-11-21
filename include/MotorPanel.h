// Página del panel web embebida en el firmware. Contiene la interfaz, sus estilos y los comandos HTTP.
#pragma once

#include <pgmspace.h>

constexpr char motorPanelHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>MotorPAP · TMC5160</title>
  <link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20viewBox%3D%220%200%2032%2032%22%3E%3Ctext%20x%3D%2216%22%20y%3D%2226%22%20fill%3D%22%230875db%22%20text-anchor%3D%22middle%22%20font-family%3D%22Segoe%20UI%20Symbol%2C%20DejaVu%20Sans%2C%20sans-serif%22%20font-size%3D%2230%22%3E%E2%9A%99%EF%B8%8E%3C%2Ftext%3E%3C%2Fsvg%3E">
  <style>
    /* Variables de color y estilos base compartidos por todo el panel. */
    :root {
      font: 14px "Segoe UI", system-ui, sans-serif;
      color: #111b2d;
      background: #f7f8fa;
      --line: #d7dee8;
      --muted: #43516a;
      --blue: #0867d8;
    }
    * { box-sizing: border-box; }
    body { margin: 0; min-height: 100vh; display: flex; flex-direction: column; }
    header { background: linear-gradient(#fff, #f8f9fb); border-bottom: 1px solid var(--line); }
    .top, .control-panel, .foot, .log-panel, .wifi-panel { max-width: 1600px; width: 100%; margin: 0 auto; padding: .9rem clamp(.7rem, 1.1vw, 1.25rem); }
    /* Pestañas de navegación con una banda azul grisácea y la activa unida al contenido. */
    .panel-tabs { background: #456375; }
    .panel-tabs > [role="tablist"] {
      max-width: 1600px;
      margin: 0 auto;
      padding: .45rem clamp(.7rem, 1.1vw, 1.25rem) 0;
      display: flex;
      gap: .25rem;
    }
    .panel-tabs button[role="tab"] {
      flex: 0 1 9rem;
      min-width: 0;
      min-height: 2.35rem;
      padding: .42rem clamp(.3rem, 2vw, .85rem);
      text-align: left;
      border: 1px solid #a0b7c8;
      border-bottom: 0;
      border-radius: .6rem .6rem 0 0;
      background: #a7c0d2;
      color: #213949;
      cursor: pointer;
    }
    .panel-tabs button[role="tab"]:hover { background: #c6d9e6; }
    .panel-tabs button[aria-selected="true"] {
      background: #f7f8fa;
      color: #111b2d;
      border-color: #f7f8fa;
      font-weight: 700;
    }
    .panel-tabs button[role="tab"]:focus-visible { outline: 2px solid #0954a6; outline-offset: -4px; }
    [role="tabpanel"][hidden] { display: none; }
    .log-panel .row { flex-wrap: wrap; margin-top: 0; }
    .log-panel pre { max-height: 60vh; overflow: auto; white-space: pre-wrap; overflow-wrap: anywhere; }
    .wifi-panel .card { max-width: 35rem; }
    .wifi-form { display: grid; gap: .65rem; margin-top: .75rem; }
    .wifi-form label { display: grid; gap: .25rem; }
    .wifi-form input { width: 100%; padding: .4rem .5rem; border: 1px solid #bbc8d8; border-radius: .35rem; font: inherit; }
    .wifi-form button { justify-self: start; }
    #wifi-address-settings, #wifi-address-manual:not([hidden]) { display: grid; gap: .55rem; }
    #wifi-address-settings { border: 1px solid var(--line); border-radius: .4rem; padding: .65rem; }
    #wifi-address-method { justify-self: start; }
    #wifi-address-feedback:empty { display: none; }
    .wifi-panel [hidden] { display: none; }
    .wifi-search { display: flex; flex-wrap: wrap; gap: .5rem; margin: .75rem 0; }
    .wifi-mode { margin: .75rem 0; border: 1px solid var(--line); border-radius: .4rem; }
    .wifi-mode label { display: inline-flex; align-items: center; gap: .25rem; margin: .2rem .6rem .2rem 0; }
    .wifi-results { list-style: none; margin: .35rem 0; padding: 0; }
    .wifi-results li + li { border-top: 1px solid var(--line); }
    .wifi-results label { display: grid; grid-template-columns: auto 1fr auto; align-items: center; gap: .45rem; padding: .45rem 0; cursor: pointer; }
    .wifi-results strong { overflow-wrap: anywhere; }
    .wifi-results small { text-align: right; }
    .top { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 1.15rem; }
    h1, h2, p { margin: 0; }
    h1 { font-size: clamp(1.45rem, 2.4vw, 2rem); letter-spacing: -.035em; }
    h2 { font-size: 1.07rem; letter-spacing: -.02em; }
    .intro { display: flex; align-items: center; gap: .85rem; }
    .brand-icon { color: #0875db; font-size: 3.1rem; line-height: 1; }
    .intro p, small { color: var(--muted); }
    .intro p { margin-top: .2rem; }
    /* Distribución y aspecto de las tarjetas de estado. */
    .status-grid { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: .65rem; flex: 1 1 34rem; max-width: 56rem; }
    .card { background: linear-gradient(145deg, #fff, #f4f6f9); border: 1px solid var(--line); border-radius: .65rem; padding: .7rem .85rem; box-shadow: 0 1px 2px #182c4210; }
    .status-grid .card { display: flex; align-items: center; gap: .65rem; min-height: 4.7rem; }
    .status-copy { display: grid; justify-items: start; gap: .15rem; }
    .status-copy strong { font-size: .86rem; }
    .icon { display: inline-block; font-size: 1.15em; line-height: 1; color: #263549; margin-right: .35rem; }
    .status-grid .icon { font-size: 1.85rem; margin: 0; }
    .wifi-icon { color: #159444; }
    .pill { display: inline-block; padding: .15rem .55rem; border-radius: .55rem; background: #e9edf2; color: #43516a; font-size: .8rem; font-weight: 700; }
    .pill.online { background: #ccf9d2; color: #08721a; }
    .pill.pending { background: #fff1d5; color: #734810; }
    .pill.fault { background: #ffe1dd; color: #a4211c; }
    .control-panel { flex: 1; display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: .75rem; align-items: start; align-content: start; }
    #motor-panel {
      grid-template-columns: repeat(2, minmax(0, 1fr));
      grid-template-areas: "driver speed" "diagnostics speed";
    }
    .motor-driver { grid-area: driver; }
    .motor-speed { grid-area: speed; }
    .motor-diagnostics { grid-area: diagnostics; }
    #motor-feedback:empty { display: none; }
    /* Espaciado de los grupos de ajustes y sus controles. */
    .stack { display: grid; gap: .65rem; min-width: 0; }
    fieldset { min-width: 0; border: 0; margin: 0; padding: 0; }
    .row { display: flex; align-items: center; justify-content: space-between; gap: .6rem; }
    .row, .range, .choices, .actions { margin-top: .55rem; }
    label { font-size: .87rem; }
    input, button, select { font: inherit; }
    input[type="number"], button, #pin-controls select, .wifi-panel select { border: 1px solid #bbc8d8; border-radius: .35rem; padding: .36rem .45rem; min-height: 2rem; }
    input[type="number"] { width: 5.8rem; background: #f3f6fa; color: var(--muted); opacity: 1; }
    input[type="range"] { min-width: 0; flex: 1; }
    .range { display: flex; align-items: center; gap: .4rem; }
    .limits { display: flex; justify-content: space-between; margin-top: .25rem; }
    .rpm { display: block; margin-top: .45rem; color: var(--muted); }
    .choices { display: flex; flex-wrap: wrap; gap: .4rem; }
    button:disabled { opacity: 1; cursor: not-allowed; }
    .choices button, #pin-controls select, .wifi-panel select { color: #25334a; background: linear-gradient(#fff, #edf1f6); min-height: 1.9rem; padding: .2rem .35rem; }
    .choices button { flex: 1; }
    select:not(:disabled) { cursor: pointer; }
    .choices button[aria-pressed="true"] { background: var(--blue); color: white; }
    .driver-toggle { white-space: nowrap; }
    .driver-toggle input { position: absolute; opacity: 0; }
    .switch { display: inline-flex; align-items: center; gap: .45rem; padding: .25rem .35rem .25rem .55rem; border-radius: 1.1rem; background: #667587; color: white; font-size: .7rem; font-weight: 700; }
    .switch::after { content: ""; width: 1.15rem; height: 1.15rem; border-radius: 50%; background: #fff; }
    .driver-toggle input:checked + .switch { background: #00a76a; }
    .driver-toggle input:focus-visible + .switch { outline: 2px solid var(--blue); outline-offset: 2px; }
    .driver-toggle input:not(:disabled) + .switch { cursor: pointer; }
    /* Distribución de los indicadores y botones de movimiento. */
    .metrics, .actions { display: grid; grid-template-columns: 1fr 1fr; gap: .55rem; }
    .metrics .card { min-height: 5.15rem; }
    .metric { display: inline-block; color: #142238; font-size: 1.85rem; font-weight: 750; margin: .45rem .4rem 0 0; }
    .metrics .card:first-child .metric { color: #1239da; }
    .position-readings { display: flex; align-items: baseline; flex-wrap: wrap; column-gap: .85rem; }
    .position-readings span { overflow-wrap: anywhere; }
    .actions.jog { grid-template-columns: repeat(3, 1fr); }
    .actions button, .position button, .gcode-tools button, .wifi-panel button { background: linear-gradient(#2179df, #1059ab); border-color: #1162ba; color: white; font-weight: 700; min-height: 2.55rem; box-shadow: 0 2px 4px #17304e25; }
    .actions.jog button { min-height: 5.2rem; padding-block: .4rem; }
    .actions .stop { grid-column: 1 / -1; }
    .actions .stop, .gcode-tools .stop, .position .stop { background: linear-gradient(#f74132, #cb291f); border-color: #c72922; }
    .button-icon {
      position: relative; display: grid; place-items: center; flex: none;
      width: 2.1rem; height: 2.1rem; margin: 0 auto .25rem; border-radius: .65rem;
      background: linear-gradient(#ffffff32, #ffffff14); border: 1px solid #ffffff45;
      box-shadow: inset 0 1px 1px #ffffff20, 0 2px 4px #10284d38;
    }
    .button-icon::before, .button-icon::after { content: ""; position: absolute; }
    .button-icon.arrow::before {
      width: 1.05rem; height: 2px; background: currentColor; border-radius: 2px;
    }
    .button-icon.arrow::after {
      right: .49rem; width: .48rem; height: .48rem;
      border-top: 2px solid; border-right: 2px solid; border-radius: 1px;
      transform: rotate(45deg);
    }
    .button-icon.arrow-left { transform: scaleX(-1); }
    .button-icon.home::before {
      top: .49rem; width: .74rem; height: .74rem; border-top: 2px solid; border-left: 2px solid;
      border-radius: 2px 0 0; transform: rotate(45deg);
    }
    .button-icon.home::after {
      top: .86rem; width: .86rem; height: .68rem; border: 2px solid; border-top: 0;
      border-radius: 0 0 2px 2px;
      background: linear-gradient(currentColor, currentColor) center bottom / 2px .35rem no-repeat;
    }
    .actions .stop { display: flex; align-items: center; justify-content: center; gap: .45rem; }
    .button-icon.stop-icon { width: 1.5rem; height: 1.5rem; margin: 0; border-radius: .42rem; }
    .button-icon.stop-icon::before {
      width: .62rem; height: .62rem; border-radius: 2px; background: currentColor;
    }
    .button-icon.stop-icon::after { content: none; }
    .actions small { display: block; color: #edf5ff; }
    .position { container-type: inline-size; }
    .position-operations, .position-calibration { display: grid; gap: .85rem; margin-top: .65rem; }
    .position-operation { display: grid; gap: .45rem .65rem; align-items: end; }
    .position-operation button { justify-self: start; }
    .position-destinations { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: .45rem; }
    .position-commands { display: flex; flex-wrap: wrap; gap: .45rem; }
    .position label, .position-home-readout { display: grid; gap: .25rem; min-width: 0; }
    .position input[type="number"] { width: 100%; }
    .position-home-value { display: flex; align-items: baseline; gap: .3rem; min-height: 2rem; padding-top: .2rem; }
    .position-home-value strong { font-size: 1.15rem; overflow-wrap: anywhere; }
    .position-calibration {
      grid-template-columns: repeat(2, minmax(0, 1fr)); border-top: 1px solid var(--line); padding-top: .65rem;
    }
    .position-calibration input { max-width: 14rem; }
    @container (min-width: 22rem) {
      .position-operation { grid-template-columns: minmax(0, 1fr) auto; }
    }
    @container (min-width: 39rem) {
      .position-operations { grid-template-columns: repeat(2, minmax(0, 1fr)); }
      .position-operation + .position-operation { border-left: 1px solid var(--line); padding-left: .85rem; }
    }
    @container (max-width: 63.99rem) {
      #position-form { grid-template-columns: minmax(0, 1fr); }
    }
    .diagnostics { margin: .55rem 0 0; }
    .diagnostics div { display: flex; justify-content: space-between; gap: .5rem; padding: .4rem 0; border-top: 1px solid var(--line); font-size: .87rem; }
    .diagnostics dt::before { content: ""; display: inline-block; width: .65rem; height: .65rem; margin: 0 .55rem 0 .1rem; border-radius: 50%; background: #838b96; }
    .diagnostics [data-level="ok"] dt::before { background: #159444; }
    .diagnostics [data-level="warn"] dt::before { background: #bf7900; }
    .diagnostics [data-level="fault"] dt::before { background: #cc3026; }
    .diagnostics dd { margin: 0; color: var(--muted); text-align: right; }
    .series-tabs { display: flex; gap: .2rem; }
    .telemetry-head { display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: .5rem; }
    .telemetry-actions { display: flex; align-items: center; flex-wrap: wrap; gap: .45rem; }
    .series-tabs button { background: #fff; color: var(--blue); cursor: pointer; }
    .series-tabs [aria-pressed="true"] { background: var(--blue); color: #fff; }
    .plot {
      position: relative; margin-top: .45rem; border: 1px solid var(--line);
      border-radius: .35rem; color: var(--muted); background: #f8fafd;
    }
    .plot canvas { display: block; width: 100%; height: 12rem; }
    .plot span { position: absolute; inset: 48% .5rem auto; text-align: center; }
    .plot-summary { display: flex; justify-content: space-between; flex-wrap: wrap; gap: .35rem; margin-top: .45rem; }
    .axis { display: block; text-align: center; margin-top: .3rem; }
    .gcode-tools { justify-content: flex-start; flex-wrap: wrap; }
    .gcode-tools label { display: flex; align-items: center; gap: .35rem; flex-wrap: wrap; }
    .gcode-tools input[type="file"] { max-width: 100%; }
    .gcode-tools input::file-selector-button {
      font: inherit; color: #25334a; background: linear-gradient(#fff, #edf1f6);
      border: 1px solid #bbc8d8; border-radius: .35rem; padding: .36rem .45rem;
    }
    .gcode-tools button:disabled, .wifi-panel :is(button, select):disabled { opacity: .55; }
    .movement-programs { align-self: stretch; grid-template-rows: auto 1fr; }
    .gcode-card { display: flex; flex-direction: column; min-width: 0; }
    .gcode-source {
      --gcode-padding: .55rem;
      --gcode-line-height: 1.5rem;
      position: relative; display: flex; flex: 1; overflow: hidden; margin-top: .55rem;
      border: 1px solid #bbc8d8; border-radius: .35rem; background: #f8fafd;
      font: .92rem/var(--gcode-line-height) ui-monospace, Consolas, monospace;
    }
    .gcode-source:focus-within { outline: 2px solid #2179df; }
    .gcode-numbers {
      position: relative; flex: none; overflow: hidden; min-width: 3ch;
      padding-inline: var(--gcode-padding); background: #e8eff7; color: #405775; text-align: right;
    }
    .gcode-numbers pre {
      height: 0; margin: 0; padding-block: var(--gcode-padding); font: inherit;
    }
    .gcode-active-line {
      position: absolute; inset-inline: 0; height: var(--gcode-line-height); pointer-events: none;
      background: #c3defb; border-left: 3px solid #2179df;
    }
    .gcode-editor {
      position: relative; display: block; flex: 1; min-width: 0; min-height: 10rem;
      resize: vertical; padding: var(--gcode-padding); border: 0; border-radius: 0; outline: none;
      background: transparent; color: #111b2d; font: inherit; white-space: pre; overflow: auto;
    }
    footer { background: linear-gradient(#203a5a, #172d48); color: #f1f5fa; }
    .foot { display: flex; flex-wrap: wrap; align-items: center; gap: .5rem 1.1rem; font-size: .86rem; }
    .foot span + span { border-left: 1px solid #6d8099; padding-left: 1.1rem; }
    .foot-credit { display: flex; align-items: center; gap: .35rem; margin-inline-start: auto; }
    .foot-credit button {
      padding: .1rem .3rem; border: 0; background: transparent; color: inherit;
      font-size: 1.25rem; line-height: 1; cursor: pointer;
    }
    .foot-credit button:hover { background: #ffffff26; }
    #copyright-notice {
      width: min(42rem, calc(100% - 2rem)); padding: 1.25rem;
      border: 1px solid var(--line); border-radius: .6rem; color: inherit;
    }
    #copyright-notice::backdrop { background: #111b2d80; }
    .copyright-text { max-height: 65vh; overflow: auto; padding-inline: .2rem .75rem; line-height: 1.5; }
    #copyright-notice form { margin-top: .85rem; text-align: right; }
    /* Adapta la distribución a pantallas medianas y estrechas. */
    @media (min-width: 68rem) {
      #motor-panel {
        grid-template-columns: repeat(3, minmax(0, 1fr));
        grid-template-areas: "driver speed diagnostics";
      }
    }
    @media (min-width: 68rem) and (max-height: 43rem),
           (min-width: 43rem) and (max-width: 68rem) and (max-height: 64rem) {
      .top, .control-panel, .foot { padding-block: .4rem; }
      .status-grid .card { min-height: 3.25rem; padding-block: .28rem; }
      #motor-panel .stack { gap: .4rem; }
      #motor-panel .card { padding-block: .43rem; }
      .motor-speed .range { margin-top: .25rem; }
      .motor-speed .limits, .motor-speed .rpm { margin-top: .15rem; }
    }
    @media (max-width: 43rem) {
      .control-panel { grid-template-columns: 1fr; }
      #motor-panel {
        grid-template-columns: 1fr;
        grid-template-areas: "driver" "speed" "diagnostics";
      }
      .status-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
      .foot span + span { padding-left: 0; border: 0; }
    }
  </style>
</head>
<body>
<!-- Cabecera y tarjetas de estado de conexión, controlador, driver y motor. -->  <header>
    <div class="top">
      <div class="intro">
        <span class="brand-icon" aria-hidden="true">⚙︎</span>
        <div><h1>Control Motor PAP - TMC5160</h1>
        <p>Control de motor paso a paso (PAP) con driver TMC5160</p></div>
      </div>
      <div class="status-grid">
        <div class="card"><span class="icon wifi-icon" aria-hidden="true">◔</span><div class="status-copy"><strong>Wi-Fi</strong><span class="pill" id="wifi">Sin datos</span><small id="signal">— dBm</small></div></div>
        <div class="card"><span class="icon" aria-hidden="true">▣</span><div class="status-copy"><strong>Controlador</strong><span class="pill" id="controller">Sin datos</span><small>ESP8285</small></div></div>
        <div class="card"><span class="icon" aria-hidden="true">▣</span><div class="status-copy"><strong>Driver TMC5160</strong><span class="pill" id="driver-link">Sin datos</span></div></div>
        <div class="card"><span class="icon" aria-hidden="true">▥</span><div class="status-copy"><strong>Motor</strong><span class="pill" id="motor-state" role="status">Sin datos</span></div></div>
      </div>
    </div>
  </header>
  <div class="panel-tabs">
    <div role="tablist" aria-label="Panel MotorPAP">
      <button type="button" role="tab" id="movement-tab" aria-controls="movement-panel" aria-selected="true">Movimiento</button>
      <button type="button" role="tab" id="motor-tab" aria-controls="motor-panel" aria-selected="false" tabindex="-1">Motor</button>
      <button type="button" role="tab" id="wifi-tab" aria-controls="wifi-panel" aria-selected="false" tabindex="-1">Wi-Fi</button>
      <button type="button" role="tab" id="log-tab" aria-controls="log-panel" aria-selected="false" tabindex="-1">Registro</button>
    </div>
  </div>
<!-- Ajustes del motor y diagnóstico. -->  <main class="control-panel" id="motor-panel" role="tabpanel" aria-labelledby="motor-tab" hidden>
    <section class="stack motor-driver" aria-label="Ajustes del driver">
      <section class="card row" style="margin-top:0">
        <div><h2><span class="icon" aria-hidden="true">🔒︎</span>Habilitar driver</h2><small data-current-summary>0,80 A RMS en marcha · 0,40 A RMS en reposo</small></div>
        <label class="driver-toggle"><input data-driver-toggle type="checkbox" role="switch" aria-label="Habilitar driver" disabled><span class="switch" aria-hidden="true">OFF</span></label>
      </section>
<!-- Corriente RMS de marcha ajustable y valor de mantenimiento calculado por el firmware. -->      <fieldset class="card" id="current-controls" disabled>
        <h2><span class="icon" aria-hidden="true">ϟ</span>Corriente del motor</h2>
        <div class="choices">
          <label>Marcha RMS<br><input id="run-current" type="number" min="0.10" max="1.20" step="0.01" value="0.80" required> A</label>
          <label>Mantenimiento RMS<br><input id="hold-current" type="number" value="0.40" readonly> A</label>
        </div>
        <div class="row"><small>0,10 a 1,20 A RMS · Ajusta con el driver en OFF.</small><button type="button" id="apply-current">Aplicar</button></div>
      </fieldset>
      <section class="card row" style="margin-top:0">
        <div><h2>Modo silencioso</h2><small id="chopper-mode">SpreadCycle · Ajusta con el driver en OFF.</small></div>
        <label class="driver-toggle"><input id="silent-mode-toggle" type="checkbox" role="switch" aria-label="Modo silencioso StealthChop" disabled><span class="switch" id="silent-mode-switch" aria-hidden="true">OFF</span></label>
      </section>
      <fieldset class="card" id="pin-controls" disabled aria-labelledby="pin-heading">
        <h2 id="pin-heading">Polaridad de los pines</h2>
        <div class="choices">
          <label>Dirección (DIR)<br><select data-invert-mask="1"><option value="0">Normal</option><option value="1">Invertido</option></select></label>
          <label>Enable (EN)<br><select data-invert-mask="2"><option value="0">Normal</option><option value="1">Invertido</option></select></label>
          <label>CLK (STEP)<br><select data-invert-mask="4"><option value="0">Normal</option><option value="1">Invertido</option></select></label>
        </div>
        <div class="row"><small>Ajusta con el driver en OFF. Usa EN normal con cableado directo.</small><button type="button" id="apply-pins">Aplicar</button></div>
      </fieldset>
      <p id="motor-feedback" role="status"></p>
    </section>
    <section class="stack motor-speed" aria-label="Rampas de velocidad">
      <section class="card">
        <h2><span class="icon" aria-hidden="true">◿</span>Tiempo de aceleración</h2>
        <div class="range">
          <input id="ramp-slider" type="range" min="0" max="1000" step="1" value="80" aria-label="Ajustar tiempo de aceleración" aria-valuetext="0,20 segundos">
          <input id="jog-ramp-time" type="number" min="0.10" max="600" step="0.01" value="0.20" required aria-label="Tiempo de aceleración en segundos"><small>s</small>
        </div>
        <div class="limits"><small>0,10 s</small><small>600 s</small></div>
        <small>Desde reposo hasta la velocidad elegida.</small>
        <p class="rpm"><output id="ramp-acceleration" for="jog-speed ramp-slider jog-ramp-time">—</output></p>
      </section>
      <section class="card">
        <h2><span class="icon" aria-hidden="true">◹</span>Tiempo de deceleración</h2>
        <div class="range">
          <input id="deceleration-slider" type="range" min="0" max="1000" step="1" value="80" aria-label="Ajustar tiempo de deceleración" aria-valuetext="0,20 segundos">
          <input id="jog-deceleration-time" type="number" min="0.10" max="600" step="0.01" value="0.20" required aria-label="Tiempo de deceleración en segundos"><small>s</small>
        </div>
        <div class="limits"><small>0,10 s</small><small>600 s</small></div>
        <small>Desde la velocidad actual hasta reposo al pulsar STOP.</small>
        <p class="rpm"><output id="ramp-deceleration" for="jog-speed deceleration-slider jog-deceleration-time">—</output></p>
      </section>
    </section>
    <section class="stack motor-diagnostics" aria-label="Diagnóstico y micropasos">
      <section class="card">
        <h2><span class="icon" aria-hidden="true">⌁</span>Estado del TMC5160</h2>
        <dl class="diagnostics">
          <div><dt>Identificación SPI</dt><dd id="driver-id">—</dd></div>
          <div><dt>Entrada EN</dt><dd id="driver-enable">—</dd></div>
          <div><dt>OT · Sobretemperatura</dt><dd id="diag-temperature">—</dd></div>
          <div><dt>SG · StallGuard</dt><dd id="diag-stallguard">—</dd></div>
          <div><dt>Error</dt><dd id="diag-error">—</dd></div>
        </dl>
      </section>
      <fieldset class="card" id="microstep-controls" disabled>
        <h2><span class="icon" aria-hidden="true">▱</span>Microstepping</h2>
        <div class="choices" role="group" aria-label="Micropasos por paso">
          <button type="button" value="1" aria-pressed="false">1</button>
          <button type="button" value="2" aria-pressed="false">2</button>
          <button type="button" value="4" aria-pressed="false">4</button>
          <button type="button" value="8" aria-pressed="false">8</button>
          <button type="button" value="16" aria-pressed="true">16</button>
          <button type="button" value="32" aria-pressed="false">32</button>
          <button type="button" value="64" aria-pressed="false">64</button>
          <button type="button" value="128" aria-pressed="false">128</button>
          <button type="button" value="256" aria-pressed="false">256</button>
        </div>
        <small>Cámbialo con el driver en OFF.</small>
      </fieldset>
    </section>
  </main>
  <section class="control-panel" id="movement-panel" role="tabpanel" aria-labelledby="movement-tab">
    <div class="stack">
      <section class="card row" style="margin-top:0">
        <div><h2><span class="icon" aria-hidden="true">🔒︎</span>Habilitar driver</h2><small data-current-summary>0,80 A RMS en marcha · 0,40 A RMS en reposo</small></div>
        <label class="driver-toggle"><input data-driver-toggle type="checkbox" role="switch" aria-label="Habilitar driver" disabled><span class="switch" aria-hidden="true">OFF</span></label>
      </section>
      <section class="card">
        <h2><span class="icon" aria-hidden="true">◴</span>Velocidad</h2>
        <div class="range"><input id="speed-slider" type="range" min="100" max="29333" value="100" aria-label="Ajustar velocidad"><input id="jog-speed" type="number" min="100" max="29333" step="1" value="100" required aria-label="Velocidad en micropasos por segundo"><small>µpasos/s</small></div>
        <div class="limits"><small>100</small><small id="speed-limit">29.333 µpasos/s · máx. 550 RPM</small></div>
        <p class="rpm"><output id="target-rpm" for="speed-slider jog-speed">—</output>
          <small> · 200 pasos/vuelta × <span id="rpm-microsteps">16</span> micropasos</small></p>
      </section>
<!-- Órdenes de movimiento manual y parada. -->      <fieldset class="card" aria-labelledby="movement">
        <h2 id="movement"><span class="icon" aria-hidden="true">✥</span>Controles de movimiento</h2>
        <div class="actions jog">
          <button id="jog-negative" type="button" disabled><span class="button-icon arrow arrow-left" aria-hidden="true"></span>Mover −<small>(Jog)</small></button>
          <button id="move-home" type="button" disabled><span class="button-icon home" aria-hidden="true"></span>Inicio</button>
          <button id="jog-positive" type="button" disabled><span class="button-icon arrow" aria-hidden="true"></span>Mover +<small>(Jog)</small></button>
        </div>
        <div class="actions"><button class="stop" id="stop-motion" type="button" disabled><span class="button-icon stop-icon" aria-hidden="true"></span>STOP</button></div>
        <p id="motion-feedback" role="status" class="row"></p>
      </fieldset>
      <section class="card position" aria-labelledby="position">
        <h2 id="position"><span class="icon" aria-hidden="true">◎</span>Posicionamiento</h2>
        <div class="position-operations">
          <form id="position-form" class="position-operation" novalidate>
            <div class="position-destinations">
              <label>Posición (pasos)<input type="number" id="position-input" min="-1000000" max="1000000" step="1" required disabled></label>
              <label>Posición (mm)<input type="number" id="position-input-mm" step="any" required disabled></label>
            </div>
            <div class="position-commands">
              <button type="submit" id="move-position" disabled>Mover a posición</button>
              <button type="button" class="stop" id="cancel-position" disabled>Cancelar movimiento</button>
            </div>
          </form>
          <div class="position-operation">
            <div class="position-home-readout">
              <small>Posición de inicio</small>
              <div class="position-home-value"><strong id="home-position">—</strong><small>pasos</small></div>
            </div>
            <button type="button" id="set-home" disabled>Establecer posición de inicio</button>
          </div>
        </div>
        <div class="position-calibration">
          <label>Pasos por milímetro<input id="steps-per-mm" type="number" min="0.001" max="1000000" step="any" autocomplete="off" required></label>
          <label>Milímetros por vuelta<input id="mm-per-revolution" type="number" min="0.0002" max="200000" step="any" value="10" autocomplete="off" required></label>
        </div>
      </section>
      <section class="metrics" aria-label="Lecturas de posición">
        <div class="card">
          <h2>Posición objetivo</h2>
          <div class="position-readings">
            <div><span class="metric" id="position-target">—</span><small>pasos</small></div>
            <div><span class="metric" id="position-target-mm">—</span><small>mm</small></div>
          </div>
        </div>
        <div class="card">
          <h2>Posición actual</h2>
          <div class="position-readings">
            <div><span class="metric" id="position-current">—</span><small>pasos</small></div>
            <div><span class="metric" id="position-current-mm">—</span><small>mm</small></div>
          </div>
        </div>
      </section>
    </div>
    <div class="stack movement-programs">
      <section class="card">
        <div class="telemetry-head">
          <h2><span class="icon" aria-hidden="true">↗</span>Telemetría en tiempo real</h2>
          <div class="telemetry-actions">
            <label class="driver-toggle">
              <input id="telemetry-toggle" type="checkbox" role="switch" aria-label="Activar telemetría" checked>
              <span class="switch" id="telemetry-switch" aria-hidden="true">ON</span>
            </label>
            <div class="series-tabs" aria-label="Serie de telemetría">
              <button type="button" data-series="position" aria-pressed="true">Posición</button>
              <button type="button" data-series="speed" aria-pressed="false">Velocidad</button>
            </div>
          </div>
        </div>
        <div class="plot-summary"><small id="plot-readout">Posición (pasos)</small><small>Últimos 30 s</small></div>
        <div class="plot">
          <canvas id="telemetry-plot" aria-hidden="true"></canvas>
          <span id="plot-status" role="status">Esperando datos…</span>
        </div>
        <small class="axis">Tiempo (s)</small>
        <small id="plot-connection" role="status" hidden></small>
      </section>
      <section class="card gcode-card" aria-labelledby="gcode-heading">
        <h2 id="gcode-heading">Código G</h2>
        <div class="row gcode-tools">
          <label>Cargar archivo <input id="gcode-file" type="file" accept=".nc,.ngc,.gcode,.tap,.txt"></label>
        </div>
        <div class="gcode-source">
          <div class="gcode-numbers" aria-hidden="true"><pre id="gcode-numbers">1</pre></div>
          <div class="gcode-active-line" id="gcode-active-line" aria-hidden="true" hidden></div>
          <textarea class="gcode-editor" id="gcode-editor" aria-label="Editor de código G" spellcheck="false"
                    wrap="off" maxlength="65536" placeholder="G21 G90&#10;G0 X0&#10;G1 X5 F60&#10;M30"></textarea>
        </div>
        <div class="row gcode-tools">
          <button type="button" id="gcode-run" disabled>Ejecutar programa</button>
          <button type="button" class="stop" id="gcode-stop" disabled>Parar programa</button>
          <button type="button" id="gcode-save" disabled>Guardar código G</button>
          <small id="gcode-status" role="status">Eje X · G0, G1, G20/G21 y G90/G91</small>
        </div>
      </section>
    </div>
  </section>
  <section class="log-panel" id="log-panel" role="tabpanel" aria-labelledby="log-tab" hidden>
    <div class="card">
      <div class="row">
        <small>Últimos 48 eventos en RAM. El historial se borra al reiniciar el ESP.</small>
        <button type="button" id="download-log" disabled>Descargar TXT</button>
      </div>
      <p id="log-status" role="status" hidden></p>
      <pre id="log-content" tabindex="0" aria-label="Historial de eventos">Cargando registro…</pre>
    </div>
  </section>
  <section class="wifi-panel" id="wifi-panel" role="tabpanel" aria-labelledby="wifi-tab" hidden>
    <div class="card">
      <h2>Conexión Wi-Fi</h2>
      <p role="status"><span class="pill" id="wifi-connection">Comprobando…</span> <strong id="wifi-network"></strong></p>
      <fieldset class="wifi-mode">
        <legend>Modo Wi-Fi</legend>
        <label><input type="radio" name="wifi-mode" value="station" checked> Conectar a una red</label>
        <label><input type="radio" name="wifi-mode" value="ap"> Punto de acceso</label>
      </fieldset>
      <form class="wifi-form" id="wifi-address-form" autocomplete="off">
        <fieldset id="wifi-address-settings" disabled>
          <legend>Configuración IP</legend>
          <label>Asignación de dirección
            <select name="method" id="wifi-address-method">
              <option value="dhcp">Automática (DHCP)</option>
              <option value="manual">Manual</option>
            </select>
          </label>
          <fieldset id="wifi-address-manual" hidden disabled aria-label="Dirección manual">
            <label>Dirección IP<input name="ip" inputmode="decimal" maxlength="15" required></label>
            <label>Máscara de subred<input name="mask" inputmode="decimal" maxlength="15" required></label>
            <label>Puerta de enlace<input name="gateway" inputmode="decimal" maxlength="15" required></label>
          </fieldset>
          <small id="wifi-address-help"></small>
          <div class="row"><button type="submit">Guardar configuración IP</button></div>
        </fieldset>
        <small id="wifi-address-feedback" role="status"></small>
      </form>
      <div id="wifi-station">
        <div class="wifi-search">
          <button type="button" id="wifi-scan">Buscar redes Wi-Fi</button>
          <button type="button" id="wifi-manual">Introducir SSID</button>
        </div>
        <p id="wifi-scan-status" role="status"></p>
        <section id="wifi-networks" aria-labelledby="wifi-networks-title" hidden>
          <h2 id="wifi-networks-title">Redes disponibles</h2>
          <ul class="wifi-results" id="wifi-results"></ul>
          <div class="row">
            <button type="button" id="wifi-prev">Anterior</button>
            <small id="wifi-page" role="status"></small>
            <button type="button" id="wifi-next">Siguiente</button>
          </div>
        </section>
      </div>
      <form class="wifi-form" id="wifi-form" method="post" action="/api/wifi" autocomplete="off" hidden>
        <input type="hidden" name="mode" id="wifi-mode-value" value="station">
        <label>Nombre de la red (SSID)
          <input name="ssid" id="wifi-ssid" maxlength="32" required autocapitalize="none" spellcheck="false">
        </label>
        <input type="hidden" name="ssidHex" id="wifi-ssid-hex">
        <label id="wifi-password-field">Contraseña
          <input type="password" name="password" id="wifi-password" maxlength="64" autocomplete="new-password" aria-describedby="wifi-help">
        </label>
        <small id="wifi-help">Usa una red de 2,4 GHz. Deja la contraseña vacía si la red es abierta.</small>
        <small>Para cambiar de red, pon el driver en OFF. El panel se desconectará durante el intento.</small>
        <button type="submit" id="wifi-apply">Conectar a esta red</button>
      </form>
      <p id="wifi-feedback" role="status"></p>
    </div>
  </section>
<!-- Estado de la placa y autoría. -->  <footer><div class="foot">
    <span>IP <strong id="ip">—</strong></span><span>SPI <strong id="spi-link">—</strong></span>
    <span>Tiempo activo <strong id="uptime">—</strong></span>
    <div class="foot-credit">
      <span>Creado por Juan Miguel González García · 2026</span>
      <button id="show-copyright" type="button" aria-label="Abrir aviso de copyright" aria-haspopup="dialog">©</button>
    </div>
  </div></footer>
  <dialog id="copyright-notice" aria-labelledby="copyright-title">
    <h2 id="copyright-title">Copyright &amp; License</h2>
    <div class="copyright-text" tabindex="0" role="region" aria-labelledby="copyright-title">
      <p><strong>Copyright © 2026 Juan Miguel González García</strong></p>
      <p><strong>Todos los derechos reservados, excepto los expresamente concedidos en este documento.</strong></p>
      <h3>1. Propiedad intelectual</h3>
      <p>Este proyecto, incluyendo su código fuente, documentación y demás materiales originales, está protegido por la legislación vigente sobre propiedad intelectual.</p>
      <p>El titular de los derechos de autor es:</p>
      <p><strong>Juan Miguel González García</strong></p>
      <h3>2. Uso personal y no comercial</h3>
      <p>Se concede gratuitamente permiso para:</p>
      <ul>
        <li>Descargar y ejecutar el software con fines personales, educativos, experimentales y no comerciales.</li>
        <li>Estudiar su funcionamiento y aprender de su código fuente.</li>
        <li>Modificarlo y adaptarlo para uso personal y no comercial.</li>
        <li>Utilizarlo en proyectos particulares, domésticos o de experimentación sin finalidad comercial.</li>
      </ul>
      <p>Estos usos no requieren autorización previa del autor.</p>
      <h3>3. Prohibición de explotación comercial</h3>
      <p><strong>Queda expresamente prohibida cualquier explotación comercial del software, total o parcial, sin autorización previa y por escrito del titular de los derechos.</strong></p>
      <p>Esta prohibición incluye:</p>
      <ul>
        <li>Vender el software original o versiones modificadas.</li>
        <li>Incorporar el código a productos, dispositivos o sistemas destinados a la comercialización.</li>
        <li>Utilizar el software como parte de servicios comerciales remunerados.</li>
        <li>Distribuirlo como parte de soluciones comerciales.</li>
        <li>Explotar económicamente versiones derivadas que incorporen código protegido del proyecto.</li>
        <li>Utilizarlo en actividades empresariales o profesionales con finalidad comercial.</li>
      </ul>
      <p>Toda autorización comercial deberá obtenerse expresamente del titular.</p>
      <h3>4. Modificaciones y distribución</h3>
      <p>Se permite modificar el código fuente exclusivamente para uso personal, educativo o experimental no comercial.</p>
      <p>No se concede autorización general para redistribuir públicamente copias originales o modificadas, salvo los usos permitidos por la legislación aplicable y las condiciones de servicio de GitHub.</p>
      <p>Cualquier otra redistribución requerirá consentimiento previo y por escrito del autor.</p>
      <h3>5. Reconocimiento de autoría</h3>
      <p>Toda copia o modificación autorizada deberá conservar los avisos de copyright y las referencias al autor original.</p>
      <p>No se autoriza presentar el software original como obra propia de terceros.</p>
      <h3>6. Exclusión de garantías</h3>
      <p>El software se proporciona <strong>"TAL CUAL" (AS IS)</strong>, sin garantías expresas o implícitas, en la medida permitida por la legislación aplicable.</p>
      <p>El autor no asume responsabilidad por daños derivados de su utilización, dentro de los límites establecidos legalmente.</p>
      <h3>7. Licencias comerciales</h3>
      <p>Para solicitar una licencia de utilización o explotación comercial, contactar con:</p>
      <p><strong>Autor:</strong> Juan Miguel González García</p>
      <p><strong>Correo electrónico:</strong> juammigg@gmail.com</p>
      <p>Cualquier autorización comercial deberá formalizarse por escrito.</p>
      <h3>8. Legislación aplicable</h3>
      <p>Las presentes condiciones se interpretarán de acuerdo con la legislación española y demás normativa de propiedad intelectual que resulte aplicable.</p>
      <hr>
      <p><strong>Copyright © 2026 Juan Miguel González García</strong></p>
      <p><strong>Uso personal, educativo y experimental no comercial permitido.</strong></p>
      <p><strong>Explotación comercial prohibida sin autorización escrita.</strong></p>
      <p><em>This software is made available for personal, educational and non-commercial experimental use only. Commercial exploitation requires prior written permission from the copyright holder.</em></p>
      <p><strong>Author:</strong> Juan Miguel González García<br>
        <strong>Contact:</strong> juammigg@gmail.com</p>
    </div>
    <form method="dialog"><button type="submit" autofocus>Cerrar</button></form>
  </dialog>
  <script>
    // Referencias a controles y estado local de las órdenes del navegador.
    const byId = (id) => document.getElementById(id);
    byId('show-copyright').addEventListener('click', () => byId('copyright-notice').showModal());
    let latestStatus = null;
    let pendingCommand = null;
    let stopPending = false;
    let driverPending = false;
    let settingPending = false;
    let motionId = null;
    let positionCommand = false;
    let programRun = null;
    let programLoading = false;
    let gcodeFileName = 'programa.gcode';
    let pendingSpeed = null;
    let speedTimer = null;
    let speedRequestPending = false;
    const speedInput = byId('jog-speed');
    const speedSlider = byId('speed-slider');
    const rampInput = byId('jog-ramp-time');
    const rampSlider = byId('ramp-slider');
    const decelerationInput = byId('jog-deceleration-time');
    const decelerationSlider = byId('deceleration-slider');
    const runCurrentInput = byId('run-current');
    const holdCurrentInput = byId('hold-current');
    let displayedRunCurrentMa = null;
    const driverToggles = document.querySelectorAll('[data-driver-toggle]');
    const currentSummaries = document.querySelectorAll('[data-current-summary]');
    const silentModeToggle = byId('silent-mode-toggle');
    const microstepButtons = document.querySelectorAll('#microstep-controls button');
    const pinSelectors = document.querySelectorAll('#pin-controls select');
    let displayedPinInversions = null;
    const motorStepsPerRevolution = 200;
    let microstepsPerStep = 16;
    const rpmFormat = new Intl.NumberFormat('es-ES', {maximumFractionDigits: 2});
    const pulseFormat = new Intl.NumberFormat('es-ES');
    const positionFormat = new Intl.NumberFormat('es-ES', {maximumFractionDigits: 8});
    const millimeterFormat = new Intl.NumberFormat('es-ES', {maximumFractionDigits: 6});
    const stepsPerMm = byId('steps-per-mm');
    const mmPerRevolution = byId('mm-per-revolution');
    const positionSteps = byId('position-input');
    const positionMm = byId('position-input-mm');
    let positionSource = positionSteps;
    const currentFormat = new Intl.NumberFormat('es-ES', {minimumFractionDigits: 2, maximumFractionDigits: 2});

    // Convierte la velocidad en micropasos por segundo a revoluciones por minuto.
    function formatRpm(pulseSpeed) {
      const rpm = pulseSpeed * 60 / (motorStepsPerRevolution * microstepsPerStep);
      return rpmFormat.format(rpm) + ' RPM';
    }

    // Actualiza las RPM indicadas y los cálculos de ambas rampas.
    function updateTargetRpm() {
      byId('target-rpm').value = speedInput.validity.valid
        ? formatRpm(speedInput.valueAsNumber) : '—';
      updateRampRate(rampInput, 'ramp-acceleration');
      updateRampRate(decelerationInput, 'ramp-deceleration');
    }

    // Calcula la aceleración o deceleración media para la velocidad y el tiempo actuales.
    function updateRampRate(input, outputId) {
      const output = byId(outputId);
      if (!speedInput.validity.valid || !input.validity.valid) {
        output.value = '—';
        return;
      }
      const pulses = speedInput.valueAsNumber / input.valueAsNumber;
      const rpm = pulses * 60 / (motorStepsPerRevolution * microstepsPerStep);
      output.value = rpmFormat.format(rpm) + ' RPM/s · ' +
        rpmFormat.format(pulses) + ' µpasos/s²';
    }

    // Sincroniza el campo en segundos con un deslizador de escala logarítmica.
    function bindRampSlider(input, slider, outputId) {
      const minimum = Number(input.min);
      const span = Number(input.max) / minimum;
      const sliderMax = Number(slider.max);
      function updateReadout() {
        if (input.validity.valid) {
          slider.setAttribute('aria-valuetext', rpmFormat.format(input.valueAsNumber) + ' segundos');
        }
        updateRampRate(input, outputId);
      }
      function syncSlider() {
        if (input.validity.valid) {
          slider.value = Math.round(sliderMax * Math.log(input.valueAsNumber / minimum) / Math.log(span));
        }
        updateReadout();
      }
      slider.addEventListener('input', () => {
        input.value = (minimum * Math.pow(span, slider.valueAsNumber / sliderMax)).toFixed(2);
        updateReadout();
      });
      input.addEventListener('input', syncSlider);
      syncSlider();
    }

    // Habilita o bloquea los controles según el estado de conexión, driver y movimiento.
    function updateControls() {
      const busy = Boolean(latestStatus?.moving || latestStatus?.programState === 1 ||
        pendingCommand || motionId !== null || programRun);
      const canJog = latestStatus?.driverResponding && latestStatus.stepDirMode &&
                     latestStatus.driverEnabled && !latestStatus.silentInitializing && !latestStatus.moving &&
                     !busy && !stopPending && !driverPending && !settingPending;
      const canChangeDirection = canChangeSpeed() && !settingPending;
      byId('move-home').disabled = byId('move-position').disabled =
        positionSteps.disabled = positionMm.disabled = !canJog;
      byId('set-home').disabled = !latestStatus || busy || stopPending || driverPending || settingPending;
      const hasGcode = Boolean(byId('gcode-editor').value.trim());
      byId('gcode-save').disabled = programLoading || !hasGcode;
      byId('gcode-run').disabled = !canJog || programLoading || !hasGcode ||
        latestStatus.overtemperature || latestStatus.driverError || latestStatus.shortCircuit;
      byId('gcode-stop').disabled = !programRun || stopPending;
      for (const id of ['gcode-editor', 'gcode-file', 'steps-per-mm', 'mm-per-revolution']) {
        byId(id).disabled = Boolean(programRun) || programLoading;
      }
      byId('jog-negative').disabled = !canJog && !canChangeDirection;
      byId('jog-positive').disabled = !canJog && !canChangeDirection;
      byId('stop-motion').disabled = stopPending || latestStatus?.stopping ||
        (!latestStatus && !pendingCommand);
      byId('cancel-position').disabled = byId('stop-motion').disabled || !busy;
      const driverDisabled = driverPending || settingPending || !latestStatus ||
        (!latestStatus.driverEnabled && (!latestStatus.driverResponding || !latestStatus.stepDirMode));
      for (const toggle of driverToggles) {
        toggle.disabled = driverDisabled;
        if (!driverPending) toggle.checked = latestStatus?.driverEnabled === true;
        toggle.nextElementSibling.textContent = toggle.checked ? 'ON' : 'OFF';
      }
      speedInput.disabled = speedSlider.disabled = busy && !canChangeSpeed();
      rampInput.disabled = rampSlider.disabled = busy;
      decelerationInput.disabled = decelerationSlider.disabled = busy;
      byId('microstep-controls').disabled = !latestStatus || latestStatus.driverEnabled ||
        busy || driverPending || settingPending;
      byId('current-controls').disabled = byId('microstep-controls').disabled;
      byId('pin-controls').disabled = byId('microstep-controls').disabled;
      silentModeToggle.disabled = byId('microstep-controls').disabled;
      if (latestStatus && !settingPending) {
        if (displayedPinInversions !== latestStatus.pinInversions) {
          displayedPinInversions = latestStatus.pinInversions;
          for (const select of pinSelectors) {
            select.value = displayedPinInversions & Number(select.dataset.invertMask) ? '1' : '0';
          }
        }
        silentModeToggle.checked = latestStatus.silentMode === true;
        byId('silent-mode-switch').textContent = silentModeToggle.checked ? 'ON' : 'OFF';
        byId('chopper-mode').textContent = latestStatus.silentInitializing ? 'StealthChop · Ajustando' :
          (latestStatus.silentMode ? 'StealthChop' : 'SpreadCycle') +
          (latestStatus.driverEnabled ? '' : ' · Ajusta con el driver en OFF.');
        if (displayedRunCurrentMa !== latestStatus.runCurrentMa) {
          displayedRunCurrentMa = latestStatus.runCurrentMa;
          runCurrentInput.value = (displayedRunCurrentMa / 1000).toFixed(2);
        }
        holdCurrentInput.value = (latestStatus.holdCurrentMa / 1000).toFixed(2);
        const currentSummary =
          currentFormat.format(latestStatus.runCurrentMa / 1000) + ' A RMS en marcha · ' +
          currentFormat.format(latestStatus.holdCurrentMa / 1000) + ' A RMS en reposo';
        for (const summary of currentSummaries) summary.textContent = currentSummary;
        microstepsPerStep = latestStatus.microsteps;
        for (const button of microstepButtons) {
          button.setAttribute('aria-pressed', Number(button.value) === microstepsPerStep);
        }
        byId('rpm-microsteps').textContent = microstepsPerStep;
        if (Number(speedInput.max) !== latestStatus.maxPulseSpeed) {
          speedInput.max = speedSlider.max = latestStatus.maxPulseSpeed;
          if (speedInput.valueAsNumber > latestStatus.maxPulseSpeed) {
            speedInput.value = latestStatus.maxPulseSpeed;
          }
          if (speedInput.validity.valid) speedSlider.value = speedInput.value;
        }
        byId('speed-limit').textContent =
          pulseFormat.format(latestStatus.maxPulseSpeed) + ' µpasos/s · máx. ' +
          latestStatus.maxRpm + ' RPM';
        updateTargetRpm();
      }
    }

    // Envía una orden POST e incluye la cabecera que exige el servidor.
    async function sendCommand(path, signal, body) {
      const response = await fetch(path, {
        method: 'POST',
        headers: {'X-MotorPAP-Command': '1'},
        body,
        signal
      });
      if (!response.ok) {
        throw Object.assign(new Error(await response.text()), {status: response.status});
      }
      return response.text();
    }

    // Comprueba si un Jog activo puede aceptar un ajuste de velocidad.
    function canChangeSpeed() {
      return motionId !== null && !positionCommand && latestStatus?.driverEnabled &&
        !latestStatus.positioning && !latestStatus.stopping &&
        !stopPending && !driverPending;
    }

    // Descarta los ajustes de velocidad pendientes al cerrar una sesión de movimiento.
    function cancelSpeedChange() {
      clearTimeout(speedTimer);
      speedTimer = null;
      pendingSpeed = null;
    }

    // Agrupa los cambios rápidos del deslizador antes de enviarlos.
    function scheduleSpeedChange() {
      if (speedTimer === null && !speedRequestPending && pendingSpeed !== null) {
        speedTimer = setTimeout(applySpeedChange, 120);
      }
    }

    // Envía el cambio definitivo de velocidad cuando termina la edición.
    function queueSpeedChange() {
      if (canChangeSpeed() && speedInput.validity.valid) {
        pendingSpeed = speedInput.valueAsNumber;
        scheduleSpeedChange();
      }
    }

    // Envía el último ajuste pendiente al Jog y respeta el identificador de la sesión.
    async function applySpeedChange() {
      speedTimer = null;
      if (!canChangeSpeed() || pendingSpeed === null) {
        cancelSpeedChange();
        return;
      }
      const id = motionId;
      const speed = pendingSpeed;
      pendingSpeed = null;
      speedRequestPending = true;
      const request = new AbortController();
      const timeout = setTimeout(() => request.abort(), 2000);
      try {
        await sendCommand('/api/jog/speed?id=' + id + '&speed=' + speed, request.signal);
      } catch (error) {
        if (canChangeSpeed() && motionId === id) byId('motion-feedback').textContent = error.message;
      } finally {
        clearTimeout(timeout);
        speedRequestPending = false;
        scheduleSpeedChange();
      }
    }

    // Inicia un Jog o solicita una inversión en el movimiento manual activo.
    async function jog(direction) {
      if (canChangeSpeed()) {
        settingPending = true;
        byId('motion-feedback').textContent = '';
        updateControls();
        try {
          await sendCommand('/api/jog/' + direction + '?id=' + motionId);
        } catch (error) {
          byId('motion-feedback').textContent = error.message;
        } finally {
          settingPending = false;
          updateControls();
        }
        return;
      }
      if (!speedInput.reportValidity() || !rampInput.reportValidity() ||
          !decelerationInput.reportValidity()) return;
      byId('motion-feedback').textContent = '';
      pendingCommand = sendCommand('/api/jog/' + direction + '?speed=' + speedInput.valueAsNumber +
        '&rampMs=' + Math.round(rampInput.valueAsNumber * 1000) +
        '&decelerationMs=' + Math.round(decelerationInput.valueAsNumber * 1000));
      updateControls();
      try {
        const id = await pendingCommand;
        motionId = id;
        positionCommand = false;
        if (latestStatus) {
          latestStatus.moving = true;
        }
      } catch (error) {
        byId('motion-feedback').textContent = error.message;
      } finally {
        pendingCommand = null;
        updateControls();
      }
    }

    async function moveToPosition(steps = null) {
      if (byId('move-position').disabled ||
          !speedInput.reportValidity() || !rampInput.reportValidity() ||
          !decelerationInput.reportValidity()) return;
      const feedback = byId('motion-feedback');
      feedback.textContent = '';
      const target = steps === null ? 'home=1' : 'steps=' + steps;
      pendingCommand = sendCommand('/api/position?' + target + '&speed=' + speedInput.valueAsNumber +
        '&rampMs=' + Math.round(rampInput.valueAsNumber * 1000) +
        '&decelerationMs=' + Math.round(decelerationInput.valueAsNumber * 1000));
      updateControls();
      try {
        motionId = await pendingCommand;
        positionCommand = true;
        if (latestStatus) {
          latestStatus.positioning = true;
          latestStatus.moving = true;
        }
      } catch (error) {
        feedback.textContent = error.message;
      } finally {
        pendingCommand = null;
        updateControls();
      }
    }

    async function setHomePosition() {
      const feedback = byId('motion-feedback');
      settingPending = true;
      feedback.textContent = '';
      updateControls();
      try {
        await sendCommand('/api/position/home');
        feedback.textContent = 'Posición de inicio establecida.';
      } catch (error) {
        feedback.textContent = error.message;
      } finally {
        settingPending = false;
        updateControls();
      }
    }

    // Envía STOP y actualiza los controles mientras se confirma la orden.
    async function stopMotion() {
      stopPending = true;
      const stoppedProgram = Boolean(programRun);
      finishProgram('Programa cancelado. Enviando orden de parada…');
      cancelSpeedChange();
      updateControls();
      try {
        // Enviar la parada después de una orden de movimiento que siga en tránsito.
        await pendingCommand?.catch(() => {});
        await sendCommand('/api/stop');
        if (latestStatus?.moving) {
          latestStatus.stopping = true;
          latestStatus.decelerating = true;
        }
        byId('motion-feedback').textContent = 'Orden de parada enviada.';
        if (stoppedProgram) byId('gcode-status').textContent = 'Programa cancelado. Orden de parada enviada.';
      } catch (error) {
        byId('motion-feedback').textContent = error.message;
        if (stoppedProgram) {
          motionId = null;
          byId('gcode-status').textContent = 'Programa cancelado. El ESP no confirmó la orden de parada.';
        }
      } finally {
        stopPending = false;
        updateControls();
      }
    }

    // Envía el estado del interruptor de habilitación al servidor.
    async function changeDriver(event) {
      const enabled = event.currentTarget.checked;
      driverPending = true;
      for (const toggle of driverToggles) toggle.checked = enabled;
      byId('motor-feedback').textContent = '';
      byId('motion-feedback').textContent = '';
      if (!enabled) {
        finishProgram('Programa cancelado al deshabilitar el driver.');
        motionId = null;
        cancelSpeedChange();
      }
      updateControls();
      try {
        await pendingCommand?.catch(() => {});
        if (!enabled) motionId = null;
        await sendCommand('/api/driver?enabled=' + (enabled ? '1' : '0'));
        if (latestStatus) {
          latestStatus.driverEnabled = enabled;
          latestStatus.silentInitializing = enabled && latestStatus.silentMode;
          if (!enabled) latestStatus.moving = false;
        }
      } catch (error) {
        byId('motor-feedback').textContent = error.message;
        byId('motion-feedback').textContent = error.message;
      } finally {
        driverPending = false;
        updateControls();
      }
    }

    // Conecta el interruptor de habilitación con la orden del driver.
    for (const toggle of driverToggles) toggle.addEventListener('change', changeDriver);

    // Envía un ajuste y actualiza el panel con el resultado.
    async function changeSetting(name, value) {
      settingPending = true;
      byId('motor-feedback').textContent = '';
      updateControls();
      try {
        const parameter = name === 'current' ? 'ma' : 'value';
        const command = '/api/' + name + '?' + parameter + '=' + value;
        const response = await sendCommand(command);
        if (latestStatus) Object.assign(latestStatus, JSON.parse(response));
      } catch (error) {
        byId('motor-feedback').textContent = error.message;
      } finally {
        settingPending = false;
        updateControls();
      }
    }

    for (const button of microstepButtons) {
      button.addEventListener('click', () => changeSetting('microsteps', button.value));
    }
    byId('apply-current').addEventListener('click', () => {
      if (runCurrentInput.reportValidity()) {
        changeSetting('current', Math.round(runCurrentInput.valueAsNumber * 1000));
      }
    });
    silentModeToggle.addEventListener('change', () => {
      changeSetting('silent-mode', silentModeToggle.checked ? '1' : '0');
    });
    byId('apply-pins').addEventListener('click', () => {
      let mask = 0;
      for (const select of pinSelectors) {
        if (select.value === '1') mask |= Number(select.dataset.invertMask);
      }
      changeSetting('pins', mask);
    });
    byId('jog-negative').addEventListener('click', () => jog('negative'));
    byId('jog-positive').addEventListener('click', () => jog('positive'));
    byId('stop-motion').addEventListener('click', stopMotion);
    byId('cancel-position').addEventListener('click', stopMotion);
    byId('gcode-stop').addEventListener('click', stopMotion);
    for (const input of [positionSteps, positionMm]) {
      input.addEventListener('input', () => {
        positionSource = input;
        syncPositionInputs();
      });
    }
    byId('position-form').addEventListener('submit', event => {
      event.preventDefault();
      if (positionSource === positionMm) {
        if (!positionMm.reportValidity() || !stepsPerMm.reportValidity()) return;
        syncPositionInputs();
      }
      if (positionSteps.reportValidity()) moveToPosition(positionSteps.valueAsNumber);
    });
    byId('move-home').addEventListener('click', () => moveToPosition());
    byId('set-home').addEventListener('click', setHomePosition);
    speedSlider.addEventListener('input', () => {
      speedInput.value = speedSlider.value;
      updateTargetRpm();
      queueSpeedChange();
    });
    speedInput.addEventListener('input', () => {
      if (speedInput.validity.valid) speedSlider.value = speedInput.value;
      updateTargetRpm();
    });
    speedInput.addEventListener('change', queueSpeedChange);
    bindRampSlider(rampInput, rampSlider, 'ramp-acceleration');
    bindRampSlider(decelerationInput, decelerationSlider, 'ramp-deceleration');
    updateTargetRpm();
    const series = document.querySelectorAll('.series-tabs button');
    const telemetryToggle = byId('telemetry-toggle');
    const telemetryCanvas = byId('telemetry-plot');
    const plotStatus = byId('plot-status');
    const plotConnection = byId('plot-connection');
    const plotReadout = byId('plot-readout');
    const telemetryWindowMs = 30000;
    const telemetryFormat = new Intl.NumberFormat('es-ES', {maximumFractionDigits: 2});
    const samples = [];
    let chartSeries = 'position';
    let sampleInterrupted = false;

    function drawTelemetry() {
      const last = samples[samples.length - 1];
      const speed = chartSeries === 'speed';
      const label = speed ? 'Velocidad' : 'Posición';
      const unit = speed ? 'RPM' : 'pasos';
      plotReadout.textContent = last ? label + ' · ' + telemetryFormat.format(last[chartSeries]) + ' ' + unit
        : label + ' (' + unit + ')';
      plotStatus.hidden = Boolean(last);

      const width = telemetryCanvas.clientWidth;
      const height = telemetryCanvas.clientHeight;
      const ctx = telemetryCanvas.getContext('2d');
      if (!ctx || width < 100 || height < 60) return;

      const ratio = Math.min(window.devicePixelRatio || 1, 2);
      const pixelsWide = Math.round(width * ratio);
      const pixelsHigh = Math.round(height * ratio);
      if (telemetryCanvas.width !== pixelsWide || telemetryCanvas.height !== pixelsHigh) {
        telemetryCanvas.width = pixelsWide;
        telemetryCanvas.height = pixelsHigh;
      }
      ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
      ctx.clearRect(0, 0, width, height);
      if (!last) return;

      const values = samples.map(sample => sample[chartSeries]);
      if (speed) values.push(0);
      const low = Math.min(...values);
      const high = Math.max(...values);
      const padding = Math.max(.25, (high - low) * .08);
      const yMin = low - padding;
      const yMax = high + padding;
      const top = 10;
      const bottom = height - 24;
      const right = width - 14;
      ctx.font = '11px system-ui, sans-serif';
      const ticks = Array.from({length: 5}, (_, i) => telemetryFormat.format(yMax - (yMax - yMin) * i / 4));
      const left = Math.min(width * .46, Math.max(40, ...ticks.map(text => ctx.measureText(text).width + 13)));
      const x = time => right + (time - last.time) / telemetryWindowMs * (right - left);
      const y = value => top + (yMax - value) / (yMax - yMin) * (bottom - top);

      ctx.strokeStyle = '#dbe5f0';
      ctx.fillStyle = '#43516a';
      ctx.lineWidth = 1;
      ctx.textAlign = 'right';
      ctx.textBaseline = 'middle';
      ctx.beginPath();
      ticks.forEach((text, i) => {
        const row = top + (bottom - top) * i / 4;
        ctx.moveTo(left, row);
        ctx.lineTo(right, row);
        ctx.fillText(text, left - 6, row, left - 8);
      });
      ctx.textAlign = 'center';
      for (const seconds of [-30, -20, -10, 0]) {
        const column = x(last.time + seconds * 1000);
        ctx.moveTo(column, top);
        ctx.lineTo(column, bottom);
        ctx.fillText(String(seconds), column, bottom + 13);
      }
      ctx.stroke();

      ctx.strokeStyle = ctx.fillStyle = '#0867d8';
      ctx.lineWidth = 2;
      ctx.beginPath();
      samples.forEach((sample, index) => {
        if (index === 0 || sample.gap) ctx.moveTo(x(sample.time), y(sample[chartSeries]));
        else ctx.lineTo(x(sample.time), y(sample[chartSeries]));
      });
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(x(last.time), y(last[chartSeries]), 3, 0, Math.PI * 2);
      ctx.fill();
    }

    function recordTelemetry(status) {
      if (!telemetryToggle.checked) return;
      const time = status.sampleMs;
      const position = status.positionUnits / status.positionUnitsPerStep;
      const speed = status.signedPulseSpeed * 60 / (motorStepsPerRevolution * status.microsteps);
      if (![time, position, speed].every(Number.isFinite)) return;
      const previous = samples[samples.length - 1];
      const positionPolarity = status.pinInversions & 5; // DIR y STEP cambian la referencia.
      if (previous && (time < previous.time || status.microsteps !== previous.microsteps ||
          positionPolarity !== previous.positionPolarity)) samples.length = 0;
      if (previous?.time !== time || samples.length === 0) {
        const gap = sampleInterrupted || Boolean(previous && time - previous.time > 2000);
        samples.push({time, position, speed, microsteps: status.microsteps, positionPolarity, gap});
      }
      sampleInterrupted = false;
      while (samples[0].time < time - telemetryWindowMs) samples.shift();
      plotConnection.hidden = true;
      drawTelemetry();
    }

    for (const button of series) {
      button.addEventListener('click', () => {
        for (const item of series) item.setAttribute('aria-pressed', item === button);
        chartSeries = button.dataset.series;
        drawTelemetry();
      });
    }
    telemetryToggle.addEventListener('change', () => {
      const enabled = telemetryToggle.checked;
      byId('telemetry-switch').textContent = enabled ? 'ON' : 'OFF';
      for (const button of series) button.disabled = !enabled;
      samples.length = 0;
      sampleInterrupted = false;
      plotStatus.textContent = enabled ? 'Esperando datos…' : 'Telemetría desactivada';
      plotConnection.hidden = true;
      drawTelemetry();
    });
    if ('ResizeObserver' in window) new ResizeObserver(drawTelemetry).observe(telemetryCanvas);
    else window.addEventListener('resize', drawTelemetry);

    // Actualiza las tarjetas de conexión del panel.
    function setConnection(online, accessPoint = false) {
      const wasOnline = byId('wifi').classList.contains('online');
      byId('wifi').textContent = online ? (accessPoint ? 'Punto de acceso' : 'Conectado') : 'Sin respuesta';
      byId('controller').textContent = online ? 'En línea' : 'Sin respuesta';
      byId('wifi').classList.toggle('online', online);
      byId('controller').classList.toggle('online', online);
      if (!online) showWifiConnection('Sin comunicación con el ESP');
      if (online && !wasOnline && !wifiPanel.hidden) refreshWifiNetwork();
    }

    // Aplica el estado recibido del firmware a los controles del driver.
    function setDriver(status) {
      const currentPosition = status ? status.positionUnits / status.positionUnitsPerStep : NaN;
      if (latestStatus?.positioning && status?.moving === false) {
        byId('motion-feedback').textContent = status.hasPositionTarget && currentPosition === status.positionTarget
          ? 'Posición objetivo alcanzada.'
          : 'Movimiento detenido antes de alcanzar el objetivo.';
      } else if (latestStatus?.stopping && status?.moving === false) {
        byId('motion-feedback').textContent = 'Movimiento detenido.';
      }
      updatePositionReadings(status);
      const homePosition = status ? status.homeUnits / status.positionUnitsPerStep : NaN;
      byId('home-position').textContent = Number.isFinite(homePosition) ? positionFormat.format(homePosition) : '—';
      latestStatus = status;
      const responding = status?.driverResponding === true;
      const label = status ? (responding ? 'Detectado' : 'Sin identificar') : 'Sin datos';
      byId('driver-link').textContent = label;
      byId('driver-link').classList.toggle('online', responding);
      byId('spi-link').textContent = responding ? 'OK' : label;
      byId('driver-id').textContent = responding ? 'IOIN · 0x30' : '—';
      byId('driver-enable').textContent = responding
        ? (status.moving ? 'Orden de movimiento activa' :
           (status.driverEnabled ? 'Baja · salida habilitada' :
            (status.driverDisabled ? 'Alta · salida deshabilitada' : 'Verificando EN')))
        : '—';
      updateDiagnostics(status);
      updateMotorCard(status);
      updateControls();
    }

    // Traduce el estado de movimiento a etiquetas visibles para el usuario.
    function updateMotorCard(status) {
      let label = 'Sin datos';
      let level = '';
      if (status) {
        if (!status.driverResponding) {
          label = 'Sin conexión SPI';
        } else if (status.overtemperature || status.shortCircuit || status.driverError) {
          label = 'Fallo del driver';
          level = 'fault';
        } else if (!status.driverEnabled || status.driverDisabled) {
          label = 'OFF';
        } else if (status.silentInitializing) {
          label = 'Ajustando';
          level = 'pending';
        } else if (status.reversing) {
          label = 'Invirtiendo';
          level = 'pending';
        } else if (status.decelerating) {
          label = 'Decelerando';
          level = 'pending';
        } else {
          label = status.moving ? 'En movimiento' : 'En sujeción';
          level = 'online';
        }
      }
      const badge = byId('motor-state');
      if (badge.textContent !== label) badge.textContent = label;
      badge.className = level ? 'pill ' + level : 'pill';
    }

    function showDiagnostic(id, value, level = '') {
      const reading = byId(id);
      reading.textContent = value;
      reading.parentElement.dataset.level = level;
    }

    // Muestra los diagnósticos disponibles y marca las alertas del driver.
    function updateDiagnostics(status) {
      if (!status?.diagnosticsAvailable) {
        for (const id of ['diag-temperature', 'diag-stallguard', 'diag-error']) {
          showDiagnostic(id, 'Sin lectura');
        }
        return;
      }
      if (status.overtemperature) showDiagnostic('diag-temperature', 'Sobretemperatura', 'fault');
      else if (status.temperatureWarning) showDiagnostic('diag-temperature', 'Preaviso térmico', 'warn');
      else showDiagnostic('diag-temperature', 'Normal', 'ok');

      const fault = status.shortCircuit ? 'Cortocircuito' : (status.driverError ? 'Fallo del driver' : '');
      showDiagnostic('diag-error', fault || 'Sin error', fault ? 'fault' : 'ok');

      if (!status.moving) showDiagnostic('diag-stallguard', 'En reposo');
      else if (status.silentActive) showDiagnostic('diag-stallguard', 'No disponible en StealthChop');
      else if (status.stallGuardResult >= 0) showDiagnostic('diag-stallguard', status.stallGuardResult + ' / 1023');
      else showDiagnostic('diag-stallguard', 'Esperando lectura en marcha');
    }

    // Traduce todo el programa a destinos del eje X antes de enviar la primera orden.
    function parseGcode(source, options) {
      const moves = [];
      let absolute = true;
      let mmPerUnit = 1;
      let motion = null;
      let feedMmPerMinute = null;
      let position = options.start;
      let ended = false;
      if (source.length > 65536) throw new Error('El programa supera los 64 KiB.');

      source.split(/\r\n|\r|\n/).forEach((raw, index) => {
        const line = raw.replace(/\([^()]*\)/g, '').split(';', 1)[0].trim().toUpperCase();
        if (!line || line === '%') return;
        const lineNumber = index + 1;
        const reject = message => { throw new Error('Línea ' + lineNumber + ': ' + message); };
        if (ended) reject('hay instrucciones después de M2/M30.');

        const pattern = /([A-Z])\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+))/g;
        const words = [...line.matchAll(pattern)];
        if (line.replace(pattern, '').trim()) reject('sintaxis no válida.');
        const seen = new Set();
        let x = null;
        let feed = null;
        for (const [, letter, text] of words) {
          const value = Number(text);
          if (!Number.isFinite(value)) reject('valor numérico fuera de rango.');
          let group = letter;
          if (letter === 'G') {
            if (value === 0 || value === 1) {
              group = 'motion';
              motion = value;
            } else if (value === 20 || value === 21) {
              group = 'units';
              mmPerUnit = value === 20 ? 25.4 : 1;
            } else if (value === 90 || value === 91) {
              group = 'distance';
              absolute = value === 90;
            } else reject('G' + text + ' no admitido.');
          } else if (letter === 'X') {
            x = value;
          } else if (letter === 'F' && value > 0) {
            feed = value;
          } else if (letter === 'M' && (value === 2 || value === 30)) {
            ended = true;
          } else if (letter !== 'N' || !Number.isSafeInteger(value) || value < 0) {
            reject(letter + text + ' no admitido. Solo se controla el eje X.');
          }
          if (seen.has(group)) reject('instrucciones repetidas o incompatibles.');
          seen.add(group);
        }
        if (ended && [...seen].some(group => group !== 'M' && group !== 'N')) {
          reject('M2/M30 debe ir en una línea independiente.');
        }
        if (feed !== null) feedMmPerMinute = feed * mmPerUnit;
        if (x === null) return;
        if (motion === null) reject('indica G0 o G1 antes de mover el eje.');
        const offset = x * mmPerUnit * options.scale;
        position = absolute ? offset : position + offset;
        const steps = Math.round(position);
        if (!Number.isFinite(steps) || Math.abs(steps) > options.maxPosition) {
          reject('el destino está fuera del rango de posición.');
        }
        let speed = options.speed;
        if (motion === 1) {
          if (feedMmPerMinute === null) reject('indica un avance F para G1.');
          const pulsesPerMm = options.scale * options.microsteps;
          const requested = Math.round(feedMmPerMinute * pulsesPerMm / 60);
          if (!Number.isFinite(requested) || requested < options.minSpeed || requested > options.maxSpeed) {
            const minimum = positionFormat.format(options.minSpeed * 60 / pulsesPerMm);
            const maximum = positionFormat.format(options.maxSpeed * 60 / pulsesPerMm);
            reject('el avance F de ' + positionFormat.format(feedMmPerMinute) +
              ' mm/min está fuera del rango disponible (' + minimum + ' a ' + maximum + ' mm/min).');
          }
          speed = requested;
        }
        if (moves.length === 1000) reject('se admiten hasta 1.000 movimientos.');
        moves.push({steps, speed, line: lineNumber});
      });
      if (!moves.length) throw new Error('El programa no contiene movimientos del eje X.');
      return moves;
    }

    function finishProgram(message) {
      if (!programRun) return;
      programRun = null;
      showGcodeLine(null);
      byId('gcode-status').textContent = message;
      updateControls();
    }

    async function uploadProgram(run) {
      let upload = '';
      const blockSize = 32;
      for (let offset = 0; offset < run.moves.length && programRun === run; offset += blockSize) {
        const block = run.moves.slice(offset, offset + blockSize);
        const last = offset + block.length === run.moves.length;
        const query = new URLSearchParams({offset, upload, count: run.moves.length, after: run.expectedRevision});
        if (last) {
          query.set('start', '1');
          query.set('position', run.position);
          query.set('rampMs', run.rampMs);
          query.set('decelerationMs', run.decelerationMs);
        }
        const body = block.map(move => [move.steps, move.speed, move.line].join(',') + '\n').join('');
        const request = new AbortController();
        const timeout = setTimeout(() => request.abort(), 2500);
        try {
          upload = await sendCommand('/api/program?' + query, request.signal, body);
        } finally {
          clearTimeout(timeout);
        }
      }
      return upload;
    }

    async function startProgram(run) {
      byId('gcode-status').textContent = 'Enviando programa al ESP…';
      pendingCommand = uploadProgram(run);
      updateControls();
      try {
        const id = await pendingCommand;
        if (programRun === run) {
          run.id = motionId = id;
          positionCommand = true;
          byId('gcode-status').textContent = 'Programa iniciado.';
        }
      } catch (error) {
        if (programRun === run) {
          finishProgram(error.name === 'AbortError' ? 'El ESP no confirmó el programa.' : error.message);
        }
      } finally {
        pendingCommand = null;
        updateControls();
      }
    }

    function checkProgramProgress(status, polledId) {
      const run = programRun;
      if (!run || run.id === null || run.id !== polledId) return;
      const line = status.programLine;
      if (String(status.motionId) !== run.id || status.motionRevision !== ((run.expectedRevision + 1) >>> 0) ||
          status.programState !== 1) {
        motionId = null;
        const sameProgram = String(status.motionId) === run.id &&
          status.motionRevision === ((run.expectedRevision + 1) >>> 0);
        finishProgram(sameProgram && status.programState === 2 ? 'Programa completado.' :
          'Programa interrumpido' + (sameProgram && line ? ' en la línea ' + line : '') + '.');
        return;
      }
      if (!line || line === activeGcodeLine) return;
      const index = run.moves.findIndex(move => move.line === line);
      if (index < 0) return;
      showGcodeLine(line);
      byId('gcode-status').textContent = 'Movimiento ' + (index + 1) + ' de ' + run.moves.length +
        ' · línea ' + line + ' · destino ' + run.moves[index].steps + ' pasos';
    }

    const gcodeEditor = byId('gcode-editor');
    const gcodeNumbers = byId('gcode-numbers');
    const gcodeHighlight = byId('gcode-active-line');
    let activeGcodeLine = null;

    function syncGcodeScroll() {
      const offset = gcodeEditor.scrollTop;
      gcodeNumbers.style.transform = 'translateY(' + -offset + 'px)';
      if (activeGcodeLine !== null) {
        gcodeHighlight.style.top = 'calc(var(--gcode-padding) + ' +
          'var(--gcode-line-height) * ' + (activeGcodeLine - 1) + ' - ' + offset + 'px)';
      }
    }

    function showGcodeLine(line) {
      activeGcodeLine = line;
      gcodeHighlight.hidden = line === null;
      if (line !== null) {
        const style = getComputedStyle(gcodeEditor);
        const height = parseFloat(style.lineHeight);
        const top = parseFloat(style.paddingTop) + (line - 1) * height;
        if (top < gcodeEditor.scrollTop || top + height > gcodeEditor.scrollTop + gcodeEditor.clientHeight) {
          gcodeEditor.scrollTop = top - (gcodeEditor.clientHeight - height) / 2;
        }
      }
      syncGcodeScroll();
    }

    function updateGcodeNumbers() {
      const count = gcodeEditor.value.split('\n').length;
      gcodeNumbers.textContent = Array.from({length: count}, (_, index) => index + 1).join('\n');
      syncGcodeScroll();
    }

    updateGcodeNumbers();
    gcodeEditor.addEventListener('scroll', syncGcodeScroll);
    gcodeEditor.addEventListener('input', () => {
      updateGcodeNumbers();
      updateControls();
    });
    byId('gcode-save').addEventListener('click', () => {
      downloadText(byId('gcode-editor').value, gcodeFileName);
    });
    byId('gcode-file').addEventListener('change', async event => {
      const picker = event.currentTarget;
      const file = picker.files[0];
      if (!file) return;
      programLoading = true;
      updateControls();
      try {
        if (file.size > 65536) throw new Error('El archivo supera los 64 KiB.');
        byId('gcode-editor').value = await file.text();
        updateGcodeNumbers();
        gcodeFileName = file.name;
        byId('gcode-status').textContent = 'Archivo cargado: ' + file.name;
      } catch (error) {
        byId('gcode-status').textContent = error.message;
      } finally {
        picker.value = '';
        programLoading = false;
        updateControls();
      }
    });

    function updatePositionReadings(status) {
      const scale = stepsPerMm.validity.valid ? stepsPerMm.valueAsNumber : NaN;
      const current = status ? status.positionUnits / status.positionUnitsPerStep : NaN;
      const target = status?.hasPositionTarget ? status.positionTarget : NaN;
      for (const [id, steps] of [['position-target', target], ['position-current', current]]) {
        const mm = steps / scale;
        byId(id).textContent = Number.isFinite(steps) ? positionFormat.format(steps) : '—';
        byId(id + '-mm').textContent = Number.isFinite(mm) ? millimeterFormat.format(mm) : '—';
      }
    }

    function syncPositionInputs() {
      const fromMm = positionSource === positionMm;
      const destination = fromMm ? positionSteps : positionMm;
      const scale = stepsPerMm.validity.valid ? stepsPerMm.valueAsNumber : NaN;
      const value = positionSource.validity.valid ? positionSource.valueAsNumber : NaN;
      const converted = fromMm ? Math.round(value * scale) : Number((value / scale).toPrecision(12));
      destination.value = Number.isFinite(converted) ? converted : '';
    }

    function updateCalibration(source, destination) {
      destination.value = source.validity.valid ? motorStepsPerRevolution / source.valueAsNumber : '';
      syncPositionInputs();
      updatePositionReadings(latestStatus);
    }

    stepsPerMm.addEventListener('input', () => updateCalibration(stepsPerMm, mmPerRevolution));
    mmPerRevolution.addEventListener('input', () => updateCalibration(mmPerRevolution, stepsPerMm));
    updateCalibration(mmPerRevolution, stepsPerMm);

    byId('gcode-run').addEventListener('click', () => {
      if (byId('gcode-run').disabled || !stepsPerMm.reportValidity() || !speedInput.reportValidity() ||
          !rampInput.reportValidity() || !decelerationInput.reportValidity()) return;
      try {
        const moves = parseGcode(byId('gcode-editor').value, {
          start: latestStatus.positionUnits / latestStatus.positionUnitsPerStep,
          scale: stepsPerMm.valueAsNumber, microsteps: latestStatus.microsteps, speed: speedInput.valueAsNumber,
          minSpeed: Number(speedInput.min), maxSpeed: latestStatus.maxPulseSpeed,
          maxPosition: Number(byId('position-input').max)
        });
        programRun = {
          moves, id: null, expectedRevision: latestStatus.motionRevision, position: latestStatus.positionUnits,
          rampMs: Math.round(rampInput.valueAsNumber * 1000),
          decelerationMs: Math.round(decelerationInput.valueAsNumber * 1000)
        };
        startProgram(programRun);
      } catch (error) {
        byId('gcode-status').textContent = error.message;
      }
    });

    // Renueva el permiso de movimiento y lee la línea que ejecuta el ESP.
    async function refreshStatus() {
      const id = motionId;
      const run = programRun?.id === id ? programRun : null;
      const request = new AbortController();
      const timeout = setTimeout(() => request.abort(), id === null ? 2500 : 750);
      try {
        if (id !== null && (!run || performance.now() - (run.lastKeepalive ?? -Infinity) >= 500)) {
          try {
            await sendCommand('/api/jog/keepalive?id=' + id, request.signal);
            if (run) run.lastKeepalive = performance.now();
          } catch (error) {
            if (programRun?.id === id && error.status !== 409) throw error;
            if (motionId === id && programRun?.id !== id && error.status === 409) {
              motionId = null;
              cancelSpeedChange();
            }
          }
        }
        const response = await fetch('/api/status', {signal: request.signal});
        if (!response.ok) throw new Error(response.status);
        const status = await response.json();
        setConnection(true, status.accessPoint);
        setDriver(status);
        checkProgramProgress(status, id);
        recordTelemetry(status);
        byId('signal').textContent = status.accessPoint ? 'Red propia' : status.rssi + ' dBm';
        byId('ip').textContent = status.ip;
        const seconds = status.uptime;
        byId('uptime').textContent =
          [Math.floor(seconds / 3600), Math.floor(seconds / 60) % 60, seconds % 60]
            .map((value) => String(value).padStart(2, '0')).join(':');
      } catch {
        if (programRun) {
          finishProgram('Conexión perdida. El programa no se reanudará.');
          motionId = null;
        }
        if (telemetryToggle.checked && plotConnection.hidden) {
          sampleInterrupted = true;
          plotConnection.textContent = 'Telemetría sin conexión';
          plotConnection.hidden = false;
        }
        setConnection(false);
        setDriver(null);
        byId('signal').textContent = 'Sin datos';
        byId('ip').textContent = '—';
        byId('uptime').textContent = '—';
      } finally {
        clearTimeout(timeout);
        setTimeout(refreshStatus, programRun?.id ? 150 : 500);
      }
    }
    const tabs = document.querySelectorAll('.panel-tabs [role="tab"]');
    const logPanel = byId('log-panel');
    const wifiPanel = byId('wifi-panel');
    const logStatus = byId('log-status');
    const logContent = byId('log-content');
    const downloadLog = byId('download-log');
    const logClock = new Intl.DateTimeFormat('es-ES', {
      dateStyle: 'short', timeStyle: 'medium', hour12: false
    });
    let logTimer = null;
    let logLoading = false;

    // Lee el historial solo mientras la pestaña Registro está visible.
    async function refreshLog() {
      if (logPanel.hidden || logLoading) return;
      logLoading = true;
      const request = new AbortController();
      const timeout = setTimeout(() => request.abort(), 2000);
      try {
        const response = await fetch('/api/log', {signal: request.signal, cache: 'no-store'});
        if (!response.ok) throw new Error(response.status);
        const snapshotAt = Date.now();
        const rows = (await response.text()).trimEnd().split('\n').filter(Boolean);
        const text = rows.map(row => {
          const separator = row.indexOf('\t');
          const time = logClock.format(snapshotAt - Number(row.slice(0, separator)));
          return '[' + time + '] ' + row.slice(separator + 1);
        }).join('\n');
        downloadLog.disabled = rows.length === 0;
        if (logContent.textContent !== text) {
          const atEnd = logContent.scrollHeight - logContent.clientHeight - logContent.scrollTop <= 2;
          logContent.textContent = text;
          if (atEnd) logContent.scrollTop = logContent.scrollHeight;
        }
        logStatus.hidden = true;
      } catch {
        logStatus.textContent = 'Registro sin respuesta. Reintentando…';
        logStatus.hidden = false;
      } finally {
        clearTimeout(timeout);
        logLoading = false;
        if (!logPanel.hidden) logTimer = setTimeout(refreshLog, 2000);
      }
    }

    function downloadText(text, fileName) {
      const url = URL.createObjectURL(new Blob([text], {type: 'text/plain;charset=utf-8'}));
      const link = document.createElement('a');
      link.href = url;
      link.download = fileName;
      link.click();
      setTimeout(() => URL.revokeObjectURL(url), 1000);
    }

    downloadLog.addEventListener('click', () => {
      const text = 'MotorPAP · Registro (hora local del navegador)\n' + logContent.textContent + '\n';
      downloadText(text, 'motorpap-registro.txt');
    });

    function showWifiConnection(state, network = '') {
      byId('wifi-connection').textContent = state;
      byId('wifi-connection').classList.toggle('online', Boolean(network));
      byId('wifi-network').textContent = network;
    }

    async function refreshWifiNetwork() {
      try {
        const response = await fetch('/api/wifi', {cache: 'no-store'});
        if (!response.ok) throw new Error(response.status);
        if (response.status === 204) {
          showWifiConnection('Desconectado');
          return;
        }
        const accessPoint = response.headers.get('X-Wifi-Mode') === 'ap';
        showWifiConnection(accessPoint ? 'Punto de acceso activo' : 'Conectado', await response.text());
        if (!wifiModeInitialized) {
          selectWifiMode(accessPoint ? 'ap' : 'station');
          wifiModeInitialized = true;
        }
        else if (wifiAddressSettings.disabled) {
          loadWifiAddress(wifiModeValue.value);
        }
      } catch {
        showWifiConnection('Sin comunicación con el ESP');
      }
    }

    const wifiForm = byId('wifi-form');
    const wifiResults = byId('wifi-results');
    const wifiScan = byId('wifi-scan');
    const wifiManual = byId('wifi-manual');
    const wifiScanStatus = byId('wifi-scan-status');
    const wifiModeInputs = document.querySelectorAll('input[name="wifi-mode"]');
    const wifiModeValue = byId('wifi-mode-value');
    const wifiAddressForm = byId('wifi-address-form');
    const wifiAddressSettings = byId('wifi-address-settings');
    const wifiAddressMethod = byId('wifi-address-method');
    const wifiAddressManual = byId('wifi-address-manual');
    const wifiAddressFeedback = byId('wifi-address-feedback');
    let wifiAddressRequest = 0;
    let wifiAccessPointUrl = 'http://192.168.4.1/';
    const wifiPageSize = 10;
    let wifiModeInitialized = false;
    let wifiNetworks = [];
    let wifiPage = 0;
    let selectedNetwork = null;

    function showWifiAddressMethod(mode) {
      const manual = wifiAddressMethod.value === 'manual';
      wifiAddressManual.hidden = wifiAddressManual.disabled = !manual;
      byId('wifi-address-help').textContent = mode === 'ap'
        ? manual ? 'El ESP no asignará direcciones por DHCP. Configura el dispositivo cliente en la misma subred.'
                 : 'El punto de acceso usa 192.168.4.1 y asigna direcciones a los clientes por DHCP.'
        : manual ? 'Usa una IP libre de tu red. Al aplicar el cambio, vuelve a abrir el panel en esa dirección.'
                 : 'El router asigna al ESP la dirección IP, la máscara y la puerta de enlace.';
    }

    async function loadWifiAddress(mode) {
      const request = ++wifiAddressRequest;
      wifiAddressSettings.disabled = true;
      wifiAddressFeedback.textContent = 'Leyendo configuración IP…';
      try {
        const response = await fetch('/api/wifi/address?mode=' + mode, {cache: 'no-store'});
        if (!response.ok) throw new Error(response.status);
        const config = await response.json();
        if (request !== wifiAddressRequest) return;
        wifiAddressMethod.value = config.manual ? 'manual' : 'dhcp';
        for (const name of ['ip', 'mask', 'gateway']) {
          wifiAddressForm.elements.namedItem(name).value = config[name] === '0.0.0.0' ? '' : config[name];
        }
        if (mode === 'ap') wifiAccessPointUrl = 'http://' + config.ip + '/';
        showWifiAddressMethod(mode);
        wifiAddressSettings.disabled = false;
        wifiAddressFeedback.textContent = '';
      } catch {
        if (request === wifiAddressRequest) wifiAddressFeedback.textContent = 'No se pudo leer la configuración IP.';
      }
    }

    wifiAddressMethod.addEventListener('change', () => showWifiAddressMethod(wifiModeValue.value));
    wifiAddressForm.addEventListener('submit', async event => {
      event.preventDefault();
      const mode = wifiModeValue.value;
      const request = ++wifiAddressRequest;
      const params = new URLSearchParams(new FormData(wifiAddressForm));
      params.set('mode', mode);
      const manual = params.get('method') === 'manual';
      const ip = manual ? params.get('ip') : '192.168.4.1';
      wifiAddressSettings.disabled = true;
      wifiAddressFeedback.textContent = 'Guardando configuración IP…';
      try {
        const response = await fetch('/api/wifi/address', {
          method: 'POST', headers: {'X-MotorPAP-Command': '1'}, body: params
        });
        if (!response.ok) throw new Error(await response.text());
        if (request !== wifiAddressRequest) return;
        if (mode === 'ap') wifiAccessPointUrl = 'http://' + ip + '/';
        if (response.status === 204) {
          wifiAddressFeedback.textContent = 'Configuración guardada. Se aplicará al activar este modo Wi-Fi.';
        } else if (mode === 'ap' || manual) {
          wifiAddressFeedback.textContent = 'Cambio solicitado. Vuelve a abrir el panel en http://' + ip + '/.' +
            (mode === 'ap' && manual ? ' Asigna primero al cliente una IP de la misma subred.' : '');
        } else {
          wifiAddressFeedback.textContent = 'Cambio solicitado. Consulta la IP asignada por DHCP en el monitor serie.';
        }
      } catch (error) {
        if (request === wifiAddressRequest) wifiAddressFeedback.textContent = error.message;
      } finally {
        if (request === wifiAddressRequest) wifiAddressSettings.disabled = false;
      }
    });

    function showWifiForm(ssid = '', encoded = '', secured = null) {
      const ssidInput = byId('wifi-ssid');
      const password = byId('wifi-password');
      const accessPoint = wifiModeValue.value === 'ap';
      ssidInput.value = ssid;
      ssidInput.readOnly = Boolean(encoded);
      byId('wifi-ssid-hex').value = encoded;
      password.value = '';
      password.required = accessPoint || secured === true;
      password.maxLength = accessPoint ? 63 : 64;
      byId('wifi-password-field').hidden = secured === false;
      byId('wifi-help').hidden = secured === false;
      byId('wifi-help').textContent = accessPoint
        ? 'Usa una clave de 8 a 63 bytes. Después conecta tu dispositivo a esta red.'
        : 'Usa una red de 2,4 GHz. Deja la contraseña vacía si la red es abierta.';
      byId('wifi-apply').textContent = accessPoint ? 'Activar punto de acceso' : 'Conectar a esta red';
      byId('wifi-feedback').textContent = '';
      wifiForm.hidden = false;
      if (!accessPoint) {
        if (!encoded) ssidInput.focus();
        else if (secured) password.focus();
        else byId('wifi-apply').focus();
      }
    }

    function selectWifiMode(mode) {
      for (const input of wifiModeInputs) input.checked = input.value === mode;
      wifiModeValue.value = mode;
      loadWifiAddress(mode);
      const accessPoint = mode === 'ap';
      byId('wifi-station').hidden = accessPoint;
      if (accessPoint) {
        const active = byId('wifi-connection').textContent === 'Punto de acceso activo';
        showWifiForm(active ? byId('wifi-network').textContent : 'MotorPAP');
      } else {
        selectedNetwork = null;
        wifiForm.hidden = true;
        renderWifiNetworks();
      }
    }

    for (const input of wifiModeInputs) {
      input.addEventListener('change', () => {
        wifiModeInitialized = true;
        byId('wifi-feedback').textContent = '';
        selectWifiMode(input.value);
      });
    }

    function renderWifiNetworks() {
      const pageCount = Math.ceil(wifiNetworks.length / wifiPageSize);
      const start = wifiPage * wifiPageSize;
      wifiResults.replaceChildren();
      for (const network of wifiNetworks.slice(start, start + wifiPageSize)) {
        const row = document.createElement('li');
        const label = document.createElement('label');
        const checkbox = document.createElement('input');
        const name = document.createElement('strong');
        const details = document.createElement('small');
        checkbox.type = 'checkbox';
        checkbox.checked = selectedNetwork === network;
        name.textContent = network.ssid;
        details.textContent = network.signal + ' dBm · ' + (network.secured ? 'Protegida' : 'Abierta');
        checkbox.addEventListener('change', () => {
          selectedNetwork = checkbox.checked ? network : null;
          for (const item of wifiResults.querySelectorAll('input')) {
            if (item !== checkbox) item.checked = false;
          }
          if (selectedNetwork) showWifiForm(network.ssid, network.encoded, network.secured);
          else wifiForm.hidden = true;
        });
        label.append(checkbox, name, details);
        row.append(label);
        wifiResults.append(row);
      }
      byId('wifi-networks').hidden = pageCount === 0;
      byId('wifi-page').textContent = 'Página ' + (wifiPage + 1) + ' de ' + pageCount;
      byId('wifi-prev').disabled = wifiPage === 0;
      byId('wifi-next').disabled = wifiPage + 1 >= pageCount;
    }

    byId('wifi-prev').addEventListener('click', () => {
      --wifiPage;
      renderWifiNetworks();
    });
    byId('wifi-next').addEventListener('click', () => {
      ++wifiPage;
      renderWifiNetworks();
    });
    wifiManual.addEventListener('click', () => {
      selectedNetwork = null;
      renderWifiNetworks();
      showWifiForm();
    });

    wifiScan.addEventListener('click', async () => {
      wifiScan.disabled = true;
      wifiManual.disabled = true;
      wifiForm.hidden = true;
      byId('wifi-feedback').textContent = '';
      byId('wifi-networks').hidden = true;
      wifiScanStatus.textContent = 'Buscando redes de 2,4 GHz…';
      const request = new AbortController();
      const timeout = setTimeout(() => request.abort(), 15000);
      try {
        await sendCommand('/api/wifi/scan', request.signal);
        let response;
        do {
          await new Promise(resolve => setTimeout(resolve, 500));
          response = await fetch('/api/wifi/scan', {signal: request.signal, cache: 'no-store'});
        } while (response.status === 202);
        if (!response.ok) throw new Error(await response.text());
        const rows = (await response.text()).trimEnd().split('\n').filter(Boolean);
        const networks = rows.map(row => row.split('\t')).sort((a, b) => Number(b[1]) - Number(a[1]));
        const seen = new Set();
        const decoder = new TextDecoder();
        wifiNetworks = [];
        wifiPage = 0;
        selectedNetwork = null;
        for (const [encoded, signal, secured] of networks) {
          const key = encoded + ':' + secured;
          if (seen.has(key)) continue;
          seen.add(key);
          const ssid = decoder.decode(Uint8Array.from(encoded.match(/../g), pair => parseInt(pair, 16)));
          wifiNetworks.push({ssid, encoded, signal, secured: secured === '1'});
        }
        renderWifiNetworks();
        wifiScanStatus.textContent = seen.size ? '' : 'No se encontraron redes.';
        if (seen.size && !wifiPanel.hidden && wifiModeValue.value === 'station') {
          wifiResults.querySelector('input').focus();
        }
      } catch (error) {
        wifiScanStatus.textContent = error.name === 'AbortError'
          ? 'La búsqueda tardó demasiado. Vuelve a intentarlo.'
          : error.message;
      } finally {
        clearTimeout(timeout);
        wifiScan.disabled = false;
        wifiManual.disabled = false;
      }
    });

    wifiForm.addEventListener('submit', async event => {
      event.preventDefault();
      const button = byId('wifi-apply');
      const feedback = byId('wifi-feedback');
      const accessPoint = wifiModeValue.value === 'ap';
      button.disabled = true;
      feedback.textContent = 'Solicitando el cambio…';
      const request = new AbortController();
      const timeout = setTimeout(() => request.abort(), 4000);
      try {
        const response = await fetch('/api/wifi', {
          method: 'POST',
          headers: {'X-MotorPAP-Command': '1'},
          body: new URLSearchParams(new FormData(wifiForm)),
          signal: request.signal
        });
        if (!response.ok) throw new Error(await response.text());
        byId('wifi-password').value = '';
        feedback.textContent = accessPoint
          ? 'Cambio solicitado. Conéctate a la red del ESP y abre ' + wifiAccessPointUrl + '.'
          : 'Cambio solicitado. Consulta la nueva IP en el monitor serie. Si la red no conecta, el ESP intentará recuperar la anterior.';
      } catch (error) {
        feedback.textContent = error.name === 'AbortError' || error instanceof TypeError
          ? 'No se recibió respuesta. Consulta el monitor serie para comprobar si se aplicó el cambio.'
          : error.message;
      } finally {
        clearTimeout(timeout);
        button.disabled = false;
      }
    });

    function selectTab(selected) {
      tabs.forEach(tab => {
        const active = tab === selected;
        tab.setAttribute('aria-selected', active);
        tab.tabIndex = active ? 0 : -1;
        byId(tab.getAttribute('aria-controls')).hidden = !active;
      });
      clearTimeout(logTimer);
      if (selected.id === 'movement-tab') drawTelemetry();
      if (!logPanel.hidden) refreshLog();
      if (!wifiPanel.hidden) refreshWifiNetwork();
    }

    tabs.forEach((tab, index) => {
      tab.addEventListener('click', () => selectTab(tab));
      tab.addEventListener('keydown', event => {
        if (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight') return;
        event.preventDefault();
        const step = event.key === 'ArrowRight' ? 1 : -1;
        const next = tabs[(index + step + tabs.length) % tabs.length];
        next.focus();
        selectTab(next);
      });
    });
    refreshStatus();
  </script>
</body>
</html>
)HTML";
