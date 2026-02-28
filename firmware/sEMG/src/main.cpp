#include "soc/rtc_cntl_reg.h"
#include <Arduino.h>
#include <ESP32Servo.h>
#include <math.h>

// ===================================================
//  PINS
// ===================================================
#define EMG_PIN    34
#define FSR_PIN    35
#define SERVO_PIN  25

// ===================================================
//  SERVO
// ===================================================
#define SERVO_OPEN       0
#define SERVO_CLOSED     130
#define SERVO_STEP_MS    12

// ===================================================
//  FSR THRESHOLDS
// ===================================================
#define FSR_CONTACT      200
#define FSR_MAX          3000
#define FSR_SLIP_MARGIN  10

// ===================================================
//  EMG
// ===================================================
#define VREF             3.3f
#define ADC_MAX          4095.0f
#define MIDPOINT         1.65f
#define WINDOW_SIZE      200

// ===================================================
//  TIMING
// ===================================================
#define CONFIRM_MS       300
#define RELEASE_MS       400
#define FSR_CHECK_MS     50

// ===================================================
//  SIGNAL PROCESSING
// ===================================================
hw_timer_t   *emgTimer = NULL;
portMUX_TYPE  timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool newSample = false;
volatile int  rawADC    = 0;

float rmsBuffer[WINDOW_SIZE] = {0};
int   rmsIndex  = 0;
float rmsValue  = 0;
float hp_in     = 0, hp_out = 0;
float lp_state  = 0;

// ===================================================
//  CALIBRATION — hardcoded from your session data
// ===================================================
float restMean  = 0.025f;
float restStd   = 0.008f;
float actMean   = 0.380f;
float threshold = 0.055f;
bool  calibDone = true;
int   calibPhase = 3;

// ===================================================
//  MUSCLE STATE
// ===================================================
bool muscleActive = false;
bool musclePrev   = false;
unsigned long muscleOnTime  = 0;
unsigned long muscleOffTime = 0;

// ===================================================
//  FSR
// ===================================================
int fsrRaw      = 0;
int fsrSmoothed = 0;
int fsrBuf[8]   = {0};
int fsrBufIdx   = 0;

// ===================================================
//  HAND STATE MACHINE
// ===================================================
enum HandState { IDLE, CLOSING, HOLDING, OPENING };
HandState handState = IDLE;

int  servoAngle     = SERVO_OPEN;
int  holdAngle      = SERVO_OPEN;
bool objectHeld     = false;
unsigned long stepTimer     = 0;
unsigned long fsrCheckTimer = 0;

// ===================================================
//  TELEPLOT
// ===================================================
unsigned long plotTimer = 0;

Servo grip;

// ===================================================
//  ISR
// ===================================================
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  rawADC    = analogRead(EMG_PIN);
  newSample = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

// ===================================================
//  FILTERS
// ===================================================
float highPass(float in) {
  const float a = 0.9747f;
  float out = a * (hp_out + in - hp_in);
  hp_in  = in;
  hp_out = out;
  return out;
}
float lowPass(float in) {
  const float a = 0.7f;
  lp_state = a * lp_state + (1.0f - a) * in;
  return lp_state;
}
float computeRMS() {
  float sum = 0;
  for (int i = 0; i < WINDOW_SIZE; i++)
    sum += rmsBuffer[i] * rmsBuffer[i];
  return sqrtf(sum / WINDOW_SIZE);
}

// ===================================================
//  PROCESS EMG
// ===================================================
void processEMG(int adc) {
  float v  = (adc / ADC_MAX) * VREF - MIDPOINT;
  float hp = highPass(v);
  float lp = lowPass(hp);
  rmsBuffer[rmsIndex] = lp;
  rmsIndex = (rmsIndex + 1) % WINDOW_SIZE;
  rmsValue = computeRMS();
}

// ===================================================
//  FSR READ
// ===================================================
void readFSR() {
  fsrBuf[fsrBufIdx] = analogRead(FSR_PIN);
  fsrBufIdx = (fsrBufIdx + 1) % 8;
  long sum = 0;
  for (int i = 0; i < 8; i++) sum += fsrBuf[i];
  fsrRaw      = fsrBuf[(fsrBufIdx - 1 + 8) % 8];
  fsrSmoothed = (int)(sum / 8);
}

// ===================================================
//  MUSCLE DEBOUNCE
// ===================================================
void updateMuscle() {
  bool raw = (rmsValue > threshold);
  if ( raw && !musclePrev) muscleOnTime  = millis();
  if (!raw &&  musclePrev) muscleOffTime = millis();
  if ( raw && millis() - muscleOnTime  >= CONFIRM_MS) muscleActive = true;
  if (!raw && millis() - muscleOffTime >= RELEASE_MS)  muscleActive = false;
  musclePrev = raw;
}

