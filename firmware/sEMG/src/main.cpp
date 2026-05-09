#include <Arduino.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include "Adafruit_DRV2605.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ======================================================
// WIFI CONFIG
// ======================================================
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// ======================================================
// MQTT CONFIG
// ======================================================
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;

PubSubClient client(espClient);

// ======================================================
// MQTT TOPICS
// ======================================================
const char* commandTopic = "gripmate/command";
const char* emgTopic = "gripmate/emg";
const char* fsrTopic = "gripmate/fsr";
const char* gestureTopic = "gripmate/gesture";

// ======================================================
// CONFIGURATION
// ======================================================
#define NUM_FINGERS 5

// ======================================================
// PINS - SENSORS
// ======================================================
const int emgPin1 = 34;
const int emgPin2 = 35;
const int fsrPin  = 33;

// ======================================================
// PINS - SERVOS
// ======================================================
const int servoPins[NUM_FINGERS] = {
  18, 19, 23, 25, 26
};

// ======================================================
// SERVO OBJECTS
// ======================================================
Servo servos[NUM_FINGERS];

// ======================================================
// DRV2605L HAPTIC DRIVER
// ======================================================
Adafruit_DRV2605 drv;

// ======================================================
// EMG SIGNAL PROCESSING
// ======================================================
const int BASELINE = 1850;

const float EMA_ALPHA = 0.10f;

float smoothedEMG1 = 0;
float smoothedEMG2 = 0;

// ======================================================
// EMG THRESHOLDS
// ======================================================
const int FLEX_THRESHOLD   = 350;
const int POINT_THRESHOLD  = 550;
const int RELAX_THRESHOLD  = 250;

// ======================================================
// SERVO ANGLE LIMITS
// ======================================================
const int OPEN_ANGLE = 10;
const int CLOSE_MIN  = 40;
const int CLOSE_MAX  = 110;

// ======================================================
// POINT GESTURE POSITIONS
// ======================================================
const int POINT_POS[NUM_FINGERS] = {
  110,
  0,
  110,
  110,
  110
};

// ======================================================
// FSR PARAMETERS
// ======================================================
const int FSR_CONTACT     = 180;
const int FSR_SLIP_THRESH = 150;
const int FSR_MAX         = 3200;

// ======================================================
// GESTURES
// ======================================================
enum Gesture {

  GESTURE_OPEN,
  GESTURE_CLOSE,
  GESTURE_POINT
};

Gesture currentGesture = GESTURE_OPEN;

unsigned long lastGestureChange = 0;

const int GESTURE_HOLD_TIME = 250;

// ======================================================
// SERVO POSITION STATE
// ======================================================
int currentPos[NUM_FINGERS] = {
  OPEN_ANGLE,
  OPEN_ANGLE,
  OPEN_ANGLE,
  OPEN_ANGLE,
  OPEN_ANGLE
};

int targetPos[NUM_FINGERS] = {
  OPEN_ANGLE,
  OPEN_ANGLE,
  OPEN_ANGLE,
  OPEN_ANGLE,
  OPEN_ANGLE
};

// ======================================================
// FSR STATE
// ======================================================
int fsrValue = 0;

int previousFSR = 0;

bool objectDetected = false;

// ======================================================
// TIMERS
// ======================================================
unsigned long servoTimer = 0;

const int SERVO_UPDATE_INTERVAL = 8;

// ======================================================
// FUNCTION DECLARATIONS
// ======================================================
void setAllTargets(int angle);

void setPointTargets();

void hapticPoint();

// ======================================================
// WIFI FUNCTION
// ======================================================
void setupWiFi() {

  Serial.println();

  Serial.print("Connecting WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");

  Serial.println(WiFi.localIP());
}

// ======================================================
// MQTT CALLBACK
// ======================================================
void callback(char* topic,
              byte* payload,
              unsigned int length) {

  String message = "";

  for (int i = 0; i < length; i++) {

    message += (char)payload[i];
  }

  Serial.print("Received Command: ");

  Serial.println(message);

  /////////////////////////////////////////////////////////
  // OPEN
  /////////////////////////////////////////////////////////

  if (message == "OPEN") {

    for (int i = 0; i < NUM_FINGERS; i++) {

      targetPos[i] = OPEN_ANGLE;
    }

    currentGesture = GESTURE_OPEN;

    objectDetected = false;
  }

  /////////////////////////////////////////////////////////
  // CLOSE
  /////////////////////////////////////////////////////////

  else if (message == "CLOSE") {

    setAllTargets(100);

    currentGesture = GESTURE_CLOSE;
  }

  /////////////////////////////////////////////////////////
  // POINT
  /////////////////////////////////////////////////////////

  else if (message == "POINT") {

    setPointTargets();

    currentGesture = GESTURE_POINT;

    hapticPoint();
  }
}

