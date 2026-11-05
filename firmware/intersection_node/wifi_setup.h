#pragma once
// Wi-Fi холболтыг автоматаар тохируулах модуль — Issue [1.2.1] (#8)
//
// Ажиллах зарчим:
//  1. Асахдаа flash-д хадгалсан SSID/нууц үгээр холбогдоно.
//  2. Хадгалсан сүлжээ байхгүй эсвэл холбогдож чадахгүй бол "SmartIntersection-Setup"
//     нэртэй AP + captive portal нээнэ. Утсаараа холбогдоод Wi-Fi ба MQTT broker-ын
//     IP/портыг оруулна. Утгууд flash-д хадгалагдаж дараагийн асалтаас автоматаар ашиглагдана.
//  3. Портал non-blocking — Wi-Fi байхгүй үед ч гэрлэн дохионы логик (fallback) зогсохгүй.
//  4. Холболт тасрахад WIFI_RETRY_PERIOD_MS тутам дахин холбогдоно.
//  5. BOOT товчийг 3 секунд дарвал хадгалсан Wi-Fi тохиргоо арилж портал нээгдэнэ.
#include <Arduino.h>

namespace wifisetup {

void begin();
void loop();

bool connected();
bool portalActive();

const char *mqttHost();
uint16_t mqttPort();

// Портал дээр MQTT тохиргоо өөрчлөгдсөн бол нэг удаа true буцаана.
bool takeMqttConfigChanged();

}  // namespace wifisetup
