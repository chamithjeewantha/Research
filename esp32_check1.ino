#include <WiFi.h>

#define LED 2
#define ADC_PIN 34

const char* ssid = "SLT-4G_1767E6";
const char* password = "cj2001AD";

void setup() {
  Serial.begin(115200);

  // LED
  pinMode(LED, OUTPUT);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Blink LED
  digitalWrite(LED, HIGH);
  delay(500);
  digitalWrite(LED, LOW);
  delay(500);

  // Read ADC
  int value = analogRead(ADC_PIN);
  Serial.print("ADC: ");
  Serial.println(value);
}