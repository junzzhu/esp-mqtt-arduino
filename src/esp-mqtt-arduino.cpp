#include "esp-mqtt-arduino.h"

#include <memory>
#include <string>
#include <cstring>

#if __has_include(<esp_crt_bundle.h>)
#include <esp_crt_bundle.h>
#define ESP_MQTT_HAS_CRT_BUNDLE 1
#else
#define ESP_MQTT_HAS_CRT_BUNDLE 0
#endif

Mqtt5ClientESP32::Mqtt5ClientESP32() { }
Mqtt5ClientESP32::~Mqtt5ClientESP32() {
  if (client_) esp_mqtt_client_destroy(client_);
}

void Mqtt5ClientESP32::begin(const char* uri, const char* client_id,
                             const char* user, const char* pass, bool use_v5) {
  connected_ = false;
  insecure_ = false;
  cfg_.broker.address.uri = uri;
  if (client_id) cfg_.credentials.client_id = client_id;
  if (user)      cfg_.credentials.username  = user;
  if (pass)      cfg_.credentials.authentication.password = pass;

  cfg_.broker.verification.use_global_ca_store = false;
  cfg_.broker.verification.certificate = nullptr;
  cfg_.broker.verification.certificate_len = 0;
  cfg_.broker.verification.skip_cert_common_name_check = false;
#if ESP_MQTT_HAS_CRT_BUNDLE
  cfg_.broker.verification.crt_bundle_attach = nullptr;
#endif
  cfg_.session.last_will.topic  = "devices/esp32/lwt";
  cfg_.session.last_will.msg    = "offline";
  cfg_.session.last_will.qos    = 1;
  cfg_.session.last_will.retain = true;

  cfg_.session.protocol_ver =
#if CONFIG_MQTT_PROTOCOL_5
      use_v5 ? MQTT_PROTOCOL_V_5 : MQTT_PROTOCOL_V_3_1_1;
#else
      MQTT_PROTOCOL_V_3_1_1;
  (void)use_v5;  // MQTT v5 support disabled at build time
#endif
}

bool Mqtt5ClientESP32::connect() {
  if (client_) { esp_mqtt_client_destroy(client_); client_ = nullptr; }
  connected_ = false;
  client_ = esp_mqtt_client_init(&cfg_);
  if (!client_) return false;

  esp_mqtt_client_register_event(client_, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                 &Mqtt5ClientESP32::eventHandler, this);
  return esp_mqtt_client_start(client_) == ESP_OK;
}

void Mqtt5ClientESP32::disconnect() {
  if (client_) esp_mqtt_client_stop(client_);
  connected_ = false;
}

bool Mqtt5ClientESP32::setUserProps(mqtt5_user_property_handle_t* handle,
                                    const Mqtt5KV* props, uint8_t n) {
  if (!props || n == 0) { *handle = nullptr; return true; }
#if CONFIG_MQTT_PROTOCOL_5
  std::unique_ptr<esp_mqtt5_user_property_item_t[]> items(new esp_mqtt5_user_property_item_t[n]);
  for (uint8_t i=0; i<n; i++) { items[i].key = props[i].key; items[i].value = props[i].value; }
  return esp_mqtt5_client_set_user_property(handle, items.get(), n) == ESP_OK;
#else
  *handle = nullptr;
  (void)props;
  (void)n;
  return false;
#endif
}

bool Mqtt5ClientESP32::publish(const char* topic, const uint8_t* payload, size_t len,
                               int qos, bool retain, const Mqtt5KV* props, uint8_t prop_count) {
  if (!client_) return false;
  mqtt5_user_property_handle_t up = nullptr;
#if CONFIG_MQTT_PROTOCOL_5
  if (setUserProps(&up, props, prop_count)) {
    esp_mqtt5_publish_property_config_t pp{};
    pp.user_property = up;
    esp_mqtt5_client_set_publish_property(client_, &pp);
  }
#else
  (void)props;
  (void)prop_count;
#endif
  int msg_id = esp_mqtt_client_publish(client_, topic, (const char*)payload, (int)len, qos, retain);
#if CONFIG_MQTT_PROTOCOL_5
  if (up) esp_mqtt5_client_delete_user_property(up);
#else
  (void)up;
#endif
  return msg_id >= 0;
}

