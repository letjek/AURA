#pragma once
#include <Arduino.h>
#include "AuraStorage.h"
#include "AuraConfig.h"

#ifdef AURA_ESP32
  #include <WiFi.h>
  #include <DNSServer.h>
#endif

class AuraWiFi {
public:
  bool isAPMode = false;

  #ifdef AURA_ESP32
  DNSServer dnsServer;
  #endif

  bool begin() {
    #ifdef AURA_ESP32
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(AURA_HOSTNAME);

    const char* ssid = Storage.settings.wifi_ssid;
    const char* pass = Storage.settings.wifi_pass;

    if (strlen(ssid) == 0) {
      Serial.println("[WiFi] No credentials, starting AP");
      startAP();
      return false;
    }

    Serial.printf("[WiFi] Connecting to %s...\n", ssid);
    WiFi.begin(ssid, pass);

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
      delay(500);
      Serial.print(".");
      tries++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
      return true;
    } else {
      Serial.println("[WiFi] Failed, starting AP mode");
      startAP();
      return false;
    }
    #endif
    return false;
  }

  void startAP() {
    #ifdef AURA_ESP32
    isAPMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AURA_AP_SSID, AURA_AP_PASSWORD);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    // Captive portal DNS
    dnsServer.start(53, "*", apIP);

    Serial.printf("[WiFi] AP started: SSID=%s IP=192.168.4.1\n", AURA_AP_SSID);
    #endif
  }

  void loop() {
    #ifdef AURA_ESP32
    if (isAPMode) {
      dnsServer.processNextRequest();
    } else if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Lost connection, reconnecting...");
      WiFi.reconnect();
    }
    #endif
  }
};

extern AuraWiFi WiFiManager;
