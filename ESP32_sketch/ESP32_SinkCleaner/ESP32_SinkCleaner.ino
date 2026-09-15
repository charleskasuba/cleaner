#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
WiFiClientSecure httpsClient;
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// ESP32 + Ultrasonic + Relay + SIM800L + OLED + WiFi Remote
// ============================================================

// -------------------- WiFi --------------------
const char *WIFI_SSID = "HASSON ESP";
const char *WIFI_PASSWORD = "SWAT2772";

// Render app host (replace with your deployed URL, e.g. cleaner.onrender.com)
const char *SERVER_HOST = "cleaner-zjto.onrender.com";
const int  SERVER_PORT  = 443;          // Render serves HTTPS (uses TLS)
const char *SERVER_PATH = "/api/telemetry";
const char *ACK_PATH    = "/api/ack";

// -------------------- Ultrasonic --------------------
#define TRIG_PIN 5
#define ECHO_PIN 18

// -------------------- Relay ---------------------------
#define RELAY_PIN 19

// -------------------- SIM800L -------------------------
#define GSM_RX_PIN 16   // ESP32 RX <- SIM800L TX
#define GSM_TX_PIN 17   // ESP32 TX -> SIM800L RX

// -------------------- OLED ----------------------------
#define OLED_SDA 21
#define OLED_SCL 23

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// GSM serial
HardwareSerial sim800(2);

// -------------------- Settings ------------------------
bool isRelayActiveLow = false;

// Distance below this (cm) = water level full -> auto pump ON
const float FULL_LIMIT_CM = 3.0;
const int   SENSOR_MAX_CM = 400;

// Phone number to receive SMS
const char phoneNumber[] = "+260968127141";

// SMS message
const char smsMessage[] =
  "Your sink was full. Water has been drained. "
  "You need to remove the dirt.";

// SMS cooldown: 20 seconds
const unsigned long SMS_COOLDOWN = 20000;

// Telemetry push interval
const unsigned long TELEMETRY_INTERVAL = 5000;

// -------------------- Variables -----------------------
long duration;
float distance;

bool pumpIsOn = false;

bool gsmRegistered = false;

unsigned long lastSmsTime = 0;

// Used to make sure SMS is sent only when pump changes
// from OFF to ON.
bool previousPumpState = false;

