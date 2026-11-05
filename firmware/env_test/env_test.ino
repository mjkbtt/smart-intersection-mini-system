/*
 * ESP32 орчны тест — Issue [1.1.1] ESP32 тохиргоо ба орчин бэлдэх (#3)
 *
 *  T1: ESP32 core/драйвер суулгаж компиляц хийгдэж байгааг шалгана
 *  T2: WiFi ба PubSubClient сан суусан эсэхийг (include + объект үүсгэж) шалгана
 *  T3: Blink + Serial output, 30 минут тасралтгүй ажиллаж байгааг тайлагнана
 *
 * Serial Monitor: 115200 baud.
 * Хүлээн авах шалгуур: "[PASS] 30 минут тасралтгүй ажиллалаа" мөр гарч,
 * дунд нь reset (дахин эхэлсэн мэдээлэл) гараагүй байх.
 */
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_system.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2   // ESP32 DevKit V1 дээрх цэнхэр LED
#endif

static const uint32_t BLINK_MS = 500;
static const uint32_t REPORT_MS = 10UL * 1000;        // 10 секунд тутам тайлан
static const uint32_t TARGET_MS = 30UL * 60 * 1000;   // 30 минут

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);   // Сан зөв холбогдсон эсэхийг шалгахад л ашиглана

static uint32_t lastBlink = 0;
static uint32_t lastReport = 0;
static uint32_t loops = 0;
static uint32_t startHeap = 0;
static bool ledOn = false;
static bool passed = false;

static const char *resetReasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "POWERON (тэжээл залгасан)";
    case ESP_RST_SW:       return "SW (программаар restart)";
    case ESP_RST_PANIC:    return "PANIC (алдаа/exception!)";
    case ESP_RST_INT_WDT:  return "INT_WDT (watchdog!)";
    case ESP_RST_TASK_WDT: return "TASK_WDT (watchdog!)";
    case ESP_RST_WDT:      return "WDT (watchdog!)";
    case ESP_RST_BROWNOUT: return "BROWNOUT (тэжээл хүрэлцэхгүй!)";
    case ESP_RST_EXT:      return "EXT (EN товч)";
    case ESP_RST_DEEPSLEEP:return "DEEPSLEEP";
    default:               return "UNKNOWN";
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println();
  Serial.println(F("========== ESP32 ОРЧНЫ ТЕСТ =========="));
  Serial.printf("Чип          : %s rev %d, %d цөм, %lu MHz\n", ESP.getChipModel(),
                ESP.getChipRevision(), ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf("Flash        : %lu KB\n", (unsigned long)(ESP.getFlashChipSize() / 1024));
  Serial.printf("SDK          : %s\n", ESP.getSdkVersion());
  Serial.printf("MAC (STA)    : %s\n", WiFi.macAddress().c_str());
  Serial.printf("Reset шалтгаан: %s\n", resetReasonText(esp_reset_reason()));

  // T2: сангууд ажиллаж байгааг шалгах
  WiFi.mode(WIFI_STA);
  int found = WiFi.scanNetworks();
  Serial.printf("[WiFi]       : сан OK, %d сүлжээ олдлоо\n", found);
  for (int i = 0; i < found && i < 5; i++) {
    Serial.printf("               - %s (%d dBm)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }
  WiFi.scanDelete();
  mqtt.setBufferSize(512);
  Serial.printf("[PubSubClient]: сан OK, buffer %u байт, төлөв %d\n", mqtt.getBufferSize(), mqtt.state());

  startHeap = ESP.getFreeHeap();
  Serial.printf("Эхлэх heap   : %lu байт\n", (unsigned long)startHeap);
  Serial.println(F("Blink эхэллээ. 30 минут тасралтгүй ажиллахыг хүлээнэ үү..."));
  Serial.println(F("======================================"));
}

void loop() {
  uint32_t now = millis();
  loops++;

  if (now - lastBlink >= BLINK_MS) {
    lastBlink = now;
    ledOn = !ledOn;
    digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
  }

  if (now - lastReport >= REPORT_MS) {
    lastReport = now;
    uint32_t sec = now / 1000;
    Serial.printf("[%02lu:%02lu:%02lu] heap=%lu (min %lu) loop/s=%lu\n",
                  (unsigned long)(sec / 3600), (unsigned long)(sec / 60 % 60), (unsigned long)(sec % 60),
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
                  (unsigned long)(loops / (REPORT_MS / 1000)));
    loops = 0;
  }

  if (!passed && now >= TARGET_MS) {
    passed = true;
    long heapDiff = (long)ESP.getFreeHeap() - (long)startHeap;
    Serial.println(F("--------------------------------------"));
    Serial.printf("[PASS] 30 минут тасралтгүй ажиллалаа. Heap өөрчлөлт: %ld байт\n", heapDiff);
    Serial.println(F("--------------------------------------"));
  }
}
