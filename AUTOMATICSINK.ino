#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// ESP32 + Ultrasonic + Relay + SIM800L + OLED
// ============================================================

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

// Phone number to receive SMS
const char phoneNumber[] = "+260968127141";

// SMS message
const char smsMessage[] =
  "Your sink was full. Water has been drained. "
  "You need to remove the dirt.";

// SMS cooldown: 20 seconds
const unsigned long SMS_COOLDOWN = 20000;

// -------------------- Variables -----------------------
long duration;
float distance;

bool pumpIsOn = false;

bool gsmRegistered = false;

unsigned long lastSmsTime = 0;

// Used to make sure SMS is sent only when pump changes
// from OFF to ON.
bool previousPumpState = false;


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

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
  Serial.println("ESP32 Sink Water Controller");
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

  } else {

    distance = 0;
  }


  // ----------------------------------------------------------
  // Pump control
  // ----------------------------------------------------------

  if (distance > 0 && distance < 400) {

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");


    // --------------------------------------------------------
    // Water level high
    // --------------------------------------------------------

    if (distance < 3.0) {

      turnPumpOn();

      Serial.println("Pump ON");


      // ------------------------------------------------------
      // Pump has just turned ON
      // ------------------------------------------------------

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


    }

    // --------------------------------------------------------
    // Water level normal
    // --------------------------------------------------------

    else {

      turnPumpOff();

      Serial.println("Pump OFF");

      previousPumpState = false;
    }


  } else {

    // --------------------------------------------------------
    // Sensor error
    // --------------------------------------------------------

    Serial.println("Sensor error!");

    turnPumpOff();

    previousPumpState = false;
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
https://web.facebook.com/saved/?cref=28
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
// UPDATE OLED
// ============================================================

void updateOLED() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);


  // ----------------------------------------------------------
  // GSM status
  // ----------------------------------------------------------

  display.setCursor(0, 0);

  display.print("GSM: ");

  if (gsmRegistered) {

    display.println("OK");

  } else {

    display.println("OFF");
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

  display.print("Distance: ");

  if (distance > 0 && distance < 400) {

    display.print(distance, 1);
    display.println(" cm");

  } else {

    display.println("ERROR");
  }


  // ----------------------------------------------------------
  // SMS cooldown
  // ----------------------------------------------------------

  display.setCursor(0, 48);

  if (lastSmsTime != 0 &&
      millis() - lastSmsTime < SMS_COOLDOWN) {

    unsigned long remaining =
      (SMS_COOLDOWN - (millis() - lastSmsTime)) / 1000;

    display.print("SMS wait: ");
    display.print(remaining);
    display.println("s");

  } else {

    display.println("SMS: Ready");
  }


  display.display();
}