// -------------------- Remote control ------------------
// Set by the web dashboard.
bool remoteManualOverride = false;   // true = remote controls relay
bool remotePumpOn = false;           // desired relay state in manual mode
long remoteRevision = 0;             // bumped each time the dashboard changes
long appliedRevision = 0;            // last revision we applied

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  // ----------------------------------------------------------
  // WiFi
  // ----------------------------------------------------------

  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi connected! IP: ");
    Serial.println(WiFi.localIP());
    httpsClient.setInsecure();   // skip cert validation for Render HTTPS
  } else {
    Serial.println("\nWiFi FAILED - will keep retrying in loop");
  }


  // ----------------------------------------------------------
  // Ultrasonic
  // ----------------------------------------------------------

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);


  // ----------------------------------------------------------
  // Relay
  // ----------------------------------------------------------

  pinMode(RELAY_PIN, OUTPUT);

  // Start with pump OFF
  turnPumpOff();


  // ----------------------------------------------------------
  // OLED
  // ----------------------------------------------------------

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {

    Serial.println("OLED not found!");

  } else {

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println("System Starting...");

    display.display();

    delay(1000);
  }


  // ----------------------------------------------------------
  // SIM800L
  // ----------------------------------------------------------

  sim800.begin(9600, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);

  Serial.println("Starting SIM800L...");

  delay(3000);

  // Test GSM
  sim800.println("AT");

  delay(1000);

  // SMS text mode
  sim800.println("AT+CMGF=1");

  delay(1000);

  Serial.println("=====================================");
  Serial.println("ESP32 Sink Water Controller + WiFi");
  Serial.println("=====================================");
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

  // ----------------------------------------------------------
  // Check GSM registration
  // ----------------------------------------------------------

  checkGSMRegistration();


  // ----------------------------------------------------------
  // Measure distance
  // ----------------------------------------------------------

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);


  // Wait for echo
  duration = pulseIn(ECHO_PIN, HIGH, 30000);


  // Calculate distance
  if (duration > 0) {

    distance = duration * 0.034 / 2;

    if (distance > SENSOR_MAX_CM) distance = 0;   // out of range

  } else {

    distance = 0;
  }


  // ----------------------------------------------------------
  // Pump control
  // ----------------------------------------------------------

  if (distance > 0 && distance < SENSOR_MAX_CM) {

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    if (remoteManualOverride) {

      // ------------------------------------------------
      // REMOTE MANUAL MODE - web dashboard drives relay
      // ------------------------------------------------

      if (remotePumpOn) {
        turnPumpOn();
        Serial.println("Pump ON (remote manual)");
      } else {
        turnPumpOff();
        Serial.println("Pump OFF (remote manual)");
      }

      previousPumpState = pumpIsOn;

    } else {

      // ------------------------------------------------
      // AUTO MODE - ultrasonic water-level logic (kept)
      // ------------------------------------------------

      if (distance < FULL_LIMIT_CM) {

        // ----------------------------------------------
        // Water level high
        // ----------------------------------------------

        turnPumpOn();

        Serial.println("Pump ON");


        // ----------------------------------------------
        // Pump has just turned ON
        // ----------------------------------------------

        if (!previousPumpState) {

          Serial.println("Pump changed from OFF to ON");


          // Send SMS only if GSM is registered
          if (gsmRegistered) {

            sendSMS();

          } else {

            Serial.println("SMS not sent - GSM not registered");
          }
        }


        previousPumpState = true;


      } else {

        // ----------------------------------------------
        // Water level normal
        // ----------------------------------------------

        turnPumpOff();

        Serial.println("Pump OFF");

        previousPumpState = false;
      }

    }

  } else {

    // --------------------------------------------------------
    // Sensor error
    // --------------------------------------------------------

    Serial.println("Sensor error!");

    // In sensor error, if remote manual says ON keep it,
    // otherwise force pump off for safety.
    if (remoteManualOverride && remotePumpOn) {
      turnPumpOn();
    } else {
      turnPumpOff();
    }

    previousPumpState = pumpIsOn;
  }


  // ----------------------------------------------------------
  // Push telemetry to server (WiFi)
  // ----------------------------------------------------------

  static unsigned long lastTelemetryTime = 0;

  if (millis() - lastTelemetryTime >= TELEMETRY_INTERVAL) {
    lastTelemetryTime = millis();
    ensureWifiConnected();
    sendTelemetry();
  }


  // ----------------------------------------------------------
  // Update OLED
  // ----------------------------------------------------------

  updateOLED();


  delay(500);
}


// ============================================================
// TURN PUMP ON
// ============================================================

void turnPumpOn() {

  if (isRelayActiveLow) {

    digitalWrite(RELAY_PIN, LOW);

  } else {

    digitalWrite(RELAY_PIN, HIGH);
  }

  pumpIsOn = true;
}


// ============================================================
// TURN PUMP OFF
// ============================================================

void turnPumpOff() {

  if (isRelayActiveLow) {

    digitalWrite(RELAY_PIN, HIGH);

  } else {

    digitalWrite(RELAY_PIN, LOW);
  }

  pumpIsOn = false;
}


// ============================================================
// CHECK GSM REGISTRATION
// ============================================================

void checkGSMRegistration() {

  // Clear old GSM data
  while (sim800.available()) {
    sim800.read();
  }

  // Ask SIM800L for network registration
  sim800.println("AT+CREG?");

  unsigned long startTime = millis();

  String response = "";

  while (millis() - startTime < 1000) {

    while (sim800.available()) {

      char c = sim800.read();

      response += c;
    }
  }


  Serial.print("GSM Response: ");
  Serial.println(response);


  // Registered on home network:
  // +CREG: 0,1
  //
  // Registered while roaming:
  // +CREG: 0,5

  if (response.indexOf("+CREG: 0,1") >= 0 ||
      response.indexOf("+CREG: 0,5") >= 0) {

    gsmRegistered = true;

  } else {

    gsmRegistered = false;
  }
}


// ============================================================
// SEND SMS
// ============================================================

