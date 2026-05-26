#include <Arduino.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include "Adafruit_DRV2605.h"

// ======================================================
// CONFIGURATION
// ======================================================
#define NUM_FINGERS 5

// ======================================================
// PINS - SENSORS
// ======================================================
const int emgPin1 = 35;   // Flexor muscle
const int emgPin2 = 34;   // Extensor muscle
const int fsrPin  = 33;   // Force sensitive resistor

// ======================================================
// PINS - SERVOS
// ======================================================
const int servoPins[NUM_FINGERS] = {18, 19, 23, 26, 27};

// ======================================================
// SERVO OBJECTS (Array-based)
// ======================================================
Servo servos[NUM_FINGERS];

// ======================================================
// DRV2605L HAPTIC DRIVER
// ======================================================
Adafruit_DRV2605 drv;

// ======================================================
// EMG SIGNAL PROCESSING
// ======================================================
const int BASELINE = 1850;              // EMG baseline (mid-point of ADC range)
const float EMA_ALPHA = 0.10f;          // Exponential moving average smoothing factor

float smoothedEMG1 = 0;                 // Smoothed flexor signal
float smoothedEMG2 = 0;                 // Smoothed extensor signal

// ======================================================
// EMG THRESHOLDS
// ======================================================
const int FLEX_THRESHOLD   = 650;       // Threshold to trigger close gesture
const int POINT_THRESHOLD  = 850;       // Threshold to trigger point gesture
const int RELAX_THRESHOLD  = 250;       // Threshold to trigger open gesture

// ======================================================
// SERVO ANGLE LIMITS
// ======================================================
const int OPEN_ANGLE   = 10;            // Fully open finger angle
const int CLOSE_MIN    = 40;            // Minimum close angle
const int CLOSE_MAX    = 170;           // Maximum close angle

// ======================================================
// POINT GESTURE POSITIONS (per finger)
// ======================================================
const int POINT_POS[NUM_FINGERS] = {
  50,    // Thumb: partially closed
  10,    // Index: fully open (pointing)
  170,   // Middle: fully closed
  170,   // Ring: fully closed
  170    // Pinky: fully closed
};

// ======================================================
// FSR (FORCE SENSOR) PARAMETERS
// ======================================================
const int FSR_CONTACT      = 180;       // Minimum FSR value to detect object contact
const int FSR_SLIP_THRESH  = 150;       // FSR drop threshold to trigger slip detection
const int FSR_MAX          = 3200;      // Maximum safe FSR value (overpressure limit)

// ======================================================
// GESTURE ENUM
// ======================================================
enum Gesture {
  GESTURE_OPEN,
  GESTURE_CLOSE,
  GESTURE_POINT
};

Gesture currentGesture = GESTURE_OPEN;

// ======================================================
// SERVO POSITION STATE
// ======================================================
int currentPos[NUM_FINGERS] = {
  OPEN_ANGLE, OPEN_ANGLE, OPEN_ANGLE, OPEN_ANGLE, OPEN_ANGLE
};

int targetPos[NUM_FINGERS] = {
  OPEN_ANGLE, OPEN_ANGLE, OPEN_ANGLE, OPEN_ANGLE, OPEN_ANGLE
};

// ======================================================
// FSR STATE
// ======================================================
int fsrValue = 0;
bool objectDetected = false;
int previousFSR = 0;

// ======================================================
// TIMERS
// ======================================================
unsigned long servoTimer = 0;
const int SERVO_UPDATE_INTERVAL = 8;   // milliseconds between servo updates

// ======================================================
// HAPTIC FEEDBACK FUNCTIONS
// ======================================================

// Real-time vibration based on force feedback
void vibrateRealtime(int strength) {
  strength = constrain(strength, 0, 127);
  drv.setRealtimeValue(strength);
}

// Haptic feedback: object touch
void hapticTouch() {
  drv.setWaveform(0, 47);  // Effect 47: touch
  drv.setWaveform(1, 0);   // Stop
  drv.go();
}

