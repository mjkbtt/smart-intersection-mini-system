/*
 * Smart Intersection — ESP32 уулзварын төхөөрөмж
 *
 * Энэ хувилбарт:
 *   - Wi-Fi автомат тохиргоо ба дахин холболт (#8)      -> wifi_setup.*
 *   - MQTT холболт, LWT, heartbeat, subscribe (#9)       -> mqtt_link.*
 *   - sensor topic руу үе үе publish хийх (#9 шалгуур)
 *
 * Бусад гишүүдийн нэмэх хэсгүүд (TODO):
 *   - #4  IR мэдрэгч унших        -> readLanes()
 *   - #11 командын дагуу LED сэлгэх -> handleMessage()
 *   - #15 fallback горим           -> loop() дотор, холболтоос үл хамааран ажиллана
 *
 * Сангууд: WiFiManager (tzapu) 2.0.17+, PubSubClient (knolleary) 2.8+
 * Serial Monitor: 115200 baud.
 */
#include "config.h"
#include "mqtt_link.h"
#include "wifi_setup.h"

struct Lane {
  bool present;
  uint16_t count;
};

static const char LANE_NAMES[4] = {'N', 'E', 'S', 'W'};
static Lane lanes[4];
static uint32_t sensorSeq = 0;
static uint32_t lastSensorPublish = 0;

// TODO(#4): IR мэдрэгчээс уншиж debounce хийнэ. Одоогоор хоосон утга.
static void readLanes() {
  for (Lane &l : lanes) {
    l.present = false;
  }
}

static void publishSensor() {
  char buf[256];
  int n = snprintf(buf, sizeof(buf), "{\"device_id\":\"%s\",\"seq\":%lu,\"uptime_ms\":%lu,\"lanes\":{",
                   DEVICE_ID, (unsigned long)sensorSeq, (unsigned long)millis());
  for (int i = 0; i < 4 && n < (int)sizeof(buf); i++) {
    n += snprintf(buf + n, sizeof(buf) - n, "%s\"%c\":{\"present\":%s,\"count\":%u}", i ? "," : "",
                  LANE_NAMES[i], lanes[i].present ? "true" : "false", lanes[i].count);
  }
  if (n < (int)sizeof(buf)) snprintf(buf + n, sizeof(buf) - n, "}}");
  if (mqttlink::publish(TOPIC_SENSOR, buf)) {
    sensorSeq++;
    for (Lane &l : lanes) l.count = 0;  // count = өмнөх мессежээс хойших машин
  }
}

// command ба config topic-оос ирсэн мессеж
static void handleMessage(const char *topic, const char *payload) {
  Serial.printf("[MQTT] <- %s %s\n", topic, payload);
  if (strcmp(topic, TOPIC_COMMAND) == 0) {
    // TODO(#11): JSON задлаад (phase, duration_ms, seq) LED-ийг аюулгүй дарааллаар сэлгэнэ
  } else if (strcmp(topic, TOPIC_CONFIG) == 0) {
    // TODO(#15): yellow_ms, all_red_ms, fallback_cycle зэргийг хадгална
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== Smart Intersection %s (fw %s) ===\n", DEVICE_ID, FW_VERSION);
  wifisetup::begin();
  mqttlink::begin(wifisetup::mqttHost(), wifisetup::mqttPort(), handleMessage);
}

void loop() {
  wifisetup::loop();
  if (wifisetup::takeMqttConfigChanged()) {
    mqttlink::setServer(wifisetup::mqttHost(), wifisetup::mqttPort());
  }
  mqttlink::loop();

  readLanes();

  uint32_t now = millis();
  if (mqttlink::connected() && now - lastSensorPublish >= SENSOR_PERIOD_MS) {
    lastSensorPublish = now;
    publishSensor();
  }

  // TODO(#13, #15): гэрлэн дохионы төлөвийн машин энд ажиллана (Wi-Fi/MQTT-ээс үл хамааран)
}
