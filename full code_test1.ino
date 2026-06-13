#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "SPIFFS.h"
#include <WiFiManager.h>

// WIFI
//const char* ssid = "SLT-4G_1767E6";
//const char* password = "cj2001AD";

// MQTT
const char* mqtt_server = "b41393959604430488160c8bbce4fca9.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "esp32user";
const char* mqtt_password = "Arrow2001";

// PINS
#define MIC_PIN 4
#define PIEZO_PIN 5
#define FSR_PIN 15

#define RED_PIN 10
#define GREEN_PIN 11
#define BLUE_PIN 12
#define BUZZER_PIN 13

#define ADXL345_ADDR 0x53

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

TwoWire OLEDWire = TwoWire(1);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &OLEDWire, -1);

WiFiClientSecure espClient;
PubSubClient client(espClient);

// BASELINES
float baselineMic = 0;
float baselinePiezo = 0;
float baselineVib = 0;
float baselineFSR = 0;

int leakCounter = 0;
void saveOfflineData(String payload);
void sendStoredData();

// THRESHOLDS
const float MIC_WARNING = 15.0;
const float PIEZO_WARNING = 20.0;
const float VIB_WARNING = 20.0;
const float FSR_WARNING = 15.0;

const float MIC_LEAK = 25.0;
const float PIEZO_LEAK = 15.0;
const float VIB_LEAK = 22.0;
const float FSR_LEAK = 17.0;

const int LEAK_CONFIRM_COUNT = 3;

// OLED
void showMessage(String line1, String line2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 10);
  display.println(line1);

  display.setCursor(0, 30);
  display.println(line2);

  display.display();
}

void showStatus(String status, float mic, float piezo, float vib, float fsr) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.print("WiFi:");
  display.print(WiFi.status() == WL_CONNECTED ? "OK" : "NO");

  display.setCursor(70, 0);
  display.print("MQTT:");
  display.print(client.connected() ? "OK" : "NO");

  display.setCursor(0, 14);
  display.print("STATUS:");
  display.println(status);

  display.setCursor(0, 28);
  display.print("MIC:");
  display.println((int)mic);

  display.setCursor(64, 28);
  display.print("PZ:");
  display.println((int)piezo);

  display.setCursor(0, 44);
  display.print("VB:");
  display.println(vib, 1);

  display.setCursor(64, 44);
  display.print("PR:");
  display.println((int)fsr);

  display.display();
}

// ADXL
void writeRegister(byte reg, byte value) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

int16_t read16(byte reg) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ADXL345_ADDR, 2);

  int16_t value = Wire.read() | (Wire.read() << 8);
  return value;
}

float getVibrationMagnitude() {
  int16_t x = read16(0x32);
  int16_t y = read16(0x34);
  int16_t z = read16(0x36);

  return sqrt((x * x) + (y * y) + (z * z));
}

// FILTER
float smoothAnalog(int pin) {
  long total = 0;

  for (int i = 0; i < 50; i++) {
    total += analogRead(pin);
    delay(2);
  }

  return total / 50.0;
}

// WIFI
void setup_wifi() {

  showMessage("WiFi Setup");

  WiFiManager wm;

  bool res;

  res = wm.autoConnect("LeakDetector_Setup");

  if(!res) {

    showMessage("WiFi Failed");

    ESP.restart();

  } else {

    showMessage("WiFi Connected");

  }
}

// MQTT
void reconnect() {

  while (!client.connected()) {

    showMessage("Connecting MQTT");

    if (client.connect("ESP32LeakPrototype",
                       mqtt_user,
                       mqtt_password)) {

      showMessage("MQTT Connected");

      sendStoredData();

    } else {

      delay(2000);

    }
  }
}

// RGB
void setGreen() {
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, HIGH);
  digitalWrite(BLUE_PIN, LOW);
}

void setBlue() {
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, HIGH);
}

void setRed() {
  digitalWrite(RED_PIN, HIGH);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
}

// BUZZER
void beep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(BUZZER_PIN, LOW);
}

