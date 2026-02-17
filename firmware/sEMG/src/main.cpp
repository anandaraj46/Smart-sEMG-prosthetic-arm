#include <Arduino.h>
#include <ESP32Servo.h>

// --- PINS ---
const int emgPin = 34;   // sEMG input
const int servoPin = 18; // Servo PWM output

Servo handServo;

// --- EMG VARIABLES ---
const int BASELINE = 1850; 
const float EMA_ALPHA = 0.1;
float smoothedEMG = 0;

// --- HYSTERESIS THRESHOLDS ---
const int FLEX_THRESHOLD = 650; 
const int RELAX_THRESHOLD = 250; 

// --- DEFAULT SERVO ANGLES ---
// Tweak these if the motor hums/buzzes at the limits!
const int OPEN_ANGLE = 10;   // Safe resting angle 
const int CLOSE_ANGLE = 170; // Safe flex angle 

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  
  // ESP32 Servo Timer Allocation
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  handServo.setPeriodHertz(50); // Standard 50Hz servo
  handServo.attach(servoPin, 500, 2400); 
  
  handServo.write(OPEN_ANGLE); // Start with hand open
  
  Serial.println("\n--- SYSTEM ONLINE. MUSCLE CONTROL ENGAGED ---");
}

void loop() {
  int rawValue = analogRead(emgPin);
  
  // 1. Rectify (Absolute difference from baseline)
  int rectifiedValue = abs(rawValue - BASELINE); 
  
  // 2. Smooth (Extract the envelope)
  smoothedEMG = (EMA_ALPHA * rectifiedValue) + ((1 - EMA_ALPHA) * smoothedEMG);
  
  // 3. Teleplot Telemetry
  Serial.printf(">Rectified:%d\n", rectifiedValue);
  Serial.printf(">Smoothed:%.2f\n", smoothedEMG);
  
  // 4. Actuation Logic (The State Machine)
  if (smoothedEMG > FLEX_THRESHOLD) {
    handServo.write(CLOSE_ANGLE); 
    Serial.println(">HandState:1"); // Graph the physical hand state on Teleplot
  } else if (smoothedEMG < RELAX_THRESHOLD) {
    handServo.write(OPEN_ANGLE);   
    Serial.println(">HandState:0"); 
  }
  
  delay(10); // Maintain ~100Hz control loop
}