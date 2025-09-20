#include <WiFi.h>
#include <esp-mqtt-arduino.h>
#include <esp_log.h>
#include "sdkconfig.h"
#include "secrets.h"

const char* broker = "mqtts://test.mosquitto.org:8883";

constexpr bool kUseInsecureTls = false; // set true only if firmware built with CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY
constexpr bool kForceCustomCa = true;

Mqtt5ClientESP32 mqtt;

volatile bool mqttReady = false;
volatile bool mqttSubscribed = false;
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("[BOOT] Starting MQTT5 demo");

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    (void)info;
    Serial.printf("[WiFi event] id=%d\n", event);
  });

  Serial.printf("[WiFi] Connecting to %s\n", SECRET_SSID);
  WiFi.begin(SECRET_SSID, SECRET_PASSWORD);

  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[WiFi] status=%d attempt=%u\n", WiFi.status(), attempts++);
    delay(500);
  }
  Serial.print("[WiFi] Connected, IP: ");
  Serial.println(WiFi.localIP());

  Serial.printf("[MQTT] Init broker %s as %s\n", broker, "esp32-demo");
  mqtt.begin(broker, "esp32-demo");
  mqtt.setKeepAlive(45);
  bool tlsConfigured = false;
#if defined(CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY)
  if (kUseInsecureTls) {
    Serial.println("[MQTT] Warning: skipping server certificate verification");
    mqtt.setInsecure();
    tlsConfigured = true;
  }
#else
  if (kUseInsecureTls) {
    Serial.println("[MQTT] Firmware lacks CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY, falling back to CA trust");
  }
#endif
  if (!tlsConfigured) {
#if ESP_MQTT_ARDUINO_HAS_CRT_BUNDLE
    if (kForceCustomCa) {
      mqtt.setCACert(MOSQUITTO_ROOT_CA);
    } else {
      mqtt.useCrtBundle();
    }
  } else {
    mqtt.setCACert(MOSQUITTO_ROOT_CA);
#endif
  }
  mqtt.onMessage([](const char* topic, size_t topic_len, const uint8_t* data, size_t len){
    Serial.printf("[MSG] %.*s => %.*s\n", (int)topic_len, topic, (int)len, (const char*)data);
  });
  mqtt.onConnected([]{
    Serial.println("[MQTT] Connected event");
    mqttReady = true;
    Serial.println("[MQTT] Subscribing to ssl/mqtt5");
    if (mqtt.subscribe("ssl/mqtt5", 1, true)) {
      Serial.println("[MQTT] Subscribe request sent");
    } else {
      Serial.println("[MQTT] Subscribe request failed");
    }
  });
  mqtt.onDisconnected([]{
    Serial.println("[MQTT] Disconnected event");
    mqttReady = false;
  });

  Serial.println("[MQTT] Connecting...");
  if (!mqtt.connect()) {
    Serial.println("[MQTT] Connect start failed");
  }
}

void loop() {
  static unsigned long lastPublishMs = 0;
  const unsigned long now = millis();

  if (mqttReady && (now - lastPublishMs) >= 10000) {
    const char* msg = "Hello from Arduino MQTT5 ESP32!";
    Serial.println("[MQTT] Publishing demo message");
    if (mqtt.publish("ssl/mqtt5", (const uint8_t*)msg, strlen(msg))) {
      Serial.println("[MQTT] Publish queued (next in ~10s)");
    } else {
      Serial.println("[MQTT] Publish failed");
    }
    lastPublishMs = now;
  }

  delay(10);
}
