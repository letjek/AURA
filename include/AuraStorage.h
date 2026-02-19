#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#ifdef AURA_ESP32
  #include <SPIFFS.h>
  #define AURA_FS SPIFFS
#else
  // STM32 - use SD or EEPROM fallback
  #include <SPIFFS.h>
  #define AURA_FS SPIFFS
#endif

// ─── Global runtime config struct ────────────────────────────────────────────
struct AuraSettings {
  // WiFi
  char wifi_ssid[64]      = "";
  char wifi_pass[64]      = "";

  // LLM
  char llm_provider[32]   = "openai";
  char llm_model[64]      = "gpt-4o-mini";
  char llm_api_key[128]   = "";
  char llm_base_url[128]  = "";   // for custom/ollama endpoints
  int  llm_max_tokens     = 512;
  float llm_temperature   = 0.7f;

  // System prompt / agent personality
  char system_prompt[512] = "You are AURA, an intelligent IoT assistant. "
                            "You can read sensors and control actuators. "
                            "Be concise and helpful.";

  // Telegram
  char telegram_token[128] = "";
  char telegram_chat_id[32]= "";   // optional whitelist

  // MQTT (optional)
  char mqtt_host[64]      = "";
  int  mqtt_port          = 1883;
  char mqtt_user[32]      = "";
  char mqtt_pass[32]      = "";
  char mqtt_topic[64]     = "aura/events";
};

// ─── Sensor/Actuator descriptor ──────────────────────────────────────────────
struct SensorDef {
  bool   enabled      = false;
  char   name[32]     = "";
  char   type[16]     = "";   // "analog" | "digital" | "i2c" | "dht" | "ds18b20"
  int    pin          = -1;
  char   i2c_addr[8]  = "0x00";
  char   unit[16]     = "";
  float  last_value   = 0;
  char   last_str[32] = "";
};

struct ActuatorDef {
  bool  enabled   = false;
  char  name[32]  = "";
  char  type[16]  = "";   // "digital" | "pwm" | "i2c"
  int   pin       = -1;
  bool  state     = false;
  int   value     = 0;    // 0-255 for PWM
};

// ─── Storage class ────────────────────────────────────────────────────────────
class AuraStorage {
public:
  AuraSettings settings;
  SensorDef    sensors[8];
  ActuatorDef  actuators[8];

  bool begin() {
    if (!AURA_FS.begin(true)) {
      Serial.println("[Storage] SPIFFS mount failed");
      return false;
    }
    Serial.println("[Storage] SPIFFS mounted OK");
    return true;
  }

  bool loadSettings() {
    File f = AURA_FS.open(CONFIG_FILE, "r");
    if (!f) { Serial.println("[Storage] No config file, using defaults"); return false; }
    StaticJsonDocument<2048> doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();

    strlcpy(settings.wifi_ssid,    doc["wifi_ssid"]   | "", sizeof(settings.wifi_ssid));
    strlcpy(settings.wifi_pass,    doc["wifi_pass"]   | "", sizeof(settings.wifi_pass));
    strlcpy(settings.llm_provider, doc["llm_provider"]| "openai", sizeof(settings.llm_provider));
    strlcpy(settings.llm_model,    doc["llm_model"]   | "gpt-4o-mini", sizeof(settings.llm_model));
    strlcpy(settings.llm_api_key,  doc["llm_api_key"] | "", sizeof(settings.llm_api_key));
    strlcpy(settings.llm_base_url, doc["llm_base_url"]| "", sizeof(settings.llm_base_url));
    strlcpy(settings.system_prompt,doc["system_prompt"]| settings.system_prompt, sizeof(settings.system_prompt));
    strlcpy(settings.telegram_token,   doc["telegram_token"]   | "", sizeof(settings.telegram_token));
    strlcpy(settings.telegram_chat_id, doc["telegram_chat_id"] | "", sizeof(settings.telegram_chat_id));
    strlcpy(settings.mqtt_host, doc["mqtt_host"] | "", sizeof(settings.mqtt_host));
    settings.mqtt_port     = doc["mqtt_port"] | 1883;
    settings.llm_max_tokens= doc["llm_max_tokens"] | 512;
    settings.llm_temperature=doc["llm_temperature"] | 0.7f;
    Serial.println("[Storage] Settings loaded");
    return true;
  }