// BASELINE
void collectBaseline() {
  setBlue();

  showMessage("Stabilizing...");
  delay(10000);

  long micSum = 0;
  long piezoSum = 0;
  float vibSum = 0;
  long fsrSum = 0;

  const int samples = 30;

  for (int i = 0; i < samples; i++) {
    showMessage("Baseline Scan", String(i + 1) + "/30");

    micSum += smoothAnalog(MIC_PIN);
    piezoSum += smoothAnalog(PIEZO_PIN);
    vibSum += getVibrationMagnitude();
    fsrSum += smoothAnalog(FSR_PIN);

    delay(300);
  }

  baselineMic = micSum / samples;
  baselinePiezo = piezoSum / samples;
  baselineVib = vibSum / samples;
  baselineFSR = fsrSum / samples;

  showMessage("Baseline Done");
  delay(1000);

  setGreen();
}
void saveOfflineData(String payload) {

  File file = SPIFFS.open("/offline.txt", FILE_APPEND);

  if (!file) {
    Serial.println("Failed to open offline file");
    return;
  }

  file.println(payload);
  file.close();

  Serial.println("Saved offline");
}

void sendStoredData() {

  if (!client.connected()) return;

  File file = SPIFFS.open("/offline.txt", FILE_READ);

  if (!file) return;

  Serial.println("Sending stored records...");

  while (file.available()) {

    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {

      client.publish("leak/monitor", line.c_str());

      Serial.println("Sent stored: " + line);

      delay(300);
    }
  }

  file.close();

  SPIFFS.remove("/offline.txt");

  Serial.println("Offline file cleared");
}
// SETUP
void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin(true)) {
  Serial.println("SPIFFS Mount Failed");
}

  analogReadResolution(12);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(9, 8);
  OLEDWire.begin(7, 6);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (1);
  }

  showMessage("BOOTING...");
  delay(1000);

  writeRegister(0x2D, 0x08);

  setup_wifi();

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);

  reconnect();

  collectBaseline();
}

// LOOP
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  float mic = smoothAnalog(MIC_PIN);
  float piezo = smoothAnalog(PIEZO_PIN);
  float vib = getVibrationMagnitude();
  float fsr = smoothAnalog(FSR_PIN);

  float micChange = abs((mic - baselineMic) / baselineMic) * 100.0;
  float piezoChange = abs((piezo - baselinePiezo) / baselinePiezo) * 100.0;
  float vibChange = abs((vib - baselineVib) / baselineVib) * 100.0;
  float fsrChange = 0;

if (baselineFSR > 200) {
  fsrChange = abs((fsr - baselineFSR) / baselineFSR) * 100.0;
}

  int warningCount = 0;
  int leakScore = 0;

  if (micChange > MIC_WARNING) warningCount++;
  if (piezoChange > PIEZO_WARNING) warningCount++;
  if (vibChange > VIB_WARNING) warningCount++;
  if (fsrChange > FSR_WARNING) warningCount++;

  if (micChange > MIC_LEAK) leakScore++;
  if (piezoChange > PIEZO_LEAK) leakScore++;
  if (vibChange > VIB_LEAK) leakScore++;
  if (fsrChange > FSR_LEAK) leakScore++;

  bool warning = (warningCount >= 2);
  bool leak = (leakScore >= 2);

  String status = "NORMAL";

  if (leak) {
    leakCounter++;
  } else {
    leakCounter = 0;
  }

  if (leakCounter >= LEAK_CONFIRM_COUNT) {
    status = "LEAK";
    setRed();
    beep();
  }
  else if (warning) {
    status = "WARNING";
    setBlue();
  }
  else {
    status = "NORMAL";
    setGreen();
  }

showStatus(status, mic, piezo, vib, fsr);

StaticJsonDocument<256> doc;

doc["mic"] = mic;
doc["piezo"] = piezo;
doc["vibration"] = vib;
doc["pressure"] = fsr;
doc["status"] = status;

char payload[256];
serializeJson(doc, payload);

if (WiFi.status() == WL_CONNECTED && client.connected()) {

  client.publish("leak/monitor", payload);

} else {

  saveOfflineData(String(payload));

}

Serial.println(payload);

delay(2000);
}