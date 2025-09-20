#pragma once
#include <functional>
#include <Arduino.h>
extern "C" {
  #include "mqtt_client.h"
  #include "mqtt5_client.h"
}

#if __has_include(<esp_crt_bundle.h>)
#define ESP_MQTT_ARDUINO_HAS_CRT_BUNDLE 1
#else
#define ESP_MQTT_ARDUINO_HAS_CRT_BUNDLE 0
#endif

using MqttMsgCB = std::function<void(const char* topic, size_t topic_len, const uint8_t* data, size_t len)>;
using MqttConnCB = std::function<void()>;

struct Mqtt5KV { const char* key; const char* value; };

class Mqtt5ClientESP32 {
 public:
  Mqtt5ClientESP32();
  ~Mqtt5ClientESP32();

  void begin(const char* uri,
             const char* client_id = nullptr,
             const char* username  = nullptr,
             const char* password  = nullptr,
             bool use_v5 = true);

  bool connect();
  void disconnect();

  bool publish(const char* topic, const uint8_t* payload, size_t len,
               int qos = 0, bool retain = false,
               const Mqtt5KV* props = nullptr, uint8_t prop_count = 0);

  bool subscribe(const char* topic, int qos = 0,
                 bool no_local = false,
                 const Mqtt5KV* props = nullptr, uint8_t prop_count = 0);

  void onMessage(MqttMsgCB cb) { msg_cb_ = cb; }
  void onConnected(MqttConnCB cb) { connected_cb_ = cb; }
  void onDisconnected(MqttConnCB cb) { disconnected_cb_ = cb; }
  bool isConnected() const { return connected_; }
  void useCrtBundle(bool enable = true);
  void setCACert(const char* cert, size_t len = 0);
  void setInsecure(bool enable = true);
  void setKeepAlive(uint16_t seconds);

 private:
  static void eventHandler(void* arg, esp_event_base_t base, int32_t event_id, void* event_data);
  void handleEvent(esp_mqtt_event_handle_t e);
  static bool setUserProps(mqtt5_user_property_handle_t* handle,
                           const Mqtt5KV* props, uint8_t n);

  esp_mqtt_client_handle_t client_ = nullptr;
  esp_mqtt_client_config_t cfg_{};
  MqttMsgCB msg_cb_{nullptr};
  MqttConnCB connected_cb_{nullptr};
  MqttConnCB disconnected_cb_{nullptr};
  bool connected_{false};
  bool insecure_{false};
};
