// FilamentMonitor - frontend
const $ = (id) => document.getElementById(id);

// Tryb kiosk (panel Waveshare 7"): ?kiosk=1 chowa menu i stopke,
// zostawia sam pulpit - strona ma byc zakladka na panelu.
if (new URLSearchParams(location.search).get('kiosk') === '1') {
  document.body.classList.add('kiosk');
}

// --- Zakładki ---
document.querySelectorAll('.tab').forEach((t) => {
  t.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach((x) => x.classList.remove('active'));
    document.querySelectorAll('.tabpage').forEach((x) => x.classList.remove('active'));
    t.classList.add('active');
    $('tab-' + t.dataset.tab).classList.add('active');
    if (t.dataset.tab === 'logi') { refreshLog(); refreshSd(); }
    if (t.dataset.tab === 'wykresy') loadHistory();
    if (t.dataset.tab === 'kalibracja') refreshCal();
    if (t.dataset.tab === 'diagnostyka') refreshDiag();
  });
});

const FMT = (v, dec) => (v === null || v === undefined || Number.isNaN(v)) ? '--' : Number(v).toFixed(dec);

function chip(id, ok, label) {
  const el = $(id);
  el.textContent = label;
  el.classList.toggle('ok', ok === true);
  el.classList.toggle('bad', ok === false);
}

function setMetric(id, value, dec) {
  $(id).textContent = FMT(value, dec);
}

function aqiStyle(aqi) {
  if (aqi <= 50)   return { c: '#00C853', label: 'Dobra' };
  if (aqi <= 100)  return { c: '#FFD600', label: 'Umiarkowana' };
  if (aqi <= 150)  return { c: '#FF9100', label: 'Zła (wrażliwe grupy)' };
  if (aqi <= 200)  return { c: '#E53935', label: 'Zła' };
  if (aqi <= 300)  return { c: '#8E24AA', label: 'Bardzo zła' };
  return { c: '#7B1F1F', label: 'Niebezpieczna' };
}

// Progi zdrowotne dla PM / gazów (kolory spójne z legendą)
function pm25Color(v) { return v <= 10 ? '#00C853' : v <= 20 ? '#76FF03' : v <= 35 ? '#FFD600' : v <= 50 ? '#FF9100' : '#E53935'; }
function pm10Color(v) { return v <= 20 ? '#00C853' : v <= 45 ? '#FFD600' : v <= 75 ? '#FF9100' : '#E53935'; }
function co2Color(v)  { return v <= 600 ? '#00C853' : v <= 1000 ? '#FFD600' : v <= 1500 ? '#FF9100' : '#E53935'; }
function tvocColor(v) { return v <= 0.3 ? '#00C853' : v <= 0.5 ? '#FFD600' : v <= 1.0 ? '#FF9100' : '#E53935'; }
function ch2oColor(v) { return v <= 20 ? '#00C853' : v <= 40 ? '#FFD600' : v <= 80 ? '#FF9100' : '#E53935'; }
function lvlColor(v)  { return v <= 1 ? '#00C853' : v <= 2 ? '#FFD600' : v <= 3 ? '#FF9100' : '#E53935'; }
function rsColor(v)   { return v >= 0.8 ? '#00C853' : v >= 0.5 ? '#FFD600' : '#E53935'; }
function coColor(v)   { return v <= 9 ? '#00C853' : v <= 35 ? '#FFD600' : v <= 100 ? '#FF9100' : '#E53935'; }

// Poziom kategorii (0..3) dla danego parametru (-1 = brak danych).
function catLevel(v, fn) {
  if (v == null) return -1;
  const c = fn(v);
  if (c === '#00C853' || c === '#76FF03') return 0;
  if (c === '#FFD600') return 1;
  if (c === '#FF9100') return 2;
  return 3;
}

// Parametr, ktory najbardziej obniza ocene jakosci powietrza.
function dominantCause(byId) {
  const items = [
    ['PM2.5', byId.pm25 && byId.pm25.valid ? byId.pm25.value : null, pm25Color],
    ['PM10',  byId.pm10  && byId.pm10.valid  ? byId.pm10.value  : null, pm10Color],
    ['CO₂',   byId.co2   && byId.co2.valid   ? byId.co2.value   : null, co2Color],
    ['TVOC',  byId.tvoc  && byId.tvoc.valid  ? byId.tvoc.value  : null, tvocColor],
    ['CH₂O',  byId.ch2o  && byId.ch2o.valid  ? byId.ch2o.value  : null, ch2oColor],
  ];
  let name = null, lvl = -1;
  for (const [n, v, fn] of items) {
    const l = catLevel(v, fn);
    if (l > lvl) { lvl = l; name = n; }
  }
  return name ? ('Największy wpływ: ' + name) : 'Największy wpływ: —';
}

