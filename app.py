import os
import threading
import time

from flask import Flask, jsonify, render_template, request

app = Flask(__name__)

# ------------------------------------------------------------------
# In-memory device state
# ------------------------------------------------------------------
state = {
    "distance_cm": None,          # last measured distance
    "pump_is_on": False,          # last reported pump state from ESP32
    "gsm_registered": False,      # last reported GSM state
    "last_update": None,          # epoch time of last telemetry

    # remote control state (set from the web dashboard)
    "manual_override": False,     # when True, web overrides auto-ultrasonic logic
    "override_pump_on": False,    # what the override state should be
    "revision": 0,                # bumped each time control changes
    "acked_revision": 0,          # last revision acknowledged by ESP32
}

LOCK = threading.Lock()


def snapshot():
    with LOCK:
        data = dict(state)
    data["server_time"] = time.time()
    return data


def control_block():
    with LOCK:
        return {
            "manual_override": state["manual_override"],
            "override_pump_on": state["override_pump_on"],
            "revision": state["revision"],
            "acked_revision": state["acked_revision"],
        }


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/status")
def api_status():
    """Dashboard polls this to render live values."""
    return jsonify(snapshot())


@app.route("/api/control")
def api_get_control():
    """Return the current control block (used by the ESP32 if needed)."""
    return jsonify(control_block())


@app.route("/api/control", methods=["POST"])
def api_set_control():
    """Dashboard user toggles remote override / pump on-off."""
    data = request.get_json(silent=True) or {}
    with LOCK:
        if "manual_override" in data:
            state["manual_override"] = bool(data["manual_override"])
        if "override_pump_on" in data:
            state["override_pump_on"] = bool(data["override_pump_on"])
        state["revision"] += 1
    return jsonify(control_block())


@app.route("/api/telemetry", methods=["POST"])
def api_telemetry():
    """ESP32 posts its live readings. Response carries pending commands."""
    data = request.get_json(silent=True) or {}
    with LOCK:
        if "distance_cm" in data and data["distance_cm"] is not None:
            state["distance_cm"] = round(float(data["distance_cm"]), 1)
        if "pump_is_on" in data:
            state["pump_is_on"] = bool(data["pump_is_on"])
        if "gsm_registered" in data:
            state["gsm_registered"] = bool(data["gsm_registered"])
        state["last_update"] = time.time()
    return jsonify(control_block())


@app.route("/api/ack", methods=["POST"])
def api_ack():
    """ESP32 confirms it applied a control revision."""
    data = request.get_json(silent=True) or {}
    with LOCK:
        if "revision" in data:
            state["acked_revision"] = int(data["revision"])
    return jsonify()


app.config["JSON_SORT_KEYS"] = False

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    app.run(host="0.0.0.0", port=port, debug=True)