#pragma once
#include <Arduino.h>
#include "AuraStorage.h"
#include "AuraSensors.h"
#include "AuraLLM.h"

#ifdef AURA_ESP32
  #include <ESPAsyncWebServer.h>
  #include <AsyncJson.h>
  #include <AsyncTCP.h>
  #include <WiFi.h>
  #include <ESPmDNS.h>
#endif

// Embedded HTML (stored in flash)
extern const char AURA_HTML[] PROGMEM;

class AuraWebServer {
public:
  #ifdef AURA_ESP32
  AsyncWebServer server{80};
  #endif

  void begin() {
    #ifdef AURA_ESP32
    // mDNS - http://aura.local
    if (MDNS.begin(AURA_HOSTNAME)) {
      Serial.println("[Web] mDNS started: http://aura.local");
    }

    // ── Main UI ────────────────────────────────────────────────
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
      req->send_P(200, "text/html", AURA_HTML);
    });

    // ── API: Get all settings ──────────────────────────────────
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* req) {
      AuraSettings& s = Storage.settings;
      StaticJsonDocument<2048> doc;
      doc["wifi_ssid"]      = s.wifi_ssid;
      doc["llm_provider"]   = s.llm_provider;
      doc["llm_model"]      = s.llm_model;
      doc["llm_api_key"]    = "****"; // masked
      doc["llm_base_url"]   = s.llm_base_url;
      doc["llm_max_tokens"] = s.llm_max_tokens;
      doc["llm_temperature"]= s.llm_temperature;
      doc["system_prompt"]  = s.system_prompt;
      doc["telegram_token"] = strlen(s.telegram_token) > 0 ? "****" : "";
      doc["telegram_chat_id"]=s.telegram_chat_id;
      doc["mqtt_host"]      = s.mqtt_host;
      doc["mqtt_port"]      = s.mqtt_port;
      String out;
      serializeJson(doc, out);
      req->send(200, "application/json", out);
    });

    // ── API: Save settings ─────────────────────────────────────
    AsyncCallbackJsonWebHandler* saveHandler =
      new AsyncCallbackJsonWebHandler("/api/settings", [](AsyncWebServerRequest* req, JsonVariant& json) {
        AuraSettings& s = Storage.settings;
        JsonObject obj = json.as<JsonObject>();

        if (obj.containsKey("wifi_ssid"))    strlcpy(s.wifi_ssid,    obj["wifi_ssid"],    sizeof(s.wifi_ssid));
        if (obj.containsKey("wifi_pass"))    strlcpy(s.wifi_pass,    obj["wifi_pass"],    sizeof(s.wifi_pass));
        if (obj.containsKey("llm_provider")) strlcpy(s.llm_provider, obj["llm_provider"], sizeof(s.llm_provider));
        if (obj.containsKey("llm_model"))    strlcpy(s.llm_model,    obj["llm_model"],    sizeof(s.llm_model));
        if (obj.containsKey("llm_base_url")) strlcpy(s.llm_base_url, obj["llm_base_url"], sizeof(s.llm_base_url));
        if (obj.containsKey("system_prompt"))strlcpy(s.system_prompt,obj["system_prompt"],sizeof(s.system_prompt));
        if (obj.containsKey("llm_max_tokens")) s.llm_max_tokens = obj["llm_max_tokens"];
        if (obj.containsKey("llm_temperature"))s.llm_temperature = obj["llm_temperature"];
        if (obj.containsKey("telegram_chat_id")) strlcpy(s.telegram_chat_id, obj["telegram_chat_id"], sizeof(s.telegram_chat_id));
        if (obj.containsKey("mqtt_host"))    strlcpy(s.mqtt_host, obj["mqtt_host"], sizeof(s.mqtt_host));
        if (obj.containsKey("mqtt_port"))    s.mqtt_port = obj["mqtt_port"];

        // Only update secrets if not masked
        if (obj.containsKey("llm_api_key") && String(obj["llm_api_key"].as<const char*>()) != "****")
          strlcpy(s.llm_api_key, obj["llm_api_key"], sizeof(s.llm_api_key));
        if (obj.containsKey("telegram_token") && String(obj["telegram_token"].as<const char*>()) != "****")
          strlcpy(s.telegram_token, obj["telegram_token"], sizeof(s.telegram_token));

        Storage.saveSettings();
        req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Settings saved\"}");
      });
    server.addHandler(saveHandler);

    // ── API: Get sensor/actuator config ────────────────────────
    server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest* req) {
      req->send(200, "application/json", Storage.getSensorStateJson());
    });

    // ── API: Save sensor config ────────────────────────────────
    AsyncCallbackJsonWebHandler* sensHandler =
      new AsyncCallbackJsonWebHandler("/api/sensors", [](AsyncWebServerRequest* req, JsonVariant& json) {
        JsonObject obj = json.as<JsonObject>();
        if (obj.containsKey("sensors")) {
          JsonArray arr = obj["sensors"].as<JsonArray>();
          int i = 0;
          for (JsonObject s : arr) {
            if (i >= 8) break;
            Storage.sensors[i].enabled = s["enabled"] | false;
            strlcpy(Storage.sensors[i].name,     s["name"]     | "", 32);
            strlcpy(Storage.sensors[i].type,     s["type"]     | "", 16);
            Storage.sensors[i].pin = s["pin"] | -1;
            strlcpy(Storage.sensors[i].i2c_addr, s["i2c_addr"] | "0x00", 8);
            strlcpy(Storage.sensors[i].unit,     s["unit"]     | "", 16);
            i++;
          }
        }
        if (obj.containsKey("actuators")) {
          JsonArray arr = obj["actuators"].as<JsonArray>();
          int i = 0;
          for (JsonObject a : arr) {
            if (i >= 8) break;
            Storage.actuators[i].enabled = a["enabled"] | false;
            strlcpy(Storage.actuators[i].name, a["name"] | "", 32);
            strlcpy(Storage.actuators[i].type, a["type"] | "", 16);
            Storage.actuators[i].pin = a["pin"] | -1;
            i++;
          }
        }
        Storage.saveSensors();
        Sensors.initPins();
        req->send(200, "application/json", "{\"ok\":true}");
      });
    server.addHandler(sensHandler);

    // ── API: Control actuator ──────────────────────────────────
    server.on("/api/actuator", HTTP_POST, [](AsyncWebServerRequest* req){},
      nullptr, [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        StaticJsonDocument<256> doc;
        deserializeJson(doc, data, len);
        const char* name = doc["name"] | "";
        bool state = doc["state"] | false;
        int pwm    = doc["pwm"] | -1;
        bool ok = Sensors.setActuator(name, state, pwm);
        req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
      });

    // ── API: Chat (direct LLM call from browser) ───────────────
    server.on("/api/chat", HTTP_POST, [](AsyncWebServerRequest* req){},
      nullptr, [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        StaticJsonDocument<512> doc;
        deserializeJson(doc, data, len);
        const char* msg = doc["message"] | "";
        Sensors.readAll();
        String ctx = Sensors.buildContextForLLM();
        String reply = LLM.chat(String(msg), ctx);
        StaticJsonDocument<1024> resp;
        resp["reply"] = reply;
        String out;
        serializeJson(resp, out);
        req->send(200, "application/json", out);
      });

    // ── API: I2C scan ──────────────────────────────────────────
    server.on("/api/i2cscan", HTTP_GET, [](AsyncWebServerRequest* req) {
      String addrs = Sensors.scanI2C();
      req->send(200, "application/json", "{\"devices\":" + addrs + "}");
    });

    // ── API: System info ───────────────────────────────────────
    server.on("/api/sysinfo", HTTP_GET, [](AsyncWebServerRequest* req) {
      StaticJsonDocument<512> doc;
      doc["version"]    = AURA_VERSION;
      doc["heap"]       = ESP.getFreeHeap();
      doc["uptime_s"]   = millis() / 1000;
      doc["wifi_rssi"]  = WiFi.RSSI();
      doc["ip"]         = WiFi.localIP().toString();
      String out;
      serializeJson(doc, out);
      req->send(200, "application/json", out);
    });

    // ── API: Restart ───────────────────────────────────────────
    server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* req) {
      req->send(200, "application/json", "{\"ok\":true}");
      delay(500);
      ESP.restart();
    });

    // ── 404 ────────────────────────────────────────────────────
    server.onNotFound([](AsyncWebServerRequest* req) {
      req->send(404, "text/plain", "Not found");
    });

    server.begin();
    Serial.printf("[Web] Server started at http://%s\n", WiFi.localIP().toString().c_str());
    #endif
  }
};

extern AuraWebServer WebServer;