bool Mqtt5ClientESP32::subscribe(const char* topic, int qos, bool no_local,
                                 const Mqtt5KV* props, uint8_t prop_count) {
  if (!client_) return false;
  mqtt5_user_property_handle_t up = nullptr;
#if CONFIG_MQTT_PROTOCOL_5
  esp_mqtt5_subscribe_property_config_t sp{};
  sp.no_local_flag = no_local;
  if (setUserProps(&up, props, prop_count)) sp.user_property = up;
  esp_mqtt5_client_set_subscribe_property(client_, &sp);
#else
  (void)props;
  (void)prop_count;
  (void)no_local;
#endif
  int msg_id = esp_mqtt_client_subscribe(client_, topic, qos);
#if CONFIG_MQTT_PROTOCOL_5
  if (up) esp_mqtt5_client_delete_user_property(up);
#else
  (void)up;
#endif
  return msg_id >= 0;
}

void Mqtt5ClientESP32::useCrtBundle(bool enable) {
#if ESP_MQTT_HAS_CRT_BUNDLE
  insecure_ = false;
  cfg_.broker.verification.skip_cert_common_name_check = false;
  cfg_.broker.verification.certificate = nullptr;
  cfg_.broker.verification.certificate_len = 0;
  cfg_.broker.verification.crt_bundle_attach = enable ? esp_crt_bundle_attach : nullptr;
#else
  (void)enable;
#endif
}

void Mqtt5ClientESP32::setCACert(const char* cert, size_t len) {
  insecure_ = false;
  cfg_.broker.verification.crt_bundle_attach = nullptr;
  cfg_.broker.verification.skip_cert_common_name_check = false;
  cfg_.broker.verification.certificate = cert;
  if (cert) {
    cfg_.broker.verification.certificate_len = len ? len : strlen(cert) + 1;
  } else {
    cfg_.broker.verification.certificate_len = 0;
  }
}

void Mqtt5ClientESP32::setInsecure(bool enable) {
  insecure_ = enable;
  if (enable) {
#if ESP_MQTT_HAS_CRT_BUNDLE
    cfg_.broker.verification.crt_bundle_attach = nullptr;
#endif
    cfg_.broker.verification.certificate = nullptr;
    cfg_.broker.verification.certificate_len = 0;
    cfg_.broker.verification.use_global_ca_store = false;
    cfg_.broker.verification.skip_cert_common_name_check = true;
  } else {
    cfg_.broker.verification.skip_cert_common_name_check = false;
  }
}

void Mqtt5ClientESP32::setKeepAlive(uint16_t seconds) {
  cfg_.session.keepalive = seconds;
}

void Mqtt5ClientESP32::eventHandler(void* arg, esp_event_base_t base, int32_t id, void* data) {
  static_cast<Mqtt5ClientESP32*>(arg)->handleEvent((esp_mqtt_event_handle_t)data);
}

void Mqtt5ClientESP32::handleEvent(esp_mqtt_event_handle_t e) {
  switch (e->event_id) {
    case MQTT_EVENT_CONNECTED:
      connected_ = true;
      if (connected_cb_) connected_cb_();
      break;
    case MQTT_EVENT_DISCONNECTED:
      connected_ = false;
      if (disconnected_cb_) disconnected_cb_();
      break;
    case MQTT_EVENT_DATA:
      if (msg_cb_) msg_cb_(e->topic, (size_t)e->topic_len,
                           (const uint8_t*)e->data, (size_t)e->data_len);
      break;
    default: break;
  }
}
