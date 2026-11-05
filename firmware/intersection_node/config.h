#pragma once
// Уулзварын төхөөрөмжийн тохиргоо. Topic/payload: docs/interfaces.md

#define FW_VERSION      "0.2.0"
#define INTERSECTION_ID "int01"
#define DEVICE_ID       "esp32-" INTERSECTION_ID

// --- MQTT topic (docs/interfaces.md §2) ---
#define TOPIC_BASE      "intersection/" INTERSECTION_ID
#define TOPIC_SENSOR    TOPIC_BASE "/sensor"
#define TOPIC_STATE     TOPIC_BASE "/state"
#define TOPIC_STATUS    TOPIC_BASE "/status"
#define TOPIC_COMMAND   TOPIC_BASE "/command"
#define TOPIC_CONFIG    TOPIC_BASE "/config"

// --- Давтамж ---
#define SENSOR_PERIOD_MS     500UL
#define HEARTBEAT_PERIOD_MS  30000UL
#define MQTT_KEEPALIVE_S     15
#define MQTT_BUFFER_SIZE     512

// --- Wi-Fi автомат тохиргоо (#8) ---
#define WIFI_PORTAL_SSID        "SmartIntersection-Setup"
#define WIFI_CONNECT_TIMEOUT_S  15     // хадгалсан сүлжээнд холбогдох хүлээлт
#define WIFI_PORTAL_TIMEOUT_S   180    // хадгалсан сүлжээтэй үед портал хэр удаан нээлттэй байх
#define WIFI_RETRY_PERIOD_MS    10000UL
#define WIFI_RESET_HOLD_MS      3000UL // BOOT товчийг ингэж удаан дарвал Wi-Fi тохиргоо арилна

// --- Пин ---
// GPIO0 = DevKit дээрх BOOT товч, GPIO2 = цэнхэр LED.
// LED хэлхээ (#5) GPIO2-ийг ашиглах бол STATUS_LED_PIN-ийг -1 болгоно.
#define RESET_BUTTON_PIN 0
#define STATUS_LED_PIN   2