function colorMetric(id, ch, fn) {
  $(id).style.color = (ch && ch.valid) ? fn(ch.value) : '';
}

// --- Pulpit ---
function renderStatus(s) {
  const byId = {};
  (s.channels || []).forEach((c) => byId[c.id] = c);

  const aqi = byId.aqi ? byId.aqi.value : null;
  const st = aqiStyle(aqi || 0);
  $('aqi-value').textContent = FMT(aqi, 0);
  $('aqi-value').style.color = st.c;
  $('aqi-label').textContent = (byId.aqi && byId.aqi.valid) ? st.label : 'czekam na dane…';
  $('aqi-cause').textContent = dominantCause(byId);
  const pct = Math.max(0, Math.min(100, (aqi || 0) / 500 * 100));
  $('aqi-marker').style.left = pct + '%';

  setMetric('m-pm1',   byId.pm1 && byId.pm1.valid ? byId.pm1.value : null, 0);
  setMetric('m-pm25',  byId.pm25 && byId.pm25.valid ? byId.pm25.value : null, 0);
  setMetric('m-pm10',  byId.pm10 && byId.pm10.valid ? byId.pm10.value : null, 0);
  setMetric('m-co2',   byId.co2 && byId.co2.valid ? byId.co2.value : null, 0);
  setMetric('m-ch2o',  byId.ch2o && byId.ch2o.valid ? byId.ch2o.value : null, 0);
  setMetric('m-tvoc',  byId.tvoc && byId.tvoc.valid ? byId.tvoc.value : null, 3);
  setMetric('m-tvoclvl', byId.tvoclvl && byId.tvoclvl.valid ? byId.tvoclvl.value : null, 0);
  setMetric('m-temp', byId.temp && byId.temp.valid ? byId.temp.value : null, 1);
  setMetric('m-hum',  byId.hum  && byId.hum.valid  ? byId.hum.value  : null, 0);
  setMetric('m-pm25-1m',  byId.pm25_1m  && byId.pm25_1m.valid  ? byId.pm25_1m.value  : null, 0);
  setMetric('m-pm25-5m',  byId.pm25_5m  && byId.pm25_5m.valid  ? byId.pm25_5m.value  : null, 0);
  setMetric('m-pm25-24h', byId.pm25_24h && byId.pm25_24h.valid ? byId.pm25_24h.value : null, 0);
  setMetric('m-temp-24h', byId.temp_24h && byId.temp_24h.valid ? byId.temp_24h.value : null, 1);
  setMetric('m-hum-24h',  byId.hum_24h  && byId.hum_24h.valid  ? byId.hum_24h.value  : null, 0);
  setMetric('m-tempext', byId.tempext && byId.tempext.valid ? byId.tempext.value : null, 1);
  setMetric('m-humext',  byId.humext  && byId.humext.valid  ? byId.humext.value  : null, 0);
  const rhc = byId.rhcorr;
  const rhHint = $('pm-rh-hint');
  if (rhHint) rhHint.textContent = (rhc && rhc.valid)
    ? ('Korekcja wilgotności PM: ON (RH ' + Number(rhc.value).toFixed(0) + '%)')
    : 'Korekcja wilgotności PM: OFF (RH poniżej progu)';
  setMetric('m-micsv', byId.mics_v && byId.mics_v.valid ? byId.mics_v.value : null, 3);
  colorMetric('m-pm25',  byId.pm25,  pm25Color);
  colorMetric('m-pm10',  byId.pm10,  pm10Color);
  colorMetric('m-co2',   byId.co2,   co2Color);
  colorMetric('m-tvoc',  byId.tvoc,  tvocColor);
  colorMetric('m-ch2o',  byId.ch2o,  ch2oColor);
  colorMetric('m-tvoclvl', byId.tvoclvl, lvlColor);
  setMetric('m-micsrs', byId.mics_rs && byId.mics_rs.valid ? byId.mics_rs.value : null, 2);
  colorMetric('m-micsrs', byId.mics_rs, rsColor);
  const warmup = byId.mics_wu && byId.mics_wu.valid && byId.mics_wu.value > 0;
  setMetric('m-micsco', (!warmup && byId.mics_co && byId.mics_co.valid) ? byId.mics_co.value : null, 0);
  colorMetric('m-micsco', (!warmup && byId.mics_co && byId.mics_co.valid) ? byId.mics_co : null, coColor);
  $('m-micsstate').textContent = warmup ? 'rozgrzewanie…'
    : (byId.mics_rs && byId.mics_rs.valid)
      ? (byId.mics_rs.value < 0.7 ? 'wykryto gaz' : 'czyste') : '--';
  $('m-micsstate').style.color = warmup ? '' :
    (byId.mics_rs && byId.mics_rs.valid)
      ? rsColor(byId.mics_rs.value) : '';

  const motion = byId.motion && byId.motion.valid && byId.motion.value > 0;
  $('motion').textContent = motion ? '● Ruch wykryty' : 'Bez ruchu';
  $('motion').classList.toggle('on', motion);

  setMetric('m-mq7v', byId.mq7_v && byId.mq7_v.valid ? byId.mq7_v.value : null, 3);
  setMetric('m-mq7rs', byId.mq7_rs && byId.mq7_rs.valid ? byId.mq7_rs.value : null, 2);
  colorMetric('m-mq7rs', byId.mq7_rs, rsColor);
  setMetric('m-mq7co', byId.mq7_co && byId.mq7_co.valid ? byId.mq7_co.value : null, 0);
  colorMetric('m-mq7co', byId.mq7_co, coColor);

  const pirLive = $('pir-live');
  if (pirLive) {
    const raw = s.pir_raw ? 1 : 0;
    pirLive.textContent = raw ? '● WYSOKI (ruch)' : '○ NISKI (spokój)';
    pirLive.style.color = raw ? '#2ecc9a' : '#888';
  }

  // status chips
  $('chip-time').textContent = s.time || '--:--:--';
  chip('chip-wifi', s.wifi_mode === 'STA', 'WiFi ' + (s.wifi_mode || '?'));
  chip('chip-mqtt', s.mqtt === true, s.mqtt ? 'MQTT ok' : 'MQTT off');
  chip('chip-sd', s.sd === true, s.sd ? 'SD ok' : 'SD brak');
  $('ver').textContent = s.version || '?';
  $('foot-sys').textContent = 'heap ' + (s.free_heap / 1024).toFixed(0) + ' kB · PSRAM ' + (s.free_psram / 1024).toFixed(0) + ' kB';
}

