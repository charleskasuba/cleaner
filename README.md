# ESP32 Sink Cleaner - Remote Dashboard

Automatic sink cleaner (ESP32) with a remote web dashboard + remote motor (relay)
control, deployed on Render.

## Hardware
- ESP32
- HC-SR04 ultrasonic sensor (distance senses the water level)
- Relay module driving the water pump / motor
- SIM800L GSM module (sent SMS alert when the sink is full)
- SSD1306 OLED display

## How it works
- The ESP32 keeps its **ultrasonic auto-trigger logic**: when the measured distance
  drops below `FULL_LIMIT_CM` (3.0 cm), the pump turns on automatically to drain
  the sink, and an SMS alert is sent via SIM800L.
- The ESP32 runs on WiFi in parallel and **posts live telemetry** (distance, pump
  state, GSM state) to the Flask server every few seconds.
- The web dashboard lets you **view the live parameters** remotely and **control
  the relay** from anywhere:
  - `AUTO` mode = ESP32's normal ultrasonic auto-triggering (default).
  - `MANUAL` mode = ignore the sensor; turn the pump relay ON/OFF from the website.

## Project layout
```
app.py                 - Flask backend (dashboard + REST API)
templates/index.html   - dashboard page
static/style.css       - styles
static/script.js       - dashboard logic (poll + control)
static/               - (assets)
requirements.txt       - Python deps
Procfile               - Render start command
ESP32_sketch/          - Arduino code with WiFi telemetry + remote control
```

## REST API
| Method | Endpoint        | Purpose                                        |
|--------|-----------------|------------------------------------------------|
| GET    | `/api/status`   | Current device state (distance, pump, gsm, …)  |
| POST   | `/api/telemetry`| ESP32 posts live readings                      |
| GET    | `/api/control`  | Current control block (auto/manual, pump)      |
| POST   | `/api/control`  | Set control from the web dashboard             |
| POST   | `/api/ack`      | ESP32 confirms it applied a control command    |

## Run locally
```bash
pip install -r requirements.txt
python app.py
# open http://localhost:5000
```

## Deploy on Render
1. Push this repo to GitHub.
2. In Render: **New → Web Service → connect the GitHub repo**.
3. Render auto-detects Python; the `Procfile` starts `gunicorn app:app`.
4. Note the public URL, e.g. `https://cleaner.onrender.com`.
5. Put that URL into the ESP32 sketch (`SERVER_HOST` / `SERVER_PATH`).

## ESP32 setup
1. Open `ESP32_sketch/ESP32_SinkCleaner/ESP32_SinkCleaner.ino` in the Arduino IDE.
2. Set your `WIFI_SSID` / `WIFI_PASSWORD`.
3. Set `SERVER_HOST` to your Render URL host (e.g. `cleaner.onrender.com`).
4. Upload to the ESP32 (needs: WiFi, HTTPClient, ArduinoJson, hardware libs).