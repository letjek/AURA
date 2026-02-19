#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "AuraStorage.h"
#include "AuraConfig.h"

// ── Optional sensor libraries (comment out if not needed) ─────
// #include <DHT.h>
// #include <OneWire.h>
// #include <DallasTemperature.h>
// #include <Adafruit_BMP280.h>
// #include <Adafruit_SSD1306.h>

class AuraSensors {
public:
  void begin() {
    Wire.begin(AURA_I2C_SDA, AURA_I2C_SCL);
    Serial.println("[Sensors] I2C initialized");
    initPins();
  }

  void initPins() {
    for (int i = 0; i < 8; i++) {
      SensorDef& s = Storage.sensors[i];
      if (!s.enabled || s.pin < 0) continue;
      if (strcmp(s.type, "digital") == 0) {
        pinMode(s.pin, INPUT);
        Serial.printf("[Sensors] Digital sensor '%s' on pin %d\n", s.name, s.pin);
      } else if (strcmp(s.type, "analog") == 0) {
        // analog pins are input by default
        Serial.printf("[Sensors] Analog sensor '%s' on pin %d\n", s.name, s.pin);
      }

      ActuatorDef& a = Storage.actuators[i];
      if (!a.enabled || a.pin < 0) continue;
      if (strcmp(a.type, "digital") == 0 || strcmp(a.type, "pwm") == 0) {
        pinMode(a.pin, OUTPUT);
        Serial.printf("[Sensors] Actuator '%s' on pin %d\n", a.name, a.pin);
      }
    }
  }

  void readAll() {
    for (int i = 0; i < 8; i++) {
      SensorDef& s = Storage.sensors[i];
      if (!s.enabled) continue;
      readSensor(s);
    }
  }

  void readSensor(SensorDef& s) {
    if (strcmp(s.type, "analog") == 0 && s.pin >= 0) {
      int raw = analogRead(s.pin);
      s.last_value = raw;
      snprintf(s.last_str, sizeof(s.last_str), "%d", raw);

    } else if (strcmp(s.type, "digital") == 0 && s.pin >= 0) {
      int v = digitalRead(s.pin);
      s.last_value = v;
      snprintf(s.last_str, sizeof(s.last_str), "%s", v ? "HIGH" : "LOW");

    } else if (strcmp(s.type, "i2c_raw") == 0) {
      // Generic I2C read - reads 2 bytes from address
      uint8_t addr = (uint8_t)strtol(s.i2c_addr, nullptr, 16);
      Wire.requestFrom(addr, (uint8_t)2);
      if (Wire.available() >= 2) {
        uint16_t val = (Wire.read() << 8) | Wire.read();
        s.last_value = val;
        snprintf(s.last_str, sizeof(s.last_str), "%d", val);
      }

    } else if (strcmp(s.type, "voltage") == 0 && s.pin >= 0) {
      // Read analog and convert to voltage (3.3V / 4095)
      int raw = analogRead(s.pin);
      float v = raw * (3.3f / 4095.0f);
      s.last_value = v;
      snprintf(s.last_str, sizeof(s.last_str), "%.2fV", v);

    } else if (strcmp(s.type, "mock") == 0) {
      // For testing without hardware
      s.last_value = random(0, 100);
      snprintf(s.last_str, sizeof(s.last_str), "%.1f", s.last_value);
    }
  }

  // ── Actuator Control ────────────────────────────────────────
  bool setActuator(const char* name, bool state, int pwmValue = -1) {
    for (int i = 0; i < 8; i++) {
      ActuatorDef& a = Storage.actuators[i];
      if (!a.enabled) continue;
      if (strcasecmp(a.name, name) == 0) {
        a.state = state;
        if (strcmp(a.type, "digital") == 0) {
          digitalWrite(a.pin, state ? HIGH : LOW);
          Serial.printf("[Actuator] %s -> %s\n", name, state ? "ON" : "OFF");
          return true;
        } else if (strcmp(a.type, "pwm") == 0) {
          int val = (pwmValue >= 0) ? pwmValue : (state ? 255 : 0);
          a.value = val;
          #ifdef AURA_ESP32
            ledcWrite(a.pin, val);
          #else
            analogWrite(a.pin, val);
          #endif
          Serial.printf("[Actuator] %s PWM -> %d\n", name, val);
          return true;
        }
      }
    }
    Serial.printf("[Actuator] '%s' not found\n", name);
    return false;
  }

  // ── I2C Scanner (for Web UI) ────────────────────────────────
  String scanI2C() {
    String result = "[";
    bool first = true;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      uint8_t err = Wire.endTransmission();
      if (err == 0) {
        if (!first) result += ",";
        char buf[8];
        snprintf(buf, sizeof(buf), "\"0x%02X\"", addr);
        result += buf;
        first = false;
      }
    }
    result += "]";
    return result;
  }

  // ── Build context string for LLM ────────────────────────────
  String buildContextForLLM() {
    String ctx = "Current sensor readings:\n";
    bool any = false;
    for (int i = 0; i < 8; i++) {
      SensorDef& s = Storage.sensors[i];
      if (!s.enabled || strlen(s.name) == 0) continue;
      ctx += "- " + String(s.name) + ": " + String(s.last_str);
      if (strlen(s.unit) > 0) ctx += " " + String(s.unit);
      ctx += "\n";
      any = true;
    }
    if (!any) ctx += "- No sensors configured\n";

    ctx += "\nAvailable actuators:\n";
    any = false;
    for (int i = 0; i < 8; i++) {
      ActuatorDef& a = Storage.actuators[i];
      if (!a.enabled || strlen(a.name) == 0) continue;
      ctx += "- " + String(a.name) + " (" + String(a.type) + "): ";
      ctx += a.state ? "ON" : "OFF";
      if (strcmp(a.type, "pwm") == 0) {
        ctx += " [pwm=" + String(a.value) + "]";
      }
      ctx += "\n";
      any = true;
    }
    if (!any) ctx += "- No actuators configured\n";

    ctx += "\nTo control actuators, reply with JSON commands in this format:\n";
    ctx += "{\"actions\":[{\"name\":\"LED\",\"state\":true},{\"name\":\"Fan\",\"pwm\":128}]}\n";
    return ctx;
  }
};

extern AuraSensors Sensors;
