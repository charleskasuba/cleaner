// ESP32 Sink Cleaner - Remote Dashboard JS
const FULL_LIMIT_CM = 3.0;
let currentMode = "auto";
let overridePumpOn = false     ;
const els = {
  conn: document.getElementById("conn-status"),
  distVal: document.getElementById("dist-val"),
  distBar: document.getElementById("dist-bar"),
  levelState: document.getElementById("level-state"),
  pumpBadge: document.getElementById("pump-badge"),
  gsmBadge: document.getElementById("gsm-badge"),
  smsState: document.getElementById("sms-state"),
  lastUpdate: document.getElementById("last-update"),
  pumpToggle: document.getElementById("pump-toggle"),
  modeAuto: document.getElementById("mode-auto"),
  modeManual: document.getElementById("mode-manual"),
  modeHint: document.getElementById("mode-hint"),
  ackState: document.getElementById("ack-state"),
  cyclesChart: document.getElementById("cycles-chart"),
  cyclesToday: document.getElementById("cycles-today"),
  cyclesTotal: document.getElementById("cycles-total"),
  notifList: document.getElementById("notif-list"),
  notifClear: document.getElementById("notif-clear"),
};
const pillOnClasses = "pill pill-on";
const pillOffClasses = "pill pill-off";
let lastPumpOn = null;
let lastLevelFull = null;
let lastOnline = null;
let notifCount = 0;
function addNotif(text, cls) {
  const li = document.createElement("li");
  li.className = "notif-item";
  li.innerHTML = '<span class="notif-dot ' + cls + '"></span>' + text;
  els.notifList.prepend(li);
  notifCount++;
  if (notifCount > 8) {
    const last = els.notifList.lastElementChild;
    if (last) last.remove();
  }
}
function render(data) {
  const dist = Number(data.distance_cm);
  const fresh = data.last_update && (Date.now() / 1000 - data.last_update) < 30;
  const online = !!fresh;
  if (lastOnline !== online) {
    lastOnline = online;
    if (online) addNotif("DEVICE ONLINE", "success");
    else addNotif("DEVICE OFFLINE", "error");
  }
  els.conn.textContent = online ? "DEVICE ONLINE" : "DEVICE OFFLINE";
  els.conn.className = online ? "badge online" : "badge offline";
  if (isFinite(dist)) {
    els.distVal.textContent = dist.toFixed(1) + " cm";
    els.distBar.style.width = Math.min(100, dist / 5 * 100) + "%";
    const full = dist <= FULL_LIMIT_CM;
    if (lastLevelFull !== full) {
      lastLevelFull = full;
      if (full) addNotif("WATER FULL - pump draining", "warn");
      else addNotif("Water restored to NORMAL", "info");
    }
    els.levelState.textContent = full ? "FULL" : "NORMAL";
    els.levelState.className = full ? "pill pill-warn" : "pill pill-ok";
  }
  els.pumpBadge.textContent = data.pump_on ? "ON" : "OFF";
  els.pumpBadge.className = data.pump_on ? "pill pill-on" : "pill pill-off";
  els.gsmBadge.textContent = data.gsm_online ? "ON" : "OFF";
  els.gsmBadge.className = data.gsm_online ? "pill pill-on" : "pill pill-off";
  els.smsState.textContent = data.sms_sent ? "LAST SENT: YES" : "READY";
  els.lastUpdate.textContent = data.last_update
    ? new Date(data.last_update * 1000).toLocaleTimeString()
    : "--";
}
async function refresh() {
  try {
    const res = await fetch("/api/status");
    const data = await res.json();
    render(data);
  } catch (err) {
    els.conn.textContent = "SERVER OFFLINE";
    els.conn.className = "badge offline";
  }
}
els.pumpToggle.addEventListener("click", () => {
  const on = els.pumpToggle.classList.contains("pump-on");
  els.pumpToggle.classList.toggle("pump-on", !on);
  fetch("/api/control", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ mode: "manual", override_pump_on: !on }),
  }).then(() => {
    els.ackState.textContent = "SENT";
    els.ackState.className = "pill pill-ok";
  });
});
els.modeAuto.addEventListener("click", () => {
  currentMode = "auto";
  els.modeAuto.classList.add("active");
  els.modeManual.classList.remove("active");
  els.modeHint.textContent = "AUTO: sensor triggers the pump automatically.";
});
els.modeManual.addEventListener("click", () => {
  currentMode = "manual";
  els.modeManual.classList.add("active");
  els.modeAuto.classList.remove("active");
  els.modeHint.textContent = "MANUAL: use the web button to run the pump.";
});
els.notifClear.addEventListener("click", () => {
  els.notifList.innerHTML = '<li class="notif-item muted">NO EVENTS YET</li>';
  notifCount = 0;
});
refresh();
setInterval(refresh, 3000);
