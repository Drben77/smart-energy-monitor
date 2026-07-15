#include <Arduino.h>

const int LED_PIN = 2;
unsigned long uptimeSeconds = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  delay(1000);                    // let the USB serial connection settle
  Serial.println();
  Serial.println("Smart Energy Monitor - serial test");
  Serial.println("----------------------------------");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);

  uptimeSeconds++;
  Serial.print("Uptime: ");
  Serial.print(uptimeSeconds);
  Serial.println(" s");
}