#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---- Display ----
const int SCREEN_WIDTH  = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_RESET    = -1;
const int OLED_ADDRESS  = 0x3C;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---- Tariff ----
const float COST_PER_KWH = 0.30;   // NZD - check your own power bill

// ---- Timing ----
const unsigned long SENSOR_INTERVAL_MS  = 500;
const unsigned long DISPLAY_INTERVAL_MS = 500;
unsigned long lastSensorRead   = 0;
unsigned long lastDisplayDraw  = 0;

// ---- Sensor interface ----
struct PowerReading {
  float voltage;      // V RMS
  float current;      // A RMS
  float power;        // W real power
  float energy;       // kWh accumulated
  float frequency;    // Hz
  float powerFactor;
  bool  valid;        // false = sensor not responding
};

PowerReading latestReading;
float accumulatedEnergyKwh = 0.0;
unsigned long lastEnergyTick = 0;

// Fake sensor - replaced by the PZEM later.
PowerReading getPowerReading() {
  PowerReading r;
  r.voltage     = 230.0 + random(-30, 31) / 10.0;    // 227.0 - 233.0 V
  r.current     = 8.0 + random(-50, 51) / 100.0;     // ~8 A, kettle-ish
  r.powerFactor = 0.98;
  r.power       = r.voltage * r.current * r.powerFactor;
  r.frequency   = 50.0;
  r.energy      = accumulatedEnergyKwh;
  r.valid       = true;
  return r;
}

// Numerically integrate power over time to get energy.
void accumulateEnergy(float watts) {
  unsigned long now = millis();
  if (lastEnergyTick == 0) { lastEnergyTick = now; return; }
  unsigned long elapsedMs = now - lastEnergyTick;
  lastEnergyTick = now;
  accumulatedEnergyKwh += (watts * elapsedMs) / 3600000000.0;
}

// ---- Display ----
void drawPowerPage(const PowerReading& r) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!r.valid) {
    display.setTextSize(2);
    display.setCursor(0, 24);
    display.print("NO DATA");
    display.display();
    return;
  }

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("POWER");

  display.setTextSize(3);
  display.setCursor(0, 16);
  display.print(r.power, 0);
  display.setTextSize(2);
  display.setCursor(display.getCursorX(), 24);
  display.print("W");

  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print(r.voltage, 1);
  display.print("V   ");
  display.print(r.current, 2);
  display.print("A");

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 init failed - halting");
    while (true) { }
  }
  Serial.println("Smart Energy Monitor - fake data mode");

  latestReading = getPowerReading();
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    latestReading = getPowerReading();
    accumulateEnergy(latestReading.power);
  }

  if (now - lastDisplayDraw >= DISPLAY_INTERVAL_MS) {
    lastDisplayDraw = now;
    drawPowerPage(latestReading);
  }
}