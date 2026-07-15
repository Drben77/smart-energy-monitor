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
const unsigned long DISPLAY_INTERVAL_MS = 250;
const unsigned long PAGE_CYCLE_MS       = 3000;   // temporary - button replaces this
unsigned long lastSensorRead  = 0;
unsigned long lastDisplayDraw = 0;
unsigned long lastPageChange  = 0;

// ---- Pages ----
enum Page {
  PAGE_POWER = 0,
  PAGE_ENERGY,
  PAGE_COST,
  PAGE_DETAIL,
  PAGE_COUNT
};
int currentPage = PAGE_POWER;

// ---- Sensor interface ----
struct PowerReading {
  float voltage;
  float current;
  float power;
  float energy;
  float frequency;
  float powerFactor;
  bool  valid;
};

PowerReading latestReading;
float accumulatedEnergyKwh = 0.0;
unsigned long lastEnergyTick = 0;

PowerReading getPowerReading() {
  PowerReading r;
  r.voltage     = 230.0 + random(-30, 31) / 10.0;
  r.current     = 8.0 + random(-50, 51) / 100.0;
  r.powerFactor = 0.98;
  r.power       = r.voltage * r.current * r.powerFactor;
  r.frequency   = 50.0;
  r.energy      = accumulatedEnergyKwh;
  r.valid       = true;
  return r;
}

void accumulateEnergy(float watts) {
  unsigned long now = millis();
  if (lastEnergyTick == 0) { lastEnergyTick = now; return; }
  unsigned long elapsedMs = now - lastEnergyTick;
  lastEnergyTick = now;
  accumulatedEnergyKwh += (watts * elapsedMs) / 3600000000.0;
}

// ---- Helpers ----
void formatElapsed(char* buf, size_t bufLen, unsigned long ms) {
  unsigned long totalSec = ms / 1000;
  snprintf(buf, bufLen, "%02lu:%02lu:%02lu",
           totalSec / 3600, (totalSec % 3600) / 60, totalSec % 60);
}

void drawHeader(const char* label) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(label);
}

void drawFooter(const char* text) {
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(text);
}

// ---- Pages ----
void drawPowerPage(const PowerReading& r) {
  drawHeader("POWER");
  display.setTextSize(3);
  display.setCursor(0, 20);
  display.print(r.power, 0);
  display.setTextSize(2);
  display.setCursor(display.getCursorX() + 4, 27);
  display.print("W");

  char buf[24];
  snprintf(buf, sizeof(buf), "%.1fV  %.2fA", r.voltage, r.current);
  drawFooter(buf);
}

void drawEnergyPage(const PowerReading& r) {
  drawHeader("ENERGY");
  display.setTextSize(3);
  display.setCursor(0, 20);
  display.print(r.energy, 3);
  display.setTextSize(1);
  display.setCursor(display.getCursorX() + 4, 34);
  display.print("kWh");

  char buf[24];
  char t[12];
  formatElapsed(t, sizeof(t), millis());
  snprintf(buf, sizeof(buf), "elapsed %s", t);
  drawFooter(buf);
}

void drawCostPage(const PowerReading& r) {
  drawHeader("COST");
  display.setTextSize(3);
  display.setCursor(0, 20);
  display.print("$");
  display.print(r.energy * COST_PER_KWH, 2);

  char buf[24];
  snprintf(buf, sizeof(buf), "@ $%.2f/kWh", COST_PER_KWH);
  drawFooter(buf);
}

void drawDetailPage(const PowerReading& r) {
  drawHeader("DETAIL");
  display.setTextSize(1);
  display.setCursor(0, 16);
  display.printf("Voltage  %6.1f V\n", r.voltage);
  display.printf("Current  %6.2f A\n", r.current);
  display.printf("Power    %6.0f W\n", r.power);
  display.printf("PF       %6.2f\n",   r.powerFactor);
  display.printf("Freq     %6.1f Hz",  r.frequency);
}

void drawPage(int page, const PowerReading& r) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!r.valid) {
    display.setTextSize(2);
    display.setCursor(0, 24);
    display.print("NO DATA");
    display.display();
    return;
  }

  switch (page) {
    case PAGE_POWER:  drawPowerPage(r);  break;
    case PAGE_ENERGY: drawEnergyPage(r); break;
    case PAGE_COST:   drawCostPage(r);   break;
    case PAGE_DETAIL: drawDetailPage(r); break;
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 init failed - halting");
    while (true) { }
  }
  Serial.println("Smart Energy Monitor - fake data, auto-cycling pages");

  latestReading = getPowerReading();
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    latestReading = getPowerReading();
    accumulateEnergy(latestReading.power);
  }

  // Temporary: auto-cycle. The button will replace this trigger.
  if (now - lastPageChange >= PAGE_CYCLE_MS) {
    lastPageChange = now;
    currentPage = (currentPage + 1) % PAGE_COUNT;
  }

  if (now - lastDisplayDraw >= DISPLAY_INTERVAL_MS) {
    lastDisplayDraw = now;
    drawPage(currentPage, latestReading);
  }
}