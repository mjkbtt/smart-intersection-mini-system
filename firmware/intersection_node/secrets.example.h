#pragma once
// Энэ файлыг "secrets.h" нэрээр хуулж утгуудаа оруулна. secrets.h нь git-д орохгүй.
// MQTT нууц үг нь broker/users.env доторх ESP32_PASS-тай ижил байна.

#define MQTT_USER          "esp32"
#define MQTT_PASS          "change-me-esp32"

// Портал дээр broker-ын хаягийг өөрчлөөгүй үед ашиглах анхны утга
#define MQTT_HOST_DEFAULT  "192.168.1.10"
#define MQTT_PORT_DEFAULT  1883

// Wi-Fi тохируулах портал (AP)-ын нууц үг, 8+ тэмдэгт
#define WIFI_PORTAL_PASS   "intersection"
