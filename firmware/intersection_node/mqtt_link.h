#pragma once
// ESP32 <-> Mosquitto холболт — Issue [1.2.2] (#9)
//  - `esp32` хэрэглэгчээр нэвтэрнэ, LWT: status = offline (retain)
//  - Холбогдмогц status = online (retain) нийтэлж, command ба config-д subscribe хийнэ
//  - Тасрахад non-blocking байдлаар 2 с -> 30 с хүртэл өсөх завсартай дахин холбогдоно
#include <Arduino.h>

namespace mqttlink {

// topic, payload (null-terminated) — command ба config мессеж ирэхэд дуудагдана
using MessageHandler = void (*)(const char *topic, const char *payload);

void begin(const char *host, uint16_t port, MessageHandler handler);
void setServer(const char *host, uint16_t port);
void loop();
bool connected();

bool publish(const char *topic, const char *payload, bool retain = false);
void publishHeartbeat();

}  // namespace mqttlink
