#include <Arduino.h>

#include <ESP32Servo.h>

#define NUM_SERVOS 7

int angleOffset(int i);


// ---------------- PIN ASSIGNMENT ----------------
int servoPins[NUM_SERVOS] = {
  13, // Finger 1
  12, // Finger 2
  14, // Finger 3
  27, // Finger 4
  26, // Finger 5
  25, // Wrist
  33  // Elbow
};

// ---------------- LIMITS ----------------
int minAngle[NUM_SERVOS] = {
  0, 0, 0, 0, 0,   // Fingers
  -45,             // Wrist
  0                // Elbow
};

int maxAngle[NUM_SERVOS] = {
  90, 90, 90, 90, 90, // Fingers
  45,                 // Wrist
  120                 // Elbow
};

// ---------------- OBJECTS ----------------
Servo servos[NUM_SERVOS];
int theta[NUM_SERVOS];   // control vector

// ------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Prosthetic Servo Controller Ready");

  // Attach servos
  for (int i = 0; i < NUM_SERVOS; i++) {
    servos[i].attach(servoPins[i]);
    theta[i] = 0;              // initialize position
    servos[i].write(90);       // neutral position for safety
  }

  Serial.println("Enter: <joint_index> <angle>");
  Serial.println("Example: 2 45");
}

void loop() {
  // -------- INPUT (Manual / ML later) --------
  if (Serial.available()) {

    int joint = Serial.parseInt();
    int angle = Serial.parseInt();

    if (joint >= 0 && joint < NUM_SERVOS) {

      angle = constrain(angle, minAngle[joint], maxAngle[joint]);
      theta[joint] = angle;

      Serial.print("Joint ");
      Serial.print(joint);
      Serial.print(" set to ");
      Serial.println(angle);

    } else {
      Serial.println("Invalid joint index!");
    }

    // clear buffer
    while (Serial.available()) Serial.read();
  }

  // -------- SERVO ACTIVATION LOGIC --------
  for (int i = 0; i < NUM_SERVOS; i++) {
    servos[i].write(theta[i] + angleOffset(i));
  }

  delay(20); // 50 Hz update (servo-friendly)
}

// ---------------- OFFSET HANDLING ----------------
// Some servos expect 0–180 only
int angleOffset(int i) {
  if (i == 5) return 90; // Wrist: map -45..45 → 45..135
  return 0;
}