// --- Filament ---
const FILAMENT_TYPES = [
  'PLA', 'PLA+', 'PLA Pro', 'PLA Silk',
  'PETG', 'PETG+', 'C-PETG',
  'ABS', 'ABS+', 'ASA',
  'TPU', 'TPE',
  'Nylon (PA)', 'PC', 'PVA', 'PET', 'PET-CF',
  'HIPS', 'PP', 'POM',
  'Inne'
];
const FILAMENT_RH = {
  'PLA': 50, 'PLA+': 50, 'PLA Pro': 50, 'PLA Silk': 40,
  'PETG': 40, 'PETG+': 35, 'C-PETG': 35,
  'ABS': 40, 'ABS+': 40, 'ASA': 40,
  'TPU': 20, 'TPE': 20,
  'Nylon (PA)': 15, 'PC': 20, 'PVA': 15, 'PET': 40, 'PET-CF': 20,
  'HIPS': 40, 'PP': 50, 'POM': 30,
  'Inne': 30
};

function renderBoxes(s) {
  const wrap = $('boxes');
  wrap.innerHTML = '';
  (s.boxes || []).forEach((b) => {
    const div = document.createElement('div');
    div.className = 'box ' + (b.state || 'off');
    const target = (b.target_rh != null) ? (' · cel ≤' + b.target_rh + '%') : '';
    const sub = [b.type, b.brand].filter(Boolean).join(' · ');
    div.innerHTML =
      '<h3>' + (b.name || ('Pojemnik ' + b.n)) + '</h3>' +
      (sub ? '<div class="t">' + sub + '</div>' : '') +
      (b.color ? '<div class="t">' + b.color + '</div>' : '') +
      '<div class="rh">' + FMT(b.hum, 1) + '%</div>' +
      '<div class="t">' + FMT(b.temp, 1) + ' °C' + target + '</div>' +
      '<div class="t">' + (b.online ? stateLabel(b.state) : 'brak danych') + '</div>';
    wrap.appendChild(div);
  });
}