// ===================================================
//  HAND STATE MACHINE
// ===================================================
void updateHand() {
  unsigned long now = millis();

  switch (handState) {

    case IDLE:
      objectHeld = false;
      if (muscleActive) {
        handState = CLOSING;
        stepTimer = now;
        Serial.println(">> IDLE -> CLOSING");
      }
      break;

    case CLOSING:
      if (!muscleActive) {
        handState = OPENING;
        stepTimer = now;
        Serial.println(">> CLOSING -> OPENING");
        break;
      }
      if (now - stepTimer >= SERVO_STEP_MS) {
        stepTimer = now;
        readFSR();

        if (fsrSmoothed >= FSR_CONTACT) {
          holdAngle     = servoAngle;
          objectHeld    = true;
          handState     = HOLDING;
          fsrCheckTimer = now;
          Serial.printf(">> CLOSING -> HOLDING at %d deg | FSR:%d\n",
                        holdAngle, fsrSmoothed);
          break;
        }
        if (servoAngle < SERVO_CLOSED) {
          servoAngle++;
          grip.write(servoAngle);
        } else {
          holdAngle     = SERVO_CLOSED;
          objectHeld    = false;
          handState     = HOLDING;
          fsrCheckTimer = now;
          Serial.println(">> CLOSING -> HOLDING (fully closed)");
        }
      }
      break;

    case HOLDING:
      if (!muscleActive) {
        handState = OPENING;
        stepTimer = now;
        Serial.println(">> HOLDING -> OPENING");
        break;
      }
      if (now - fsrCheckTimer >= FSR_CHECK_MS) {
        fsrCheckTimer = now;
        readFSR();

        if (objectHeld && fsrSmoothed < FSR_CONTACT) {
          if (servoAngle < holdAngle + FSR_SLIP_MARGIN) {
            servoAngle++;
            grip.write(servoAngle);
            Serial.println(">> SLIP - tightening");
          }
        }
        if (fsrSmoothed >= FSR_MAX) {
          if (servoAngle > SERVO_OPEN) {
            servoAngle--;
            grip.write(servoAngle);
            Serial.println(">> OVER-PRESSURE - backing off");
          }
        }
      }
      break;

    case OPENING:
      if (now - stepTimer >= SERVO_STEP_MS) {
        stepTimer = now;
        if (servoAngle > SERVO_OPEN) {
          servoAngle--;
          grip.write(servoAngle);
        } else {
          servoAngle = SERVO_OPEN;
          objectHeld = false;
          handState  = IDLE;
          Serial.println(">> OPENING -> IDLE");
        }
      }
      break;
  }
}

// ===================================================
//  SETUP
// ===================================================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(500);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Servo
  ESP32PWM::allocateTimer(0);
  grip.setPeriodHertz(50);
  grip.attach(SERVO_PIN, 500, 2400);
  grip.write(SERVO_OPEN);
  servoAngle = SERVO_OPEN;

  // Clean muscle state
  muscleActive  = false;
  musclePrev    = false;
  muscleOnTime  = 0;
  muscleOffTime = millis();
  handState     = IDLE;

  // 1kHz EMG timer
  emgTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(emgTimer, &onTimer, true);
  timerAlarmWrite(emgTimer, 1000, true);
  timerAlarmEnable(emgTimer);

  Serial.println("=====================================");
  Serial.println("  ADAPTIVE GRIP — ESP32              ");
  Serial.println("=====================================");
  Serial.printf ("  Threshold : %.4f\n", threshold);
  Serial.printf ("  FSR contact: %d\n",  FSR_CONTACT);
  Serial.printf ("  Servo range: %d to %d deg\n", SERVO_OPEN, SERVO_CLOSED);
  Serial.println("  c=recalibrate  o=force open  t=values");
  Serial.println("=====================================\n");
  Serial.println("  Ready! Flex to close, relax to open.");
}

// ===================================================
//  LOOP
// ===================================================
void loop() {
  unsigned long now = millis();

  // 1. EMG
  if (newSample) {
    portENTER_CRITICAL(&timerMux);
    int adc   = rawADC;
    newSample = false;
    portEXIT_CRITICAL(&timerMux);
    processEMG(adc);
  }

  // 2. Muscle
  updateMuscle();

  // 3. Hand
  updateHand();

  // 4. Commands
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'o') {
      handState  = OPENING;
      objectHeld = false;
      Serial.println(">> Force open");
    }
    if (cmd == 't') {
      Serial.printf("RMS:%.4f  Thresh:%.4f  Muscle:%d  FSR:%d  Angle:%d  State:%d\n",
                    rmsValue, threshold, muscleActive, fsrSmoothed,
                    servoAngle, (int)handState);
    }
    // Manual threshold tuning
    if (cmd == '+') { threshold += 0.005f; Serial.printf("Threshold -> %.4f\n", threshold); }
    if (cmd == '-') { threshold -= 0.005f; Serial.printf("Threshold -> %.4f\n", threshold); }
  }

  // 5. TelePlot @ 50Hz
  if (now - plotTimer >= 20) {
    plotTimer = now;
    Serial.printf(">rms:%.4f\n",       rmsValue);
    Serial.printf(">threshold:%.4f\n", threshold);
    Serial.printf(">muscle:%.1f\n",    muscleActive ? 1.0f : 0.0f);
    Serial.printf(">fsrRaw:%.1f\n",    (float)fsrRaw);
    Serial.printf(">fsrSmooth:%.1f\n", (float)fsrSmoothed);
    Serial.printf(">angle:%.1f\n",     (float)servoAngle);
    Serial.printf(">state:%.1f\n",     (float)handState);
  }
}