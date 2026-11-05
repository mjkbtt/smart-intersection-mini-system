#include "wifi_setup.h"

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>  // tzapu/WiFiManager >= 2.0.17

#include "config.h"
#include "secrets_include.h"

namespace wifisetup {
namespace {

WiFiManager wm;
Preferences prefs;

char hostBuf[64] = MQTT_HOST_DEFAULT;
char portBuf[6] = "";
uint16_t port = MQTT_PORT_DEFAULT;

WiFiManagerParameter paramHost("mqtt_host", "MQTT broker IP / host", hostBuf, sizeof(hostBuf) - 1);
WiFiManagerParameter paramPort("mqtt_port", "MQTT port", "1883", 5);

bool wasConnected = false;
bool mqttChanged = false;
uint32_t lastRetry = 0;
uint32_t buttonDownAt = 0;
uint32_t lastLedToggle = 0;
bool ledState = false;

void loadMqttConfig() {
  prefs.begin("si", true);
  String h = prefs.getString("mqtt_host", MQTT_HOST_DEFAULT);
  port = prefs.getUShort("mqtt_port", MQTT_PORT_DEFAULT);
  prefs.end();
  strlcpy(hostBuf, h.c_str(), sizeof(hostBuf));
  snprintf(portBuf, sizeof(portBuf), "%u", port);
  paramHost.setValue(hostBuf, sizeof(hostBuf) - 1);
  paramPort.setValue(portBuf, 5);
}

void saveMqttConfig() {
  const char *h = paramHost.getValue();
  long p = atol(paramPort.getValue());
  if (strlen(h) == 0 || p <= 0 || p > 65535) {
    Serial.println("[WiFi] MQTT тохиргоо буруу, хадгалсангүй");
    return;
  }
  strlcpy(hostBuf, h, sizeof(hostBuf));
  port = (uint16_t)p;
  prefs.begin("si", false);
  prefs.putString("mqtt_host", hostBuf);
  prefs.putUShort("mqtt_port", port);
  prefs.end();
  mqttChanged = true;
  Serial.printf("[WiFi] MQTT broker хадгалагдлаа: %s:%u\n", hostBuf, port);
}

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFi] Холбогдлоо: %s, IP %s, RSSI %d dBm\n", WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (wasConnected) {
        Serial.printf("[WiFi] Тасарлаа (reason %d)\n", info.wifi_sta_disconnected.reason);
      }
      break;
    default:
      break;
  }
}

void startPortal(bool forever) {
  wm.setConfigPortalTimeout(forever ? 0 : WIFI_PORTAL_TIMEOUT_S);
  Serial.printf("[WiFi] Тохиргооны портал нээгдлээ: SSID \"%s\" -> http://192.168.4.1\n", WIFI_PORTAL_SSID);
  wm.startConfigPortal(WIFI_PORTAL_SSID, WIFI_PORTAL_PASS);
}

void handleResetButton(uint32_t now) {
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    if (buttonDownAt == 0) buttonDownAt = now;
    if (now - buttonDownAt >= WIFI_RESET_HOLD_MS) {
      Serial.println("[WiFi] BOOT товч удаан дарагдлаа: Wi-Fi тохиргоог арилгаж байна");
      buttonDownAt = 0;
      if (wm.getConfigPortalActive()) wm.stopConfigPortal();
      wm.resetSettings();
      wasConnected = false;
      startPortal(true);
    }
  } else {
    buttonDownAt = 0;
  }
}

void updateStatusLed(uint32_t now) {
#if STATUS_LED_PIN >= 0
  // Портал: хурдан (150 мс), холбогдоогүй: удаан (1 с), холбогдсон: асаалттай
  uint32_t period = wm.getConfigPortalActive() ? 150 : (WiFi.isConnected() ? 0 : 1000);
  if (period == 0) {
    digitalWrite(STATUS_LED_PIN, HIGH);
  } else if (now - lastLedToggle >= period) {
    lastLedToggle = now;
    ledState = !ledState;
    digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
  }
#endif
}

}  // namespace

void begin() {
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
#if STATUS_LED_PIN >= 0
  pinMode(STATUS_LED_PIN, OUTPUT);
#endif
  loadMqttConfig();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_ID);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(onWifiEvent);

  wm.setHostname(DEVICE_ID);
  wm.setTitle("Smart Intersection");
  wm.addParameter(&paramHost);
  wm.addParameter(&paramPort);
  wm.setSaveParamsCallback(saveMqttConfig);
  wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_S);
  wm.setConnectRetries(2);
  wm.setConfigPortalBlocking(false);  // портал нээлттэй үед ч loop() ажиллана
  wm.setBreakAfterConfig(true);       // холбогдож чадаагүй ч параметрүүдийг хадгална
  wm.setWiFiAutoReconnect(true);

  bool saved = wm.getWiFiIsSaved();
  wm.setConfigPortalTimeout(saved ? WIFI_PORTAL_TIMEOUT_S : 0);
  Serial.printf("[WiFi] Хадгалсан сүлжээ: %s\n", saved ? wm.getWiFiSSID().c_str() : "(байхгүй)");
  if (wm.autoConnect(WIFI_PORTAL_SSID, WIFI_PORTAL_PASS)) {
    wasConnected = true;
  } else {
    Serial.printf("[WiFi] Холбогдсонгүй -> портал \"%s\" (http://192.168.4.1)\n", WIFI_PORTAL_SSID);
  }
  Serial.printf("[WiFi] MQTT broker: %s:%u\n", hostBuf, port);
}

void loop() {
  uint32_t now = millis();
  wm.process();
  handleResetButton(now);
  updateStatusLed(now);

  bool up = WiFi.isConnected();
  if (up) {
    if (!wasConnected) wasConnected = true;
    return;
  }

  // Портал хаагдсан, хадгалсан сүлжээ байгаа бол үе үе дахин оролдоно.
  if (!wm.getConfigPortalActive() && now - lastRetry >= WIFI_RETRY_PERIOD_MS) {
    lastRetry = now;
    if (wm.getWiFiIsSaved()) {
      Serial.println("[WiFi] Дахин холбогдож байна...");
      WiFi.mode(WIFI_STA);
      WiFi.begin();
    } else {
      startPortal(true);
    }
  }
}

bool connected() { return WiFi.isConnected(); }
bool portalActive() { return wm.getConfigPortalActive(); }
const char *mqttHost() { return hostBuf; }
uint16_t mqttPort() { return port; }

bool takeMqttConfigChanged() {
  bool c = mqttChanged;
  mqttChanged = false;
  return c;
}

}  // namespace wifisetup
