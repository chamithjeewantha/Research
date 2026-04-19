#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "SLT-4G_1767E6";
const char* password = "cj2001AD";

const char* mqtt_server = "mqtt-dashboard.com";

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32Client")) {
      Serial.println("MQTT connected");
    } else {
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Example data
  int value = analogRead(34);

  char msg[50];
  sprintf(msg, "Sound: %d", value);

  client.publish("leak/system/data", msg);

  delay(2000);
}