  bool saveSettings() {
    StaticJsonDocument<2048> doc;
    doc["wifi_ssid"]      = settings.wifi_ssid;
    doc["wifi_pass"]      = settings.wifi_pass;
    doc["llm_provider"]   = settings.llm_provider;
    doc["llm_model"]      = settings.llm_model;
    doc["llm_api_key"]    = settings.llm_api_key;
    doc["llm_base_url"]   = settings.llm_base_url;
    doc["llm_max_tokens"] = settings.llm_max_tokens;
    doc["llm_temperature"]= settings.llm_temperature;
    doc["system_prompt"]  = settings.system_prompt;
    doc["telegram_token"] = settings.telegram_token;
    doc["telegram_chat_id"]=settings.telegram_chat_id;
    doc["mqtt_host"]      = settings.mqtt_host;
    doc["mqtt_port"]      = settings.mqtt_port;

    File f = AURA_FS.open(CONFIG_FILE, "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    Serial.println("[Storage] Settings saved");
    return true;
  }

  bool loadSensors() {
    File f = AURA_FS.open(SENSORS_FILE, "r");
    if (!f) return false;
    StaticJsonDocument<2048> doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();
    JsonArray arr = doc["sensors"].as<JsonArray>();
    int i = 0;
    for (JsonObject s : arr) {
      if (i >= 8) break;
      sensors[i].enabled = s["enabled"] | false;
      strlcpy(sensors[i].name, s["name"] | "", sizeof(sensors[i].name));
      strlcpy(sensors[i].type, s["type"] | "", sizeof(sensors[i].type));
      sensors[i].pin = s["pin"] | -1;
      strlcpy(sensors[i].i2c_addr, s["i2c_addr"] | "0x00", sizeof(sensors[i].i2c_addr));
      strlcpy(sensors[i].unit,     s["unit"]     | "",      sizeof(sensors[i].unit));
      i++;
    }
    JsonArray arr2 = doc["actuators"].as<JsonArray>();
    i = 0;
    for (JsonObject a : arr2) {
      if (i >= 8) break;
      actuators[i].enabled = a["enabled"] | false;
      strlcpy(actuators[i].name, a["name"] | "", sizeof(actuators[i].name));
      strlcpy(actuators[i].type, a["type"] | "", sizeof(actuators[i].type));
      actuators[i].pin = a["pin"] | -1;
      i++;
    }
    return true;
  }

  bool saveSensors() {
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.createNestedArray("sensors");
    for (int i = 0; i < 8; i++) {
      JsonObject s = arr.createNestedObject();
      s["enabled"]  = sensors[i].enabled;
      s["name"]     = sensors[i].name;
      s["type"]     = sensors[i].type;
      s["pin"]      = sensors[i].pin;
      s["i2c_addr"] = sensors[i].i2c_addr;
      s["unit"]     = sensors[i].unit;
    }
    JsonArray arr2 = doc.createNestedArray("actuators");
    for (int i = 0; i < 8; i++) {
      JsonObject a = arr2.createNestedObject();
      a["enabled"] = actuators[i].enabled;
      a["name"]    = actuators[i].name;
      a["type"]    = actuators[i].type;
      a["pin"]     = actuators[i].pin;
    }
    File f = AURA_FS.open(SENSORS_FILE, "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
  }

  String getSensorStateJson() {
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.createNestedArray("sensors");
    for (int i = 0; i < 8; i++) {
      if (!sensors[i].enabled) continue;
      JsonObject s = arr.createNestedObject();
      s["name"]  = sensors[i].name;
      s["value"] = sensors[i].last_value;
      s["str"]   = sensors[i].last_str;
      s["unit"]  = sensors[i].unit;
    }
    JsonArray arr2 = doc.createNestedArray("actuators");
    for (int i = 0; i < 8; i++) {
      if (!actuators[i].enabled) continue;
      JsonObject a = arr2.createNestedObject();
      a["name"]  = actuators[i].name;
      a["state"] = actuators[i].state;
      a["value"] = actuators[i].value;
    }
    String out;
    serializeJson(doc, out);
    return out;
  }
};

extern AuraStorage Storage;
