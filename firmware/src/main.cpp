#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "secrets.h"
#include <WebServer.h>

// ---- WiFi ----
const unsigned long WIFI_TIMEOUT_MS = 15000;
bool wifiConnected = false;

// ---- Display ----
const int SCREEN_WIDTH  = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_RESET    = -1;
const int OLED_ADDRESS  = 0x3C;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---- Web server ----
WebServer server(80);

// ---- Button ----
const int BTN_PIN = 4;
const unsigned long DEBOUNCE_MS = 50;

int lastRawState = HIGH;
int stableState  = HIGH;
unsigned long lastChangeTime = 0;


// ---- Tariff ----
const float COST_PER_KWH = 0.30;   // NZD - check your own power bill

// ---- Timing ----
const unsigned long SENSOR_INTERVAL_MS  = 500;
const unsigned long DISPLAY_INTERVAL_MS = 250;
unsigned long lastSensorRead  = 0;
unsigned long lastDisplayDraw = 0;


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

// Returns true once, on the transition from released to pressed.
bool buttonPressed() {
  bool fired = false;
  int raw = digitalRead(BTN_PIN);

  if (raw != lastRawState) {          // pin moved - could be a bounce
    lastChangeTime = millis();
    lastRawState = raw;
  }

  if (millis() - lastChangeTime >= DEBOUNCE_MS) {   // held long enough to be real
    if (raw != stableState) {
      stableState = raw;
      if (stableState == LOW) fired = true;         // falling edge = press
    }
  }

  return fired;
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

  display.setCursor(104, 0);
  display.print(wifiConnected ? "WiFi" : "----");
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

bool connectWiFi() {
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WiFi failed - continuing without dashboard");
  return false;
}

void handleRoot() {
  server.send(200, "text/plain", "Smart Energy Monitor is alive");
}

void handleData() {
  const PowerReading& r = latestReading;
  char json[256];

  if (r.valid) {
    snprintf(json, sizeof(json),
      "{\"voltage\":%.1f,\"current\":%.2f,\"power\":%.1f,"
      "\"energy\":%.4f,\"frequency\":%.1f,\"powerFactor\":%.2f,"
      "\"cost\":%.2f,\"uptime\":%lu,\"valid\":true}",
      r.voltage, r.current, r.power, r.energy,
      r.frequency, r.powerFactor,
      r.energy * COST_PER_KWH, millis() / 1000);
  } else {
    snprintf(json, sizeof(json), "{\"valid\":false}");
  }

  server.send(200, "application/json", json);
}

void setup() {
  
  Serial.begin(115200);
  delay(1000);

  pinMode(BTN_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 init failed - halting");
    while (true) { }
  }
  Serial.println("fake data, button paging");

  wifiConnected = connectWiFi();

  if (wifiConnected) {
    server.on("/", handleRoot);
    server.on("/data", handleData); 
    server.begin();
    Serial.println("Web server started");
 }
  
  latestReading = getPowerReading();
}

void loop() {

  if (wifiConnected) server.handleClient();

  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    latestReading = getPowerReading();
    accumulateEnergy(latestReading.power);
  }

  // ---- Page cycling ----
 if (buttonPressed()) {
  currentPage = (currentPage + 1) % PAGE_COUNT;
  Serial.printf("Page -> %d\n", currentPage);
 }
  // ---- Display update ----
  if (now - lastDisplayDraw >= DISPLAY_INTERVAL_MS) {
    lastDisplayDraw = now;
    drawPage(currentPage, latestReading);
  }
}