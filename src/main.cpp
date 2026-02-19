// ═══════════════════════════════════════════════════════════════
//   AURA - Autonomous Universal Reactive Agent
//   Intelligent IoT Agent for ESP32 / STM32
//   Version: 1.0.0
//
//   Features:
//   • Multi-LLM support (OpenAI, Gemini, Anthropic, Groq, OpenRouter, Ollama)
//   • Sensor/Actuator management (Analog, Digital, I2C, PWM)
//   • Web-based configuration UI at http://aura.local
//   • Telegram bot integration
//   • WiFi with captive-portal AP fallback
//   • PlatformIO compatible (ESP32 / STM32)
// ═══════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "AuraConfig.h"
#include "AuraStorage.h"
#include "AuraSensors.h"
#include "AuraLLM.h"
#include "AuraTelegram.h"
#include "AuraWiFi.h"
#include "AuraWebServer.h"
#include "AuraHTML.h"

// ── Global instances ───────────────────────────────────────────
AuraStorage  Storage;
AuraSensors  Sensors;
AuraLLM      LLM;
AuraTelegram Telegram;
AuraWiFi     WiFiManager;
AuraWebServer WebServer;

// ── Timers ────────────────────────────────────────────────────
unsigned long lastSensorRead = 0;

void printBanner() {
  Serial.println();
  Serial.println("  ╔═══════════════════════════════════════╗");
  Serial.println("  ║     AURA - Autonomous Universal       ║");
  Serial.println("  ║          Reactive Agent               ║");
  Serial.println("  ║           v" AURA_VERSION "                         ║");
  Serial.println("  ╚═══════════════════════════════════════╝");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  printBanner();

  // 1. Load config from flash
  Serial.println("[AURA] Initializing storage...");
  Storage.begin();
  Storage.loadSettings();
  Storage.loadSensors();

  // 2. Connect WiFi (or start AP)
  Serial.println("[AURA] Starting WiFi...");
  bool connected = WiFiManager.begin();

  // 3. Initialize sensors/actuators
  Serial.println("[AURA] Initializing I/O...");
  Sensors.begin();
  Sensors.readAll(); // initial read

  // 4. Start web server
  Serial.println("[AURA] Starting web server...");
  WebServer.begin();

  // 5. Start Telegram bot (only if WiFi connected)
  if (connected) {
    Serial.println("[AURA] Starting Telegram bot...");
    Telegram.begin();
  }

  Serial.println("[AURA] Ready! 🚀");
  if (WiFiManager.isAPMode) {
    Serial.println("[AURA] → Connect to WiFi: " AURA_AP_SSID);
    Serial.println("[AURA] → Password: " AURA_AP_PASSWORD);
    Serial.println("[AURA] → Open: http://192.168.4.1");
  } else {
    Serial.printf("[AURA] → Open: http://aura.local  or  http://%s\n",
      #ifdef AURA_ESP32
      WiFi.localIP().toString().c_str()
      #else
      "your-ip"
      #endif
    );
  }
}

void loop() {
  // WiFi health check / AP DNS
  WiFiManager.loop();

  // Poll Telegram
  Telegram.loop();

  // Read sensors at interval
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
    lastSensorRead = millis();
    Sensors.readAll();

    // Optional: print sensor readings to serial
    #if AURA_DEBUG >= 1
    for (int i = 0; i < 8; i++) {
      SensorDef& s = Storage.sensors[i];
      if (!s.enabled || strlen(s.name) == 0) continue;
      Serial.printf("[Sensor] %s: %s %s\n", s.name, s.last_str, s.unit);
    }
    #endif
  }
}
