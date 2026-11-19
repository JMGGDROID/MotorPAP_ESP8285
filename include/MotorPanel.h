#pragma once

#include <pgmspace.h>

constexpr char motorPanelHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>MotorPAP · TMC5160</title>
  <style>
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
    .top, main, .foot { max-width: 1600px; width: 100%; margin: 0 auto; padding: .9rem clamp(.7rem, 1.1vw, 1.25rem); }
    .top { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 1.15rem; }
    h1, h2, p { margin: 0; }
    h1 { font-size: clamp(1.45rem, 2.4vw, 2rem); letter-spacing: -.035em; }
    h2 { font-size: 1.07rem; letter-spacing: -.02em; }
    .intro { display: flex; align-items: center; gap: .85rem; }
    .brand-icon { color: #0875db; font-size: 3.1rem; line-height: 1; }
    .intro p, small { color: var(--muted); }
    .intro p { margin-top: .2rem; }
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
    main { flex: 1; display: grid; grid-template-columns: minmax(0, 1.03fr) minmax(0, 1.08fr) minmax(0, 1fr); gap: .75rem; align-items: start; align-content: start; }
    .stack { display: grid; gap: .65rem; min-width: 0; }
    fieldset { min-width: 0; border: 0; margin: 0; padding: 0; }
    .notice { grid-column: 1 / -1; margin: 0; padding: .45rem .75rem; background: #edf2f8; border: 1px solid var(--line); border-radius: .4rem; color: var(--muted); font-size: .86rem; }
    .row { display: flex; align-items: center; justify-content: space-between; gap: .6rem; }
    .row, .range, .choices, .actions { margin-top: .55rem; }
    label { font-size: .87rem; }
    input, button { font: inherit; }
    input[type="number"], button { border: 1px solid #bbc8d8; border-radius: .35rem; padding: .36rem .45rem; min-height: 2rem; }
    input[type="number"] { width: 5.8rem; background: #f3f6fa; color: var(--muted); opacity: 1; }
    input[type="range"] { min-width: 0; flex: 1; }
    .range { display: flex; align-items: center; gap: .4rem; }
    .limits { display: flex; justify-content: space-between; margin-top: .25rem; }
    .rpm { display: block; margin-top: .45rem; color: var(--muted); }
    .choices { display: flex; flex-wrap: wrap; gap: .4rem; }
    button:disabled { opacity: 1; cursor: not-allowed; }
    .choices button { flex: 1; color: #25334a; background: linear-gradient(#fff, #edf1f6); min-height: 1.9rem; padding: .2rem .35rem; }
    .choices button[aria-pressed="true"] { background: var(--blue); color: white; }
    .driver-toggle { white-space: nowrap; }
    .driver-toggle input { position: absolute; opacity: 0; }
    .switch { display: inline-flex; align-items: center; gap: .45rem; padding: .25rem .35rem .25rem .55rem; border-radius: 1.1rem; background: #667587; color: white; font-size: .7rem; font-weight: 700; }
    .switch::after { content: ""; width: 1.15rem; height: 1.15rem; border-radius: 50%; background: #fff; }
    .driver-toggle input:checked + .switch { background: #00a76a; }
    .driver-toggle input:focus-visible + .switch { outline: 2px solid var(--blue); outline-offset: 2px; }
    .driver-toggle input:not(:disabled) + .switch { cursor: pointer; }
    .metrics, .actions { display: grid; grid-template-columns: 1fr 1fr; gap: .55rem; }
    .metrics .card { min-height: 5.15rem; }
    .metric { display: inline-block; color: #142238; font-size: 1.85rem; font-weight: 750; margin: .45rem .4rem 0 0; }
    .metrics .card:first-child .metric { color: #1239da; }
    .actions.jog { grid-template-columns: repeat(3, 1fr); }
    .actions button, .position button { background: linear-gradient(#2179df, #1059ab); border-color: #1162ba; color: white; font-weight: 700; min-height: 2.55rem; box-shadow: 0 2px 4px #17304e25; }
    .actions.jog button { min-height: 4rem; }
    .actions .stop { grid-column: 1 / -1; background: linear-gradient(#f74132, #cb291f); border-color: #c72922; }
    .button-icon { display: block; font-size: 1.4rem; line-height: 1.05; }
    .actions small { display: block; color: #edf5ff; }
    .position .row { flex-wrap: wrap; justify-content: flex-start; }
    .diagnostics { margin: .55rem 0 0; }
    .diagnostics div { display: flex; justify-content: space-between; gap: .5rem; padding: .4rem 0; border-top: 1px solid var(--line); font-size: .87rem; }
    .diagnostics dt::before { content: ""; display: inline-block; width: .65rem; height: .65rem; margin: 0 .55rem 0 .1rem; border-radius: 50%; background: #838b96; }
    .diagnostics [data-level="ok"] dt::before { background: #159444; }
    .diagnostics [data-level="warn"] dt::before { background: #bf7900; }
    .diagnostics [data-level="fault"] dt::before { background: #cc3026; }
    .diagnostics dd { margin: 0; color: var(--muted); text-align: right; }
    .tabs { display: flex; gap: .2rem; }
    .telemetry-head { display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: .5rem; }
    .tabs button { background: #fff; color: var(--blue); cursor: pointer; }
    .tabs [aria-pressed="true"] { background: var(--blue); color: #fff; }
    .plot {
      display: grid; place-items: center; text-align: center; min-height: 12rem; margin-top: .65rem;
      border: 1px solid var(--line); border-radius: .35rem; color: var(--muted); padding: .6rem;
      background: repeating-linear-gradient(0deg, transparent 0 23px, #e7edf5 23px 24px),
                  repeating-linear-gradient(90deg, transparent 0 23px, #e7edf5 23px 24px), #f8fafd;
    }
    .plot span { background: #f8fafd; padding: .4rem; }
    .axis { display: block; text-align: center; margin-top: .3rem; }
    footer { background: linear-gradient(#203a5a, #172d48); color: #f1f5fa; }
    .foot { display: flex; flex-wrap: wrap; gap: .5rem 1.1rem; font-size: .86rem; }
    .foot span + span { border-left: 1px solid #6d8099; padding-left: 1.1rem; }
    @media (max-width: 68rem) {
      main { grid-template-columns: repeat(2, minmax(0, 1fr)); }
      .monitoring { grid-column: 1 / -1; grid-template-columns: repeat(2, minmax(0, 1fr)); }
    }
    @media (max-width: 43rem) {
      main, .monitoring { grid-template-columns: 1fr; }
      .status-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
      .foot span + span { padding-left: 0; border: 0; }
    }
  </style>
</head>
<body>
  <header>
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
  <main>
    <p class="notice" id="driver-notice">Habilita el driver para mover. STOP desacelera y mantiene la sujeción. Apaga el interruptor para liberar el motor.</p>
    <section class="stack" aria-label="Ajustes del motor" aria-describedby="driver-notice">
      <section class="card row" style="margin-top:0">
        <div><h2><span class="icon" aria-hidden="true">🔒︎</span>Habilitar driver</h2><small id="current-summary">0,80 A RMS en marcha · 0,40 A RMS en reposo</small></div>
        <label class="driver-toggle"><input id="driver-toggle" type="checkbox" role="switch" aria-label="Habilitar driver" disabled><span class="switch" id="driver-switch" aria-hidden="true">OFF</span></label>
      </section>
      <section class="card">
        <h2><span class="icon" aria-hidden="true">⟳</span>Sentido de giro</h2>
        <div class="choices">
          <label><input type="radio" name="direction" value="cw" checked disabled> Horario (CW)</label>
          <label><input type="radio" name="direction" value="ccw" disabled> Antihorario (CCW)</label>
        </div>
        <small id="direction-hint">Mover + usa este sentido. Mover − usa el contrario.</small>
      </section>
      <section class="card">
        <h2><span class="icon" aria-hidden="true">◴</span>Velocidad</h2>
        <div class="range"><input id="speed-slider" type="range" min="100" max="29333" value="100" aria-label="Ajustar velocidad"><input id="jog-speed" type="number" min="100" max="29333" step="1" value="100" required aria-label="Velocidad en micropasos por segundo"><small>µpasos/s</small></div>
        <div class="limits"><small>100</small><small id="speed-limit">29.333 µpasos/s · máx. 550 RPM</small></div>
        <p class="rpm"><output id="target-rpm" for="speed-slider jog-speed">—</output>
          <small> · 200 pasos/vuelta × <span id="rpm-microsteps">16</span> micropasos</small></p>
      </section>
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
      <section class="card row" style="margin-top:0">
        <div><h2>Modo silencioso</h2><small id="chopper-mode">SpreadCycle · Ajusta con el driver en OFF.</small></div>
        <label class="driver-toggle"><input id="silent-mode-toggle" type="checkbox" role="switch" aria-label="Modo silencioso StealthChop" disabled><span class="switch" id="silent-mode-switch" aria-hidden="true">OFF</span></label>
      </section>
      <fieldset class="card" id="current-controls" disabled>
        <h2><span class="icon" aria-hidden="true">ϟ</span>Corriente del motor</h2>
        <div class="choices">
          <label>Marcha RMS<br><input id="run-current" type="number" min="0.10" max="1.20" step="0.01" value="0.80" required> A</label>
          <label>Mantenimiento RMS<br><input id="hold-current" type="number" value="0.40" readonly> A</label>
        </div>
        <div class="row"><small>0,10 a 1,20 A RMS · Ajusta con el driver en OFF.</small><button type="button" id="apply-current">Aplicar</button></div>
      </fieldset>
    </section>
    <div class="stack">
      <fieldset class="card" aria-labelledby="movement" aria-describedby="driver-notice">
        <h2 id="movement"><span class="icon" aria-hidden="true">✥</span>Controles de movimiento</h2>
        <div class="actions jog">
          <button id="jog-negative" type="button" disabled><span class="button-icon" aria-hidden="true">←</span>Mover −<small>(Jog)</small></button>
          <button type="button" disabled><span class="button-icon" aria-hidden="true">⌂</span>Inicio</button>
          <button id="jog-positive" type="button" disabled><span class="button-icon" aria-hidden="true">→</span>Mover +<small>(Jog)</small></button>
        </div>
        <div class="actions"><button class="stop" id="stop-motion" type="button" disabled><span aria-hidden="true">Ⅱ </span>STOP</button></div>
        <p id="motion-feedback" role="status" class="row"></p>
      </fieldset>
      <fieldset class="card position" disabled aria-labelledby="position">
        <h2 id="position"><span class="icon" aria-hidden="true">◎</span>Posicionamiento</h2>
        <div class="row"><label>Posición (pasos) <input type="number"></label><button>Mover a posición</button></div>
      </fieldset>
      <section class="metrics" aria-label="Lecturas de posición">
        <div class="card"><h2>Posición objetivo</h2><span class="metric">—</span><small>pasos</small></div>
        <div class="card"><h2>Posición actual</h2><span class="metric">—</span><small>pasos</small></div>
      </section>
    </div>
    <div class="stack monitoring">
      <section class="card">
        <h2><span class="icon" aria-hidden="true">⌁</span>Estado del TMC5160</h2>
        <dl class="diagnostics">
          <div><dt>Identificación SPI</dt><dd id="driver-id">—</dd></div>
          <div><dt>Entrada EN</dt><dd id="driver-enable">—</dd></div>
          <div><dt>OT · Sobretemperatura</dt><dd id="diag-temperature">—</dd></div>
          <div><dt>UV · Subtensión</dt><dd id="diag-voltage">—</dd></div>
          <div><dt>SG · StallGuard</dt><dd id="diag-stallguard">—</dd></div>
          <div><dt>Error</dt><dd id="diag-error">—</dd></div>
        </dl>
      </section>
      <section class="card">
        <div class="telemetry-head">
          <h2><span class="icon" aria-hidden="true">↗</span>Telemetría en tiempo real</h2>
          <div class="tabs" aria-label="Serie de telemetría">
            <button type="button" aria-pressed="true">Posición</button>
            <button type="button" aria-pressed="false">Velocidad</button>
          </div>
        </div>
        <div class="plot"><span id="plot-status" aria-live="polite">Sin datos de posición</span></div>
        <small class="axis">Tiempo (s)</small>
      </section>
    </div>
  </main>
  <footer><div class="foot">
    <span>IP <strong id="ip">—</strong></span><span>SPI <strong id="spi-link">—</strong></span>
    <span>Tiempo activo <strong id="uptime">—</strong></span>
  </div></footer>
  <script>
    const byId = (id) => document.getElementById(id);
    let latestStatus = null;
    let pendingCommand = null;
    let stopPending = false;
    let driverPending = false;
    let settingPending = false;
    let motionId = null;
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
    const driverToggle = byId('driver-toggle');
    const silentModeToggle = byId('silent-mode-toggle');
    const directions = document.querySelectorAll('input[name="direction"]');
    const microstepButtons = document.querySelectorAll('#microstep-controls button');
    const motorStepsPerRevolution = 200;
    let microstepsPerStep = 16;
    const rpmFormat = new Intl.NumberFormat('es-ES', {maximumFractionDigits: 2});
    const pulseFormat = new Intl.NumberFormat('es-ES');
    const currentFormat = new Intl.NumberFormat('es-ES', {minimumFractionDigits: 2, maximumFractionDigits: 2});

    function formatRpm(pulseSpeed) {
      const rpm = pulseSpeed * 60 / (motorStepsPerRevolution * microstepsPerStep);
      return rpmFormat.format(rpm) + ' RPM';
    }

    function updateTargetRpm() {
      byId('target-rpm').value = speedInput.validity.valid
        ? formatRpm(speedInput.valueAsNumber) : '—';
      updateRampRate(rampInput, 'ramp-acceleration');
      updateRampRate(decelerationInput, 'ramp-deceleration');
    }

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

    function updateControls() {
      const busy = Boolean(latestStatus?.moving || pendingCommand || motionId !== null);
      const canJog = latestStatus?.driverResponding && latestStatus.stepDirMode &&
                     latestStatus.driverEnabled && !latestStatus.silentInitializing && !latestStatus.moving &&
                     !busy && !stopPending && !driverPending && !settingPending;
      const canChangeDirection = canChangeSpeed() && !settingPending;
      byId('jog-negative').disabled = !canJog && !canChangeDirection;
      byId('jog-positive').disabled = !canJog && !canChangeDirection;
      byId('stop-motion').disabled = stopPending || latestStatus?.stopping ||
        (!latestStatus && !pendingCommand);
      driverToggle.disabled = driverPending || settingPending || !latestStatus ||
        (!latestStatus.driverEnabled && (!latestStatus.driverResponding || !latestStatus.stepDirMode));
      if (!driverPending) driverToggle.checked = latestStatus?.driverEnabled === true;
      byId('driver-switch').textContent = driverToggle.checked ? 'ON' : 'OFF';
      speedInput.disabled = speedSlider.disabled = busy && !canChangeSpeed();
      rampInput.disabled = rampSlider.disabled = busy;
      decelerationInput.disabled = decelerationSlider.disabled = busy;
      byId('microstep-controls').disabled = !latestStatus || latestStatus.driverEnabled ||
        busy || driverPending || settingPending;
      byId('current-controls').disabled = byId('microstep-controls').disabled;
      silentModeToggle.disabled = byId('microstep-controls').disabled;
      for (const input of directions) {
        input.disabled = !latestStatus || (busy && !canChangeSpeed()) || settingPending;
      }
      if (latestStatus && !settingPending) {
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
        byId('current-summary').textContent =
          currentFormat.format(latestStatus.runCurrentMa / 1000) + ' A RMS en marcha · ' +
          currentFormat.format(latestStatus.holdCurrentMa / 1000) + ' A RMS en reposo';
        for (const input of directions) {
          const clockwise = latestStatus.moving ? latestStatus.requestedClockwise : latestStatus.clockwise;
          input.checked = (input.value === 'cw') === clockwise;
        }
        byId('direction-hint').textContent = latestStatus.moving
          ? 'Al cambiar el sentido, el motor baja a cero y vuelve a acelerar.'
          : 'Mover + usa este sentido. Mover − usa el contrario.';
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

    async function sendCommand(path, signal) {
      const response = await fetch(path, {
        method: 'POST',
        headers: {'X-MotorPAP-Command': '1'},
        signal
      });
      if (!response.ok) {
        throw Object.assign(new Error(await response.text()), {status: response.status});
      }
      return response.text();
    }

    function canChangeSpeed() {
      return motionId !== null && latestStatus?.driverEnabled && !latestStatus.stopping &&
        !stopPending && !driverPending;
    }

    function cancelSpeedChange() {
      clearTimeout(speedTimer);
      speedTimer = null;
      pendingSpeed = null;
    }

    function scheduleSpeedChange() {
      if (speedTimer === null && !speedRequestPending && pendingSpeed !== null) {
        speedTimer = setTimeout(applySpeedChange, 120);
      }
    }

    function queueSpeedChange() {
      if (canChangeSpeed() && speedInput.validity.valid) {
        pendingSpeed = speedInput.valueAsNumber;
        scheduleSpeedChange();
      }
    }

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

    async function jog(direction) {
      if (canChangeSpeed()) {
        settingPending = true;
        byId('motion-feedback').textContent = '';
        updateControls();
        try {
          await sendCommand('/api/jog/' + direction + '?id=' + motionId);
          if (latestStatus) {
            latestStatus.requestedClockwise = (direction === 'positive') === latestStatus.clockwise;
          }
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
        if (latestStatus) {
          latestStatus.moving = true;
          latestStatus.requestedClockwise = (direction === 'positive') === latestStatus.clockwise;
        }
      } catch (error) {
        byId('motion-feedback').textContent = error.message;
      } finally {
        pendingCommand = null;
        updateControls();
      }
    }

    async function stopMotion() {
      stopPending = true;
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
      } catch (error) {
        byId('motion-feedback').textContent = error.message;
      } finally {
        stopPending = false;
        updateControls();
      }
    }

    async function changeDriver() {
      const enabled = driverToggle.checked;
      driverPending = true;
      byId('motion-feedback').textContent = '';
      if (!enabled) {
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
        byId('motion-feedback').textContent = error.message;
      } finally {
        driverPending = false;
        updateControls();
      }
    }

    driverToggle.addEventListener('change', changeDriver);

    async function changeSetting(name, value) {
      const directionId = name === 'direction' && canChangeSpeed() ? motionId : null;
      settingPending = true;
      byId('motion-feedback').textContent = '';
      updateControls();
      try {
        const parameter = name === 'current' ? 'ma' : 'value';
        const command = '/api/' + name + '?' + parameter + '=' + value;
        const response = await sendCommand(command + (directionId === null ? '' : '&id=' + directionId));
        if (latestStatus) {
          if (name === 'direction') {
            latestStatus.clockwise = value === 'cw';
            if (directionId !== null) latestStatus.requestedClockwise = value === 'cw';
          }
          else Object.assign(latestStatus, JSON.parse(response));
        }
      } catch (error) {
        byId('motion-feedback').textContent = error.message;
      } finally {
        settingPending = false;
        updateControls();
      }
    }

    for (const input of directions) {
      input.addEventListener('change', () => changeSetting('direction', input.value));
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
    byId('jog-negative').addEventListener('click', () => jog('negative'));
    byId('jog-positive').addEventListener('click', () => jog('positive'));
    byId('stop-motion').addEventListener('click', stopMotion);
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
    const series = document.querySelectorAll('.tabs button');
    for (const button of series) {
      button.addEventListener('click', () => {
        for (const item of series) item.setAttribute('aria-pressed', item === button);
        byId('plot-status').textContent = 'Sin datos de ' + button.textContent.toLowerCase();
      });
    }

    function setConnection(online) {
      byId('wifi').textContent = online ? 'Conectado' : 'Sin respuesta';
      byId('controller').textContent = online ? 'En línea' : 'Sin respuesta';
      byId('wifi').classList.toggle('online', online);
      byId('controller').classList.toggle('online', online);
    }

    function setDriver(status) {
      if (latestStatus?.stopping && status?.moving === false) {
        byId('motion-feedback').textContent = 'Movimiento detenido.';
      }
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

    function updateDiagnostics(status) {
      if (!status?.diagnosticsAvailable) {
        for (const id of ['diag-temperature', 'diag-voltage', 'diag-stallguard', 'diag-error']) {
          showDiagnostic(id, 'Sin lectura');
        }
        return;
      }
      if (status.overtemperature) showDiagnostic('diag-temperature', 'Sobretemperatura', 'fault');
      else if (status.temperatureWarning) showDiagnostic('diag-temperature', 'Preaviso térmico', 'warn');
      else showDiagnostic('diag-temperature', 'Normal', 'ok');

      showDiagnostic('diag-voltage', status.undervoltage ? 'Bomba de carga · evento registrado' : 'Sin evento UV',
        status.undervoltage ? 'warn' : 'ok');
      const fault = status.shortCircuit ? 'Cortocircuito' : (status.driverError ? 'Fallo del driver' : '');
      showDiagnostic('diag-error', fault || 'Sin error', fault ? 'fault' : 'ok');

      if (!status.moving) showDiagnostic('diag-stallguard', 'En reposo');
      else if (status.silentActive) showDiagnostic('diag-stallguard', 'No disponible en StealthChop');
      else if (status.stallGuardResult >= 0) showDiagnostic('diag-stallguard', status.stallGuardResult + ' / 1023');
      else showDiagnostic('diag-stallguard', 'Esperando lectura en marcha');
    }

    async function refreshStatus() {
      const request = new AbortController();
      const timeout = setTimeout(() => request.abort(), 2500);
      try {
        const id = motionId;
        if (id !== null) {
          try {
            await sendCommand('/api/jog/keepalive?id=' + id, request.signal);
          } catch (error) {
            if (motionId === id) {
              motionId = null;
              cancelSpeedChange();
              if (error.status !== 409) {
                byId('motion-feedback').textContent = 'Se interrumpió la comunicación de movimiento.';
              }
            }
          }
        }
        const response = await fetch('/api/status', {signal: request.signal});
        if (!response.ok) throw new Error(response.status);
        const status = await response.json();
        setConnection(true);
        setDriver(status);
        byId('signal').textContent = status.rssi + ' dBm';
        byId('ip').textContent = status.ip;
        const seconds = status.uptime;
        byId('uptime').textContent =
          [Math.floor(seconds / 3600), Math.floor(seconds / 60) % 60, seconds % 60]
            .map((value) => String(value).padStart(2, '0')).join(':');
      } catch {
        setConnection(false);
        setDriver(null);
        byId('signal').textContent = '— dBm';
        byId('ip').textContent = '—';
        byId('uptime').textContent = '—';
      } finally {
        clearTimeout(timeout);
        setTimeout(refreshStatus, 500);
      }
    }
    refreshStatus();
  </script>
</body>
</html>
)HTML";