function stateLabel(st) {
  return { ok: '● suchy', warn: '● uwaga', alarm: '● mokry', brak: 'brak danych' }[st] || st;
}

// --- Status ---
async function refreshStatus() {
  try {
    const r = await fetch('/api/status');
    if (r.ok) {
      const s = await r.json();
      renderStatus(s);
      renderBoxes(s);
    }
  } catch (e) { /* serwer niedostępny */ }
}

// --- Kalibracja ---
async function refreshCal() {
  try {
    const s = await (await fetch('/api/status')).json();
    const byId = {};
    (s.channels || []).forEach((c) => byId[c.id] = c);
    setMetric('cal-v', byId.mics_v && byId.mics_v.valid ? byId.mics_v.value : null, 3);
    setMetric('cal-mq7-v', byId.mq7_v && byId.mq7_v.valid ? byId.mq7_v.value : null, 3);
    const ht = $('cal-heater');
    if (ht) ht.textContent = (s.mics_heater ? 'ON (grzeje)' : 'OFF');
    const cfg = await (await fetch('/api/config')).json();
    setMetric('cal-r0', cfg.mics_r0 > 0 ? cfg.mics_r0 : null, 3);
    setMetric('cal-mq7-r0', cfg.mq7_r0 > 0 ? cfg.mq7_r0 : null, 3);
  } catch (e) {}
}

async function setHeater(on) {
  const r = await fetch('/api/heater?on=' + (on ? 1 : 0), { method: 'POST' });
  try { await r.json(); } catch (e) {}
  refreshCal();
}

async function calibrateMics() {
  const r = await fetch('/api/calibrate', { method: 'POST' });
  const j = await r.json();
  if (j.ok) { alert('R0 zapisane: ' + j.r0 + ' V'); refreshCal(); }
  else alert('Błąd: ' + (j.error || 'nieznany'));
}

async function calibrateMq7() {
  const r = await fetch('/api/calibrate_mq7', { method: 'POST' });
  const j = await r.json();
  if (j.ok) { alert('R0 zapisane: ' + j.r0 + ' V'); refreshCal(); }
  else alert('Błąd: ' + (j.error || 'nieznany'));
}

// --- Konfiguracja ---
function buildBoxCfg() {
  const wrap = $('box-cfg');
  wrap.innerHTML = '';
  const opts = FILAMENT_TYPES.map((t) => '<option>' + t + '</option>').join('');
  for (let i = 0; i < 16; i++) {
    const card = document.createElement('div');
    card.className = 'boxcfg';
    card.innerHTML =
      '<h3>Pojemnik ' + (i + 1) + '</h3>' +
      '<label>Nazwa<input id="box-name-' + i + '" type="text" placeholder="Pojemnik ' + (i + 1) + '"></label>' +
      '<label>Rodzaj filamentu<select id="box-type-' + i + '">' + opts + '</select></label>' +
      '<label>Kolor<input id="box-color-' + i + '" type="text" placeholder="np. czarny"></label>' +
      '<label>Producent<input id="box-brand-' + i + '" type="text" placeholder="np. Prusament"></label>' +
      '<label>Wilgotność docelowa [% RH]<input id="box-rh-' + i + '" type="number" step="0.5" min="0" max="100"></label>';
    wrap.appendChild(card);
  }
  for (let i = 0; i < 16; i++) {
    $('box-type-' + i).addEventListener('change', () => {
      const t = $('box-type-' + i).value;
      if (FILAMENT_RH[t] != null) $('box-rh-' + i).value = FILAMENT_RH[t];
    });
  }
}