// Haptic feedback: slip detected
void hapticSlip() {
  drv.setWaveform(0, 12);  // Effect 12: slip warning
  drv.setWaveform(1, 0);
  drv.go();
}

// Haptic feedback: overpressure
void hapticOverpressure() {
  drv.setWaveform(0, 84);  // Effect 84: strong hit
  drv.setWaveform(1, 0);
  drv.go();
}

// Haptic feedback: point gesture initiated
void hapticPoint() {
  drv.setWaveform(0, 10);  // Effect 10: light pulse
  drv.setWaveform(1, 0);
  drv.go();
}

// ======================================================
// SERVO CONTROL FUNCTIONS
// ======================================================

// Set target angle for all fingers
void setAllTargets(int angle) {
  for (int i = 0; i < NUM_FINGERS; i++) {
    targetPos[i] = angle;
  }
}

// Set target angles for point gesture
void setPointTargets() {
  for (int i = 0; i < NUM_FINGERS; i++) {
    targetPos[i] = POINT_POS[i];
  }
}

// Smoothly update servo positions (1 degree per interval)
void updateServos() {
  if (millis() - servoTimer < SERVO_UPDATE_INTERVAL)
    return;

  servoTimer = millis();

  for (int i = 0; i < NUM_FINGERS; i++) {
    if (currentPos[i] < targetPos[i]) {
      currentPos[i]++;
      servos[i].write(currentPos[i]);
    }
    else if (currentPos[i] > targetPos[i]) {
      currentPos[i]--;
      servos[i].write(currentPos[i]);
    }
  }
}

// ======================================================
// SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  analogReadResolution(12);

  // ====================================================
  // SERVO SETUP
  // ====================================================
  Serial.println("Initializing servos...");

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  for (int i = 0; i < NUM_FINGERS; i++) {
    servos[i].setPeriodHertz(50);
    servos[i].attach(servoPins[i], 500, 2400);
    servos[i].write(OPEN_ANGLE);
    delay(50);
  }

  Serial.println("Servos initialized.");

  // ====================================================
  // DRV2605L HAPTIC DRIVER SETUP
  // ====================================================
  Serial.println("Initializing haptic driver...");

  Wire.begin(21, 22);  // SDA=21, SCL=22

  if (!drv.begin()) {
    Serial.println("ERROR: DRV2605 NOT FOUND!");
    while (1);
  }

  drv.useERM();                              // ERM (eccentric rotating mass) motor
  drv.setMode(DRV2605_MODE_REALTIME);        // Real-time mode for continuous feedback

  delay(500);
  Serial.println("Haptic driver initialized.");

  // ====================================================
  // STARTUP MESSAGE
  // ====================================================
  delay(500);
  Serial.println("\n============================================");
  Serial.println("  ADAPTIVE EMG PROSTHETIC ARM - READY");
  Serial.println("  5 Fingers | 3 Gestures | Slip Detection");
  Serial.println("============================================\n");
}

