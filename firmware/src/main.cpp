#include <Arduino.h>

const int LED_PIN = 2;   // onboard LED on most ESP32 devkits

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
}