function fillConfig(c) {
  $('cfg-wifi-ssid').value = c.wifi_ssid || '';
  $('cfg-wifi-pass').value = c.wifi_pass || '';
  $('cfg-hostname').value = c.hostname || '';
  $('cfg-wifi-static').checked = !!c.wifi_static;
  $('cfg-wifi-ip').value = c.wifi_ip || '';
  $('cfg-wifi-mask').value = c.wifi_mask || '';
  $('cfg-wifi-gw').value = c.wifi_gw || '';
  $('cfg-wifi-dns').value = c.wifi_dns || '';
  $('cfg-mqtt-host').value = c.mqtt_host || '';
  $('cfg-mqtt-port').value = c.mqtt_port;
  $('cfg-mqtt-user').value = c.mqtt_user || '';
  $('cfg-mqtt-pass').value = c.mqtt_pass || '';
  $('cfg-mqtt-prefix').value = c.mqtt_prefix || '';
  $('cfg-z2m').value = c.z2m_topic || '';
  $('cfg-ha-url').value = c.ha_url || '';
  $('cfg-ha-token').value = c.ha_token || '';
  $('cfg-ha-temp').value = c.ha_temp_entity || '';
  $('cfg-ha-hum').value = c.ha_hum_entity || '';
  $('cfg-air-en').checked = !!c.air_enabled;
  $('cfg-air-topic').value = c.air_topic || '';
  $('cfg-pm-rh-corr').checked = !!c.pm_rh_corr;
  $('cfg-pm-rh-thresh').value = (c.pm_rh_thresh != null) ? c.pm_rh_thresh : 60;
  $('cfg-pm-rh-coef').value = (c.pm_rh_coef != null) ? c.pm_rh_coef : 0.5;
  $('cfg-led-count').value = c.led_count;
  $('cfg-led-bri').value = c.led_brightness;
  $('cfg-led-ma').value = c.led_max_ma;
  $('cfg-led-matrix').checked = (c.led_matrix == null) ? true : !!c.led_matrix;
  $('cfg-led-mode').value = (c.led_mode != null) ? c.led_mode : 0;
  $('cfg-led-speed').value = (c.led_speed != null) ? c.led_speed : 5;
  $('cfg-led-layout').value = (c.led_layout != null) ? c.led_layout : 0;
  $('cfg-led-rotate').value = (c.led_rotate != null) ? c.led_rotate : 0;
  $('cfg-led-flipy').checked = !!c.led_flip_y;
  $('cfg-led-always-on').checked = (c.led_always_on == null) ? true : !!c.led_always_on;
  $('cfg-led-text').value = c.led_text || '';
  const LED_FX = ['air', 'rainbow', 'breathe', 'comet', 'sparkle', 'scroll', 'level', 'num', 'smiley', 'aqi', 'matrix', 'fire', 'plasma', 'bounce', 'auto', 'icons'];
  $('led-fx').value = LED_FX[c.led_mode] || 'air';
  $('led-fx-speed').value = (c.led_speed != null) ? c.led_speed : 5;
  $('led-fx-order').value = (c.led_order != null) ? c.led_order : 0;
  $('led-fx-font').value = (c.led_font != null) ? c.led_font : 0;
  $('led-fx-vcol').value = (c.led_value_color != null) ? c.led_value_color : 0;
  $('led-fx-text').value = c.led_text || '';
  $('led-fx-layout').value = (c.led_layout != null) ? c.led_layout : 0;
  $('led-fx-rotate').value = (c.led_rotate != null) ? c.led_rotate : 0;
  $('led-fx-flipy').checked = !!c.led_flip_y;
  $('led-fx-bri').value = (c.led_brightness != null) ? c.led_brightness : 127;
  $('led-fx-bri-val').textContent = $('led-fx-bri').value;
  $('cfg-box-meta').checked = (c.box_meta_from_node == null) ? true : !!c.box_meta_from_node;
  $('cfg-pir-en').checked = !!c.pir_enabled;
  $('cfg-pir-inv').checked = !!c.pir_inverted;
  $('cfg-pir-hold').value = Math.round((c.pir_hold_ms || 60000) / 1000);
  $('cfg-pir-deb').value = c.pir_debounce_ms || 100;
  const boxes = c.boxes || [];
  for (let i = 0; i < 16; i++) {
    const b = boxes[i] || {};
    $('box-name-' + i).value = b.name || '';
    $('box-type-' + i).value = b.type || 'PLA';
    $('box-color-' + i).value = b.color || '';
    $('box-brand-' + i).value = b.brand || '';
    $('box-rh-' + i).value = b.target_rh != null ? b.target_rh : 30;
  }
}

async function loadConfig() {
  try { fillConfig(await (await fetch('/api/config')).json()); } catch (e) {}
}