// ======================================================
// MAIN LOOP
// ======================================================
void loop() {

  // ====================================================
  // EMG SIGNAL ACQUISITION & PROCESSING
  // ====================================================
  int raw1 = analogRead(emgPin1);
  int raw2 = analogRead(emgPin2);

  // Full-wave rectification (absolute deviation from baseline)
  int rect1 = abs(raw1 - BASELINE);
  int rect2 = abs(raw2 - BASELINE);

  // Exponential moving average for noise reduction
  smoothedEMG1 = EMA_ALPHA * rect1 + (1.0f - EMA_ALPHA) * smoothedEMG1;
  smoothedEMG2 = EMA_ALPHA * rect2 + (1.0f - EMA_ALPHA) * smoothedEMG2;

  // ====================================================
  // GESTURE CLASSIFICATION (based on smoothed EMG)
  // ====================================================

  // POINT GESTURE: High extensor signal dominates
  if (smoothedEMG2 > POINT_THRESHOLD && smoothedEMG2 > smoothedEMG1) {
    currentGesture = GESTURE_POINT;
    setPointTargets();
  }

  // CLOSE GESTURE: High flexor signal dominates
  else if (smoothedEMG1 > FLEX_THRESHOLD && smoothedEMG1 > smoothedEMG2) {
    currentGesture = GESTURE_CLOSE;

    // Map EMG strength to grip angle (proportional grip)
    int grip = map(smoothedEMG1, FLEX_THRESHOLD, 1800, CLOSE_MIN, CLOSE_MAX);
    grip = constrain(grip, CLOSE_MIN, CLOSE_MAX);

    setAllTargets(grip);
  }

  // OPEN GESTURE: Both signals below threshold
  else if (smoothedEMG1 < RELAX_THRESHOLD && smoothedEMG2 < RELAX_THRESHOLD) {
    currentGesture = GESTURE_OPEN;
    setAllTargets(OPEN_ANGLE);
    objectDetected = false;
  }

  // ====================================================
  // SMOOTH SERVO UPDATE
  // ====================================================
  updateServos();

  // ====================================================
  // FSR READING (averaged over 10 samples)
  // ====================================================
  int total = 0;
  for (int i = 0; i < 10; i++) {
    total += analogRead(fsrPin);
    delayMicroseconds(300);
  }
  fsrValue = total / 10;

  // ====================================================
  // REAL-TIME HAPTIC FEEDBACK (proportional to FSR)
  // ====================================================
  int vibration = map(fsrValue, 0, 4095, 0, 127);
  vibration = constrain(vibration, 0, 127);

  // Filter out noise (dead zone)
  if (vibration < 10)
    vibration = 0;

  vibrateRealtime(vibration);

  // ====================================================
  // ADAPTIVE GRIP WITH SLIP DETECTION
  // ====================================================
  if (currentGesture == GESTURE_CLOSE) {

    // ================================================
    // OBJECT DETECTION
    // ================================================
    if (!objectDetected && fsrValue > FSR_CONTACT) {
      objectDetected = true;
      hapticTouch();
      Serial.println(">> OBJECT DETECTED");
    }

    // ================================================
    // SLIP DETECTION & COMPENSATION
    // ================================================
    int fsrDrop = previousFSR - fsrValue;

    if (objectDetected && fsrDrop > FSR_SLIP_THRESH) {
      // Increase grip on all fingers to prevent slip
      for (int i = 0; i < NUM_FINGERS; i++) {
        targetPos[i] += 4;
        targetPos[i] = constrain(targetPos[i], CLOSE_MIN, CLOSE_MAX);
      }

      hapticSlip();
      Serial.println(">> SLIP DETECTED - GRIP INCREASED");
    }

    previousFSR = fsrValue;

    // ================================================
    // OVERPRESSURE PROTECTION
    // ================================================
    if (fsrValue > FSR_MAX) {
      // Decrease grip on all fingers to protect object
      for (int i = 0; i < NUM_FINGERS; i++) {
        targetPos[i] -= 2;
        targetPos[i] = constrain(targetPos[i], CLOSE_MIN, CLOSE_MAX);
      }

      hapticOverpressure();
      Serial.println(">> OVERPRESSURE - GRIP REDUCED");
    }
  }

  // ====================================================
  // POINT GESTURE HAPTIC FEEDBACK
  // ====================================================
  static Gesture prevGesture = GESTURE_OPEN;

  if (currentGesture == GESTURE_POINT && prevGesture != GESTURE_POINT) {
    hapticPoint();
  }

  prevGesture = currentGesture;

  // ====================================================
  // SERIAL DEBUG OUTPUT
  // ====================================================
  Serial.printf(">EMG1:%.2f >EMG2:%.2f >FSR:%d >Gesture:%d\n",
                smoothedEMG1,
                smoothedEMG2,
                fsrValue,
                currentGesture);

  delay(10);
}