// ======================================================
// MQTT RECONNECT
// ======================================================
void reconnectMQTT() {

  while (!client.connected()) {

    Serial.print("Connecting MQTT...");

    String clientId = "ESP32-GripMate-";

    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {

      Serial.println("connected");

      client.subscribe(commandTopic);
    }

    else {

      Serial.print("failed, rc=");

      Serial.println(client.state());

      delay(2000);
    }
  }
}

// ======================================================
// HAPTIC FUNCTIONS
// ======================================================
void vibrateRealtime(int strength) {

  strength = constrain(strength, 0, 127);

  drv.setRealtimeValue(strength);
}

void hapticTouch() {

  drv.setWaveform(0, 47);

  drv.setWaveform(1, 0);

  drv.go();
}

void hapticSlip() {

  drv.setWaveform(0, 12);

  drv.setWaveform(1, 0);

  drv.go();
}

void hapticOverpressure() {

  drv.setWaveform(0, 84);

  drv.setWaveform(1, 0);

  drv.go();
}

void hapticPoint() {

  drv.setWaveform(0, 10);

  drv.setWaveform(1, 0);

  drv.go();
}

// ======================================================
// SERVO CONTROL FUNCTIONS
// ======================================================
void setAllTargets(int angle) {

  angle = constrain(angle, CLOSE_MIN, CLOSE_MAX);

  for (int i = 0; i < NUM_FINGERS; i++) {

    targetPos[i] = angle;
  }
}

void setPointTargets() {

  for (int i = 0; i < NUM_FINGERS; i++) {

    targetPos[i] = POINT_POS[i];
  }
}

// ======================================================
// SMOOTH SERVO MOVEMENT
// ======================================================
void updateServos() {

  if (millis() - servoTimer <
      SERVO_UPDATE_INTERVAL)
    return;

  servoTimer = millis();

  for (int i = 0; i < NUM_FINGERS; i++) {

    if (currentPos[i] < targetPos[i]) {

      currentPos[i]++;
    }

    else if (currentPos[i] > targetPos[i]) {

      currentPos[i]--;
    }

    servos[i].write(currentPos[i]);
  }
}

// ======================================================
// SETUP
// ======================================================
void setup() {

  Serial.begin(115200);

  setupWiFi();

  client.setServer(mqtt_server, 1883);

  client.setCallback(callback);

  delay(1000);

  analogReadResolution(12);

  /////////////////////////////////////////////////////////
  // SERVO SETUP
  /////////////////////////////////////////////////////////

  Serial.println("Initializing Servos...");

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  for (int i = 0; i < NUM_FINGERS; i++) {

    servos[i].setPeriodHertz(50);

    servos[i].attach(
      servoPins[i],
      500,
      2400
    );

    servos[i].write(OPEN_ANGLE);

    delay(100);
  }

  Serial.println("Servos Ready.");

  /////////////////////////////////////////////////////////
  // DRV2605 SETUP
  /////////////////////////////////////////////////////////

  Serial.println(
      "Initializing Haptic Driver...");

  Wire.begin(21, 22);

  if (!drv.begin()) {

    Serial.println("DRV2605 NOT FOUND!");

    while (1);
  }

  drv.useERM();

  drv.setMode(
      DRV2605_MODE_REALTIME);

  Serial.println("Haptic Driver Ready.");

  Serial.println(
      "\n====================================");

  Serial.println(
      " EMG PROSTHETIC ARM READY ");

  Serial.println(
      "====================================\n");
}