async function saveConfig() {
  const body = {
    wifi_ssid: $('cfg-wifi-ssid').value,
    wifi_pass: $('cfg-wifi-pass').value,
    hostname: $('cfg-hostname').value,
    wifi_static: $('cfg-wifi-static').checked,
    wifi_ip: $('cfg-wifi-ip').value,
    wifi_mask: $('cfg-wifi-mask').value,
    wifi_gw: $('cfg-wifi-gw').value,
    wifi_dns: $('cfg-wifi-dns').value,
    mqtt_host: $('cfg-mqtt-host').value,
    mqtt_port: parseInt($('cfg-mqtt-port').value) || 1883,
    mqtt_user: $('cfg-mqtt-user').value,
    mqtt_pass: $('cfg-mqtt-pass').value,
    mqtt_prefix: $('cfg-mqtt-prefix').value,
    z2m_topic: $('cfg-z2m').value,
    ha_url: $('cfg-ha-url').value,
    ha_token: $('cfg-ha-token').value,
    ha_temp_entity: $('cfg-ha-temp').value,
    ha_hum_entity: $('cfg-ha-hum').value,
    air_enabled: $('cfg-air-en').checked,
    air_topic: $('cfg-air-topic').value,
    pm_rh_corr: $('cfg-pm-rh-corr').checked,
    pm_rh_thresh: parseFloat($('cfg-pm-rh-thresh').value) || 60,
    pm_rh_coef: parseFloat($('cfg-pm-rh-coef').value) || 0.5,
    led_count: parseInt($('cfg-led-count').value) || 60,
    led_brightness: parseInt($('cfg-led-bri').value) || 120,
    led_max_ma: parseInt($('cfg-led-ma').value) || 900,
    led_matrix: $('cfg-led-matrix').checked,
    led_mode: parseInt($('cfg-led-mode').value) || 0,
    led_speed: parseInt($('cfg-led-speed').value) || 5,
    led_layout: parseInt($('cfg-led-layout').value) || 0,
    led_rotate: parseInt($('cfg-led-rotate').value) || 0,
    led_flip_y: $('cfg-led-flipy').checked,
    led_always_on: $('cfg-led-always-on').checked,
    led_text: $('cfg-led-text').value || '',
    box_meta_from_node: $('cfg-box-meta').checked,
    pir_enabled: $('cfg-pir-en').checked,
    pir_inverted: $('cfg-pir-inv').checked,
    pir_hold_ms: (parseInt($('cfg-pir-hold').value) || 60) * 1000,
    pir_debounce_ms: parseInt($('cfg-pir-deb').value) || 100,
    boxes: (() => {
      const arr = [];
      for (let i = 0; i < 16; i++) {
        arr.push({
          name: $('box-name-' + i).value,
          type: $('box-type-' + i).value,
          color: $('box-color-' + i).value,
          brand: $('box-brand-' + i).value,
          target_rh: parseFloat($('box-rh-' + i).value) || 30
        });
      }
      return arr;
    })()
  };
  const r = await fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  if (r.ok) alert('Zapisano. Zmiana Wi-Fi wymaga restartu.');
  else alert('Błąd zapisu: ' + (await r.text()));
}

async function testMqtt() {
  const s = await (await fetch('/api/status')).json();
  alert(s.mqtt ? 'Połączono z brokerem MQTT.' : 'Brak połączenia MQTT (sprawdź broker i ustawienia).');
}

async function downloadBackup() {
  const b = await (await fetch('/api/backup')).text();
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([b], { type: 'application/json' }));
  a.download = 'filamentmonitor-ustawienia.json';
  a.click();
}

async function reboot() {
  if (!confirm('Zrestartować urządzenie?')) return;
  await fetch('/api/reboot', { method: 'POST' });
}

async function factoryReset() {
  if (!confirm('Przywrócić ustawienia fabryczne? Stracisz Wi-Fi, MQTT i kalibrację.')) return;
  await fetch('/api/factory', { method: 'POST' });
}

// --- LED ---
async function ledTest(color) {
  await fetch('/api/led', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ color: color, ms: 5000 })
  });
}

async function ledSetFx(effect, speed, order, brightness, save) {
  await fetch('/api/led', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      effect: effect, speed: speed, order: order, brightness: brightness,
      layout: parseInt($('led-fx-layout').value) || 0,
      rotate: parseInt($('led-fx-rotate').value) || 0,
      flip_y: $('led-fx-flipy').checked,
      font: parseInt($('led-fx-font').value) || 0,
      value_color: parseInt($('led-fx-vcol').value) || 0,
      text: $('led-fx-text').value || '',
      save: !!save
    })
  });
}

function ledBriLive() {
  const b = parseInt($('led-fx-bri').value) || 127;
  fetch('/api/led', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ brightness: b, save: false })
  });
}

function ledOrientLive() {
  fetch('/api/led', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      layout: parseInt($('led-fx-layout').value) || 0,
      rotate: parseInt($('led-fx-rotate').value) || 0,
      flip_y: $('led-fx-flipy').checked,
      save: false
    })
  });
}

