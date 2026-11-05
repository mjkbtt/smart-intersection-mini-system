#include "mqtt_link.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include "config.h"
#include "secrets_include.h"

namespace mqttlink {
namespace {

WiFiClient net;
PubSubClient client(net);
MessageHandler userHandler = nullptr;

char host[64] = "";
uint16_t port = 1883;

uint32_t lastAttempt = 0;
uint32_t retryDelay = 0;
uint32_t lastHeartbeat = 0;

const uint32_t RETRY_MIN_MS = 2000;
const uint32_t RETRY_MAX_MS = 30000;
const char WILL_PAYLOAD[] = "{\"device_id\":\"" DEVICE_ID "\",\"status\":\"offline\"}";

void onMessage(char *topic, byte *payload, unsigned int length) {
  static char buf[MQTT_BUFFER_SIZE + 1];
  unsigned int n = length < MQTT_BUFFER_SIZE ? length : MQTT_BUFFER_SIZE;
  memcpy(buf, payload, n);
  buf[n] = '\0';
  if (userHandler) userHandler(topic, buf);
}

bool tryConnect() {
  Serial.printf("[MQTT] %s:%u руу холбогдож байна...\n", host, port);
  // will: QoS 1, retain — гэнэт тасрахад broker "offline"-ийг нийтэлнэ
  bool ok = client.connect(DEVICE_ID, MQTT_USER, MQTT_PASS, TOPIC_STATUS, 1, true, WILL_PAYLOAD);
  if (!ok) {
    // -4 timeout, -2 сүлжээ, 4 нэр/нууц үг буруу, 5 эрхгүй (PubSubClient.h)
    Serial.printf("[MQTT] Амжилтгүй, state=%d\n", client.state());
    return false;
  }
  Serial.println("[MQTT] Холбогдлоо");
  client.subscribe(TOPIC_COMMAND, 1);
  client.subscribe(TOPIC_CONFIG, 1);
  publishHeartbeat();
  return true;
}

}  // namespace

void begin(const char *h, uint16_t p, MessageHandler handler) {
  userHandler = handler;
  client.setBufferSize(MQTT_BUFFER_SIZE);
  client.setKeepAlive(MQTT_KEEPALIVE_S);
  client.setSocketTimeout(3);
  client.setCallback(onMessage);
  setServer(h, p);
}

void setServer(const char *h, uint16_t p) {
  strlcpy(host, h, sizeof(host));  // PubSubClient зөвхөн pointer хадгалдаг тул өөрийн buffer
  port = p;
  if (client.connected()) client.disconnect();
  client.setServer(host, port);
  retryDelay = 0;
  lastAttempt = 0;
}

void loop() {
  if (!WiFi.isConnected()) return;

  if (client.connected()) {
    client.loop();
    uint32_t now = millis();
    if (now - lastHeartbeat >= HEARTBEAT_PERIOD_MS) publishHeartbeat();
    return;
  }

  uint32_t now = millis();
  if (lastAttempt != 0 && now - lastAttempt < retryDelay) return;
  lastAttempt = now;
  if (tryConnect()) {
    retryDelay = 0;
  } else {
    retryDelay = retryDelay == 0 ? RETRY_MIN_MS : min(retryDelay * 2, RETRY_MAX_MS);
  }
}

bool connected() { return client.connected(); }

bool publish(const char *topic, const char *payload, bool retain) {
  if (!client.connected()) return false;
  return client.publish(topic, payload, retain);
}

void publishHeartbeat() {
  lastHeartbeat = millis();
  char buf[192];
  snprintf(buf, sizeof(buf),
           "{\"device_id\":\"%s\",\"status\":\"online\",\"ip\":\"%s\",\"rssi\":%d,"
           "\"uptime_ms\":%lu,\"fw\":\"%s\"}",
           DEVICE_ID, WiFi.localIP().toString().c_str(), WiFi.RSSI(), (unsigned long)millis(), FW_VERSION);
  publish(TOPIC_STATUS, buf, true);
}

}  // namespace mqttlink
