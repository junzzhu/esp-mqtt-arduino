# esp-mqtt-arduino

Arduino wrapper around Espressif's official [esp-mqtt](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html) client, enabling **MQTT v5.0** features in Arduino IDE for ESP32.

## Features
- MQTT 3.1.1 and MQTT 5.0

## Installation
1. Download the latest [ZIP release](https://github.com/junzzhu/esp-mqtt-arduino/releases).
2. In Arduino IDE: `Sketch > Include Library > Add .ZIP Library`.
3. Select the downloaded zip.

## Basic Usage
The following example demonstrates the recommended event-driven approach. The client is configured with callbacks, and all logic for subscriptions and message handling resides within them, keeping the main `loop()` clean.

See [examples/BasicMqtt5/BasicMqtt5.ino](examples/BasicMqtt5/BasicMqtt5.ino) for a more detailed demo.

```cpp
#include <WiFi.h>
#include <esp-mqtt-arduino.h>

// Replace with your network credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* pass = "YOUR_WIFI_PASSWORD";

// MQTT broker
const char* broker = "mqtt://test.mosquitto.org";

Mqtt5ClientESP32 mqtt;

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Configure MQTT callbacks
  mqtt.onConnected([]{
    Serial.println("MQTT connected!");
    // Subscribe to a topic
    mqtt.subscribe("demo/topic", 1); // QoS 1
  });

  mqtt.onDisconnected([]{
    Serial.println("MQTT disconnected.");
  });

  mqtt.onMessage([](const char* topic, size_t topic_len, const uint8_t* data, size_t len){
    Serial.print("Received message on topic: ");
    Serial.write(topic, topic_len);
    Serial.print(" => ");
    Serial.write(data, len);
    Serial.println();
  });

  // Initialize and connect to MQTT broker
  mqtt.begin(broker, "esp32-client-id");
  mqtt.connect();
}

void loop() {
  // The MQTT client runs in its own task, so the main loop can be
  // used for other application logic or be kept minimal.
  delay(10);
}
```

## TLS example
If you need to talk to the encrypted, but unauthenticated, Mosquitto test broker on port 8883, start with the sketch in [`examples/BasicMqtt5_cert/BasicMqtt5_cert.ino`](examples/BasicMqtt5_cert/BasicMqtt5_cert.ino). It includes the `mosquitto.org` root certificate and optional flags for looser verification.
