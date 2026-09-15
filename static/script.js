// ============================================================
// ESP32 Sink Cleaner - Remote Dashboard JS
// ============================================================

const FULL_LIMIT_CM = 3.0;

let currentMode = "auto";        // "auto" | "manual"
let overridePumpOn = false;      // web-side pump state in manual mode

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
};


// ------------------------------------------------------------
// Poll /api/status and render the dashboard
// ------------------------------------------------------------
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

function render(data) {
  const dist = data.distance_cm;
  const fresh = data.last_update && (Date.now() / 1000 - data.last_update < 30);

  els.conn.textContent = fresh ? "DEVICE ONLINE" : "DEVICE OFFLINE";
  els.conn.className = fresh ? "badge online" : "badge offline";

  // distance
  if (dist !== null && dist !== undefined && dist >= 0) {
    els.distVal.textContent = dist.toFixed(1);
    const pct = Math.min(100, dist / 100 * 100 / 5 * 5); // scale to 5cm=100%
    els.distBar.style.width = Math.min(100, dist / 5 * 100) + "%";
  } else {
    els.distVal.textContent = "--";
    els.distBar.style.width = "0%";
  }

  // water level (full when distance < 3cm)
  if (dist !== null && dist !== undefined && dist >= 0 && dist < FULL_LIMIT_CM) {
    els.levelState.textContent = "FULL";
    els.levelState.className = "pill pill-warn";
  } else {
    els.levelState.textContent = "NORMAL";
    els.levelState.className = "pill pill-ok";
  }

  // pump
  setPill(els.pumpBadge, "pump", data.pump_is_on ? "ON" : "OFF");
  syncPumpButton(data.pump_is_on);

  // gsm
  setPill(els.gsmBadge, "gsm", data.gsm_registered ? "ON" : "OFF");
  els.smsState.className = "pill " + (data.gsm_registered ? "pill-ok" : "pill-off");
  els.smsState.textContent = data.gsm_registered ? "READY" : "OFF";

  // last update
  els.lastUpdate.textContent = data.last_update
    ? new Date(data.last_update * 1000).toLocaleTimeString()
    : "--";

  // control mode (from server, may have been changed elsewhere)
  currentMode = data.manual_override ? "manual" : "auto";
  overridePumpOn = Boolean(data.override_pump_on);
  renderMode();

  // ack status
  if (data.revision === data.acked_revision) {
    els.ackState.textContent = "APPLIED";
    els.ackState.className = "pill pill-on";
  } else if (data.revision === 0) {
    els.ackState.textContent = "STANDBY";
    els.ackState.className = "pill pill-off";
  } else {
    els.ackState.textContent = "PENDING";
    els.ackState.className = "pill pill-busy";
  }
}

function setPill(el, kind, label) {
  let cls = "pill";
  const isOn = label === "ON";
  if (kind === "pump" || kind === "gsm") cls += isOn ? " pill-on" : " pill-off";
  el.textContent = label;
  el.className = cls;
}

function renderMode() {
  const manual = currentMode === "manual";
  els.modeAuto.className = "seg-btn" + (manual ? "" : " active");
  els.modeManual.className = "seg-btn" + (manual ? " active" : "");
  els.modeHint.textContent = manual
    ? "MANUAL: you control the relay. The ultrasonic auto-trigger is paused."
    : "AUTO: ultrasonic sensor triggers the pump when water is high.";
  syncPumpButton();
}

function syncPumpButton(actualOn) {
  // In manual mode show the overridden request; in auto show actual state.
  const on = currentMode === "manual" ? overridePumpOn : Boolean(actualOn);
  els.pumpToggle.className = "pump-btn " + (on ? "pump-on" : "pump-off");
  els.pumpToggle.textContent = on ? "TURN PUMP OFF" : "TURN PUMP ON";
}

// ------------------------------------------------------------
// Send control command to server
// ------------------------------------------------------------
async function sendControl() {
  const payload = {
    manual_override: currentMode === "manual",
    override_pump_on: overridePumpOn,
  };
  try {
    const res = await fetch("/api/control", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    if (!res.ok) throw new Error("bad status");
    renderMode();
  } catch (err) {
    showToast("Failed to send command", "error");
  }
}

// Event wiring
els.modeAuto.addEventListener("click", () => {
  if (currentMode !== "auto") { currentMode = "auto"; sendControl(); }
});
els.modeManual.addEventListener("click", () => {
  if (currentMode !== "manual") { currentMode = "manual"; sendControl(); }
});
els.pumpToggle.addEventListener("click", () => {
  overridePumpOn = !overridePumpOn;
  sendControl();
});

// ------------------------------------------------------------
// Toast helper
// ------------------------------------------------------------
function showToast(msg, type) {
  let t = document.getElementById("toast");
  if (!t) {
    t = document.createElement("div");
    t.id = "toast";
    document.body.appendChild(t);
  }
  t.textContent = msg;
  t.className = "show " + type;
  clearTimeout(t._timer);
  t._timer = setTimeout(() => (t.className = ""), 2500);
}

refresh();
setInterval(refresh, 3000);