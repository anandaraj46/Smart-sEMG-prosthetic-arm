#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

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
int theta[NUM_SERVOS];

// ---------------- MQTT OBJECTS ----------------
WiFiClient espClient;
PubSubClient client(espClient);

// ---------------- MQTT CALLBACK ----------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (length == 0) return;

  int command = payload[0] - '0';

  switch (command) {
    case 1:
      for (int i = 0; i < 5; i++) theta[i] = 90;
      Serial.println("CMD 1: Close fingers");
      break;

    case 2:
      for (int i = 0; i < 5; i++) theta[i] = 0;
      Serial.println("CMD 2: Open fingers");
      break;

    case 3:
      theta[5] = (theta[5] == 0) ? 45 : -45;
      Serial.println("CMD 3: Wrist toggle");
      break;
    default:
      Serial.println("Unknown command");
      break;
  }
}

// ------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Prosthetic Servo Controller Ready");

  // Attach servos
  for (int i = 0; i < NUM_SERVOS; i++) {
    servos[i].attach(servoPins[i]);
    theta[i] = 0;
    servos[i].write(90);
  }

  // -------- WIFI CONNECT --------
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");

  // -------- MQTT CONNECT --------
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(mqttCallback);

  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32_GripMate")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }

  client.subscribe(MQTT_TOPIC);
  Serial.println("Subscribed to MQTT topic");
}

void loop() {
  client.loop();

  // -------- SERVO ACTIVATION LOGIC --------
  for (int i = 0; i < NUM_SERVOS; i++) {
    servos[i].write(theta[i] + angleOffset(i));
  }

  delay(20);
}

// ---------------- OFFSET HANDLING ----------------
int angleOffset(int i) {
  if (i == 5) return 90; // Wrist offset
  return 0;
}
