#include <WiFi.h>
#include <esp-mqtt-arduino.h>
#include "secrets.h"

const char* broker = "mqtt://test.mosquitto.org:1883";
Mqtt5ClientESP32 mqtt;
volatile bool mqttReady = false;

void setup() {
  Serial.begin(115200);

  WiFi.begin(SECRET_SSID, SECRET_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  mqtt.begin(broker, "esp32-demo");

  mqtt.onMessage([](const char* topic, size_t topic_len, const uint8_t* data, size_t len){
    Serial.write(topic, topic_len);
    Serial.print(" => ");
    Serial.write(data, len);
    Serial.println();
  });

  mqtt.onConnected([]{
    mqttReady = true;
    mqtt.subscribe("demo/mqtt5", 1);
  });

  mqtt.onDisconnected([]{
    mqttReady = false;
  });

  mqtt.connect();
}

void loop() {
  static unsigned long lastPublishMs = 0;
  if (mqttReady && (millis() - lastPublishMs) >= 10000) {
    lastPublishMs = millis();
    const char* msg = "Hello from Arduino MQTT5 ESP32!";
    mqtt.publish("demo/mqtt5", (const uint8_t*)msg, strlen(msg));
  }
  delay(10);
}