void sendSMS() {

  // ----------------------------------------------------------
  // Check 20 second cooldown
  // ----------------------------------------------------------

  if (lastSmsTime != 0 &&
      millis() - lastSmsTime < SMS_COOLDOWN) {

    Serial.println("SMS cooldown active.");

    return;
  }


  Serial.println("Sending SMS...");


  // Set SMS text mode
  sim800.println("AT+CMGF=1");

  delay(500);


  // Set phone number
  sim800.print("AT+CMGS=\"");
  sim800.print(phoneNumber);
  sim800.println("\"");

  delay(1000);


  // Send message
  sim800.print(smsMessage);

  delay(500);


  // CTRL+Z
  sim800.write(26);

  Serial.println("SMS command sent.");


  // Give SIM800L time to send
  delay(5000);


  // Start 20 second cooldown
  lastSmsTime = millis();


  // Print SIM800 response
  while (sim800.available()) {

    Serial.write(sim800.read());
  }
}


// ============================================================
// WIFI RECONNECT (non-blocking retry)
// ============================================================

void ensureWifiConnected() {

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}


// ============================================================
// SEND TELEMETRY (WiFi)
// Posts live readings to the server.
// The response carries the latest remote control command.
// ============================================================

void sendTelemetry() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - skipping telemetry");
    return;
  }

  HTTPClient http;

  String url = String("https://") + SERVER_HOST + SERVER_PATH;

  http.begin(httpsClient, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);

  // Build JSON payload
  String payload = "{";
  payload += "\"distance_cm\":";
  payload += String(distance, 1);              // e.g. 12.3
  payload += ",\"pump_is_on\":";
  payload += pumpIsOn ? "true" : "false";
  payload += ",\"gsm_registered\":";
  payload += gsmRegistered ? "true" : "false";
  payload += "}";

  int httpCode = http.POST(payload);

  Serial.print("Telemetry POST -> HTTP ");
  Serial.println(httpCode);

  if (httpCode > 0) {

    String body = http.getString();

    // Parse remote control fields from the JSON response
    remoteManualOverride = (body.indexOf("\"manual_override\":true") >= 0);
    remotePumpOn         = (body.indexOf("\"override_pump_on\":true") >= 0);

    // Extract revision number
    int revIdx = body.indexOf("\"revision\":");
    if (revIdx >= 0) {
      remoteRevision = body.substring(revIdx + 11).toInt();
    }

    Serial.print("Remote mode: ");
    Serial.println(remoteManualOverride ? "MANUAL" : "AUTO");
    Serial.print("Remote pump: ");
    Serial.println(remotePumpOn ? "ON" : "OFF");
    Serial.print("Revision: ");
    Serial.println(remoteRevision);

    // Acknowledge if there is a new command to apply
    if (remoteRevision > appliedRevision) {
      ackCommand(remoteRevision);
      appliedRevision = remoteRevision;
    }

  } else {

    Serial.print("Telemetry POST failed, error: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}


// ============================================================
// ACK COMMAND
// Tells the server we have applied the control revision.
// ============================================================

void ackCommand(long revision) {

  HTTPClient http;

  String url = String("https://") + SERVER_HOST + ACK_PATH;

  http.begin(httpsClient, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);

  String payload = "{";
  payload += "\"revision\":";
  payload += revision;
  payload += "}";

  int httpCode = http.POST(payload);

  Serial.print("ACK POST -> HTTP ");
  Serial.println(httpCode);

  http.end();
}


// ============================================================
// UPDATE OLED
// ============================================================

void updateOLED() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);


  // ----------------------------------------------------------
  // Mode (AUTO / MANUAL)
  // ----------------------------------------------------------

  display.setCursor(0, 0);

  display.print("Mode: ");

  if (remoteManualOverride) {
    display.println("REMOTE");
  } else {
    display.println("AUTO");
  }


  // ----------------------------------------------------------
  // Pump status
  // ----------------------------------------------------------

  display.setCursor(0, 16);

  display.print("Pump: ");

  if (pumpIsOn) {

    display.println("ON");

  } else {

    display.println("OFF");
  }


  // ----------------------------------------------------------
  // Distance
  // ----------------------------------------------------------

  display.setCursor(0, 32);

  display.print("Dist: ");

  if (distance > 0 && distance < SENSOR_MAX_CM) {

    display.print(distance, 1);
    display.println(" cm");

  } else {

    display.println("ERROR");
  }


  // ----------------------------------------------------------
  // WiFi + GSM status
  // ----------------------------------------------------------

  display.setCursor(0, 48);

  display.print(WiFi.status() == WL_CONNECTED ? "WiFi:OK " : "WiFi:NO ");
  display.print("GSM:");
  display.println(gsmRegistered ? "OK" : "OFF");


  display.display();
}