function ledExtraLive() {
  fetch('/api/led', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      font: parseInt($('led-fx-font').value) || 0,
      value_color: parseInt($('led-fx-vcol').value) || 0,
      text: $('led-fx-text').value || '',
      save: false
    })
  });
}

function ledTestHex() {
  const c = ($('led-test-color').value || '#00ff00').replace('#', '');
  ledTest(c);
}

function ledPreviewFx() {
  ledSetFx($('led-fx').value, parseInt($('led-fx-speed').value) || 5,
           parseInt($('led-fx-order').value) || 0,
           parseInt($('led-fx-bri').value) || 127, false);
}

async function ledSaveFx() {
  await ledSetFx($('led-fx').value, parseInt($('led-fx-speed').value) || 5,
                 parseInt($('led-fx-order').value) || 0,
                 parseInt($('led-fx-bri').value) || 127, true);
  await loadConfig();
}

// --- Logi ---
async function refreshLog() {
  $('log').textContent = await (await fetch('/api/syslog')).text();
  $('log').scrollTop = $('log').scrollHeight;
}
async function clearLog() {
  await fetch('/api/syslog', { method: 'DELETE' });
  $('log').textContent = '';
}

// --- Karta SD ---
async function refreshSd() {
  const wrap = $('sd-files');
  try {
    const j = await (await fetch('/api/sd/list')).json();
    const files = j.files || [];
    wrap.innerHTML = '';
    if (!files.length) {
      wrap.innerHTML = '<p class="hint">Brak plików CSV na karcie (pierwszy wpis pojawia się po ~60 s).</p>';
      return;
    }
    files.forEach((f) => {
      const row = document.createElement('div');
      row.className = 'sd-row';
      const nm = document.createElement('span');
      nm.className = 'sd-name';
      nm.textContent = f.name;
      const sz = document.createElement('span');
      sz.className = 'sd-size';
      sz.textContent = (f.size / 1024).toFixed(1) + ' kB';
      const dl = document.createElement('a');
      dl.className = 'btn';
      dl.textContent = 'Pobierz';
      dl.href = '/api/sd/file?name=' + encodeURIComponent(f.name);
      const rm = document.createElement('button');
      rm.className = 'btn danger';
      rm.textContent = 'Usuń';
      rm.addEventListener('click', async () => {
        if (!confirm('Usunąć ' + f.name + '?')) return;
        await fetch('/api/sd/delete?name=' + encodeURIComponent(f.name), { method: 'DELETE' });
        refreshSd();
      });
      row.append(nm, sz, dl, rm);
      wrap.appendChild(row);
    });
  } catch (e) {
    wrap.innerHTML = '<p class="hint">SD niedostępna.</p>';
  }
}

// --- Diagnostyka ---
async function refreshDiag() {
  try {
    const d = await (await fetch('/api/diag')).json();
    $('diag-fw').textContent = d.fw || '--';
    $('diag-ver').textContent = d.version || '--';
    $('diag-build').textContent = d.build || '--';
    $('diag-uptime').textContent = Math.floor((d.uptime_s || 0) / 60) + ' min ' + ((d.uptime_s || 0) % 60) + ' s';
    $('diag-reset').textContent = d.reset_reason || '--';
    $('diag-mem').textContent = ((d.free_heap || 0) / 1024).toFixed(0) + ' kB / ' + ((d.free_psram || 0) / 1024).toFixed(0) + ' kB';
    $('diag-wifi').textContent = (d.wifi_ip || '--') + ' (' + (d.wifi_rssi || 0) + ' dBm)';
    $('diag-mac').textContent = d.wifi_mac || '--';
    $('diag-mqttsd').textContent = 'MQTT ' + (d.mqtt ? 'ok' : 'off') + ' · SD ' + (d.sd ? 'ok' : 'brak');
    $('diag-ch').textContent = (d.channels_valid || 0) + ' / ' + (d.channels_total || 0);
    $('diag-fs').textContent = ((d.littlefs_used || 0) / 1024).toFixed(0) + ' kB / ' + ((d.littlefs_total || 0) / 1024).toFixed(0) + ' kB';
  } catch (e) {}
}

async function otaUpload() {
  const f = $('ota-file').files[0];
  if (!f) { alert('Najpierw wybierz plik .bin'); return; }
  if (!confirm('Wgrać ' + f.name + '? Urządzenie się zrestartuje.')) return;
  $('ota-status').textContent = 'Wgrywanie…';
  try {
    const fd = new FormData();
    fd.append('file', f);
    const r = await fetch('/api/ota', { method: 'POST', body: fd });
    $('ota-status').textContent = r.ok ? 'OK — restart…' : 'Błąd aktualizacji';
  } catch (e) {
    $('ota-status').textContent = 'Błąd aktualizacji';
  }
}

// --- Wykresy ---
function fmtHm(ts) {
  if (!ts) return '';
  const d = new Date(ts * 1000);
  return ('0' + d.getHours()).slice(-2) + ':' + ('0' + d.getMinutes()).slice(-2);
}
function fmtVal(v) {
  if (v == null || isNaN(v)) return '';
  return (Math.abs(v) >= 100) ? String(Math.round(v)) : v.toFixed(1);
}
function drawChart(canvas, series, labels) {
  const ctx = canvas.getContext('2d');
  const W = canvas.clientWidth || 560;
  const H = canvas.clientHeight || 200;
  const dpr = window.devicePixelRatio || 1;
  canvas.width = W * dpr;
  canvas.height = H * dpr;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, W, H);

  const ml = 46, mr = 10, mt = 8, mb = 20;
  const iw = W - ml - mr, ih = H - mt - mb;

  let lo = Infinity, hi = -Infinity;
  series.forEach((s) => s.values.forEach((v) => {
    if (v != null && !isNaN(v)) { if (v < lo) lo = v; if (v > hi) hi = v; }
  }));
  if (!isFinite(lo)) { lo = 0; hi = 1; }
  if (lo === hi) { lo -= 1; hi += 1; }
  const pad = (hi - lo) * 0.1 || 1; lo -= pad; hi += pad;

  ctx.font = '10px sans-serif';
  ctx.lineWidth = 1;
  for (let i = 0; i <= 3; i++) {
    const y = mt + ih * (i / 3);
    const val = hi - (hi - lo) * (i / 3);
    ctx.strokeStyle = '#1d2530';
    ctx.beginPath(); ctx.moveTo(ml, y); ctx.lineTo(W - mr, y); ctx.stroke();
    ctx.fillStyle = '#66707d';
    ctx.fillText(fmtVal(val), 2, y + 3);
  }
  const n = labels.length;
  if (n) {
    ctx.fillStyle = '#66707d';
    ctx.fillText(labels[0], ml, H - 6);
    if (n > 1) {
      ctx.fillText(labels[Math.floor(n / 2)], ml + iw / 2 - 14, H - 6);
      ctx.fillText(labels[n - 1], W - mr - 36, H - 6);
    }
  }
  series.forEach((s) => {
    ctx.strokeStyle = s.color;
    ctx.lineWidth = 1.7;
    ctx.beginPath();
    let started = false;
    s.values.forEach((v, i) => {
      if (v == null || isNaN(v)) { started = false; return; }
      const x = ml + iw * (s.values.length > 1 ? i / (s.values.length - 1) : 0);
      const y = mt + ih * (1 - (v - lo) / (hi - lo));
      if (!started) { ctx.moveTo(x, y); started = true; } else ctx.lineTo(x, y);
    });
    ctx.stroke();
  });
}
async function loadHistory() {
  try {
    const j = await (await fetch('/api/history?hours=12')).json();
    const pts = j.points || [];
    const labels = pts.map((p) => fmtHm(p.ts));
    drawChart($('chart-pm'), [
      { color: '#FFD600', values: pts.map((p) => p.pm25) },
      { color: '#FF9100', values: pts.map((p) => p.pm10) },
    ], labels);
    drawChart($('chart-th'), [
      { color: '#E53935', values: pts.map((p) => p.temp) },
      { color: '#2ecc9a', values: pts.map((p) => p.hum) },
    ], labels);
  } catch (e) {}
}

// --- Start ---
setInterval(refreshStatus, 3000);
setInterval(() => { if ($('tab-wykresy').classList.contains('active')) loadHistory(); }, 60000);
setInterval(() => { if ($('tab-logi').classList.contains('active')) refreshLog(); }, 2000);
refreshStatus();
buildBoxCfg();
loadConfig();
refreshCal();
refreshDiag();
$('cal-btn').addEventListener('click', calibrateMics);
$('cal-mq7-btn').addEventListener('click', calibrateMq7);
$('ota-btn').addEventListener('click', otaUpload);