// ======================================================
// MAIN LOOP
// ======================================================
void loop() {

  /////////////////////////////////////////////////////////
  // MQTT
  /////////////////////////////////////////////////////////

  if (!client.connected()) {

    reconnectMQTT();
  }

  client.loop();

  /////////////////////////////////////////////////////////
  // READ EMG SIGNALS
  /////////////////////////////////////////////////////////

  int raw1 = analogRead(emgPin1);

  int raw2 = analogRead(emgPin2);

  int rect1 = abs(raw1 - BASELINE);

  int rect2 = abs(raw2 - BASELINE);

  smoothedEMG1 =
      EMA_ALPHA * rect1 +
      (1.0f - EMA_ALPHA) * smoothedEMG1;

  smoothedEMG2 =
      EMA_ALPHA * rect2 +
      (1.0f - EMA_ALPHA) * smoothedEMG2;

  /////////////////////////////////////////////////////////
  // GESTURE DETECTION
  /////////////////////////////////////////////////////////

  Gesture newGesture = currentGesture;

  if (smoothedEMG2 > POINT_THRESHOLD &&
      smoothedEMG2 > smoothedEMG1 + 100) {

    newGesture = GESTURE_POINT;
  }

  else if (smoothedEMG1 > FLEX_THRESHOLD &&
           smoothedEMG1 > smoothedEMG2 + 80) {

    newGesture = GESTURE_CLOSE;
  }

  else if (smoothedEMG1 < RELAX_THRESHOLD &&
           smoothedEMG2 < RELAX_THRESHOLD) {

    newGesture = GESTURE_OPEN;
  }

  /////////////////////////////////////////////////////////
  // APPLY GESTURE
  /////////////////////////////////////////////////////////

  if (newGesture != currentGesture &&
      millis() - lastGestureChange >
          GESTURE_HOLD_TIME) {

    currentGesture = newGesture;

    lastGestureChange = millis();

    if (currentGesture ==
        GESTURE_POINT) {

      setPointTargets();

      hapticPoint();
    }

    else if (currentGesture ==
             GESTURE_OPEN) {

      for (int i = 0;
           i < NUM_FINGERS;
           i++) {

        targetPos[i] = OPEN_ANGLE;
      }

      objectDetected = false;
    }
  }

  /////////////////////////////////////////////////////////
  // CONTINUOUS GRIP
  /////////////////////////////////////////////////////////

  if (currentGesture ==
      GESTURE_CLOSE) {

    int grip = map(
      smoothedEMG1,
      FLEX_THRESHOLD,
      1800,
      CLOSE_MIN,
      CLOSE_MAX
    );

    grip = constrain(
      grip,
      CLOSE_MIN,
      CLOSE_MAX
    );

    setAllTargets(grip);
  }

  /////////////////////////////////////////////////////////
  // UPDATE SERVOS
  /////////////////////////////////////////////////////////

  updateServos();

  /////////////////////////////////////////////////////////
  // FSR READING
  /////////////////////////////////////////////////////////

  int total = 0;

  for (int i = 0; i < 10; i++) {

    total += analogRead(fsrPin);

    delayMicroseconds(300);
  }

  fsrValue = total / 10;

  /////////////////////////////////////////////////////////
  // REAL-TIME HAPTIC
  /////////////////////////////////////////////////////////

  int vibration =
      map(fsrValue,
          0,
          4095,
          0,
          127);

  vibration =
      constrain(vibration, 0, 127);

  if (vibration < 10)
    vibration = 0;

  vibrateRealtime(vibration);

  /////////////////////////////////////////////////////////
  // ADAPTIVE GRIP
  /////////////////////////////////////////////////////////

  if (currentGesture ==
      GESTURE_CLOSE) {

    if (!objectDetected &&
        fsrValue > FSR_CONTACT) {

      objectDetected = true;

      hapticTouch();

      Serial.println(
          ">> OBJECT DETECTED");
    }

    int fsrDrop =
        previousFSR - fsrValue;

    if (objectDetected &&
        fsrDrop > FSR_SLIP_THRESH) {

      for (int i = 0;
           i < NUM_FINGERS;
           i++) {

        targetPos[i] += 4;

        targetPos[i] =
            constrain(
                targetPos[i],
                CLOSE_MIN,
                CLOSE_MAX);
      }

      hapticSlip();

      Serial.println(
          ">> SLIP DETECTED");
    }

    previousFSR = fsrValue;

    if (fsrValue > FSR_MAX) {

      for (int i = 0;
           i < NUM_FINGERS;
           i++) {

        targetPos[i] -= 2;

        targetPos[i] =
            constrain(
                targetPos[i],
                CLOSE_MIN,
                CLOSE_MAX);
      }

      hapticOverpressure();

      Serial.println(
          ">> OVERPRESSURE");
    }
  }

  /////////////////////////////////////////////////////////
  // SEND MQTT DATA
  /////////////////////////////////////////////////////////

  String emgData =
      String(smoothedEMG1) + "," +
      String(smoothedEMG2);

  client.publish(
      emgTopic,
      emgData.c_str());

  String fsrData =
      String(fsrValue);

  client.publish(
      fsrTopic,
      fsrData.c_str());

  String gestureText = "OPEN";

  if (currentGesture ==
      GESTURE_CLOSE)
    gestureText = "CLOSE";

  else if (currentGesture ==
           GESTURE_POINT)
    gestureText = "POINT";

  client.publish(
      gestureTopic,
      gestureText.c_str());

  /////////////////////////////////////////////////////////
  // SERIAL DEBUG
  /////////////////////////////////////////////////////////

  Serial.printf(
    "EMG1: %.2f | EMG2: %.2f | "
    "FSR: %d | Gesture: %d\n",
    smoothedEMG1,
    smoothedEMG2,
    fsrValue,
    currentGesture
  );

  delay(10);
}