// ─────────────────────────────────────────────────────────────────────────────
// custom.cpp — AURA Plugin Entry Point
//
// PURPOSE
//   Add custom sensors, actuators, and multi-device MQTT support WITHOUT
//   modifying include/AuraSensors.h or src/main.cpp.
//
// HOW IT WORKS (no main.cpp changes needed)
//   1. A static initializer runs BEFORE setup(), registering a WiFi event hook.
//   2. When WiFi connects (inside WiFiManager.begin() during setup()), the hook:
//      - Calls MQTT.begin() to connect to the broker.
//      - Starts customSensorTask via FreeRTOS xTaskCreate().
//   3. customSensorTask waits 2 s for Sensors.begin()/Wire.begin() to finish,
//      then loops reading custom sensor types directly into Storage.sensors[].
//      buildContextForLLM() picks them up automatically — no other changes needed.
//
// ADDING A NEW SENSOR TYPE
//   1. In the AURA Web UI → I/O Config, set a sensor slot's type to your custom
//      type string (e.g. "aht10_temp").
//   2. Uncomment and implement the matching block in customSensorTask below.
//   3. Add the required library to platformio.ini lib_deps.
//   4. pio run — no other files need editing.
//
// MULTI-DEVICE (MQTT)
//   Remote sensor:   type="mqtt_remote", name="{remoteChipId}/{sensorName}"
//   Remote actuator: type="mqtt",        name="{remoteChipId}/{actuatorName}"
//   See include/AuraMQTT.h for the full topic scheme.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef AURA_ESP32

#include <Arduino.h>
#include <WiFi.h>
#include "AuraConfig.h"    // must come first — defines SENSORS_FILE, MAX_SENSORS, etc.
#include "AuraStorage.h"
#include "AuraSensors.h"
#define AURA_MQTT_IMPL     // define the AuraMQTT static member in this TU only
#include "AuraMQTT.h"

// ── Uncomment sensor libraries as needed ──────────────────────────────────────
// #include <Adafruit_AHTX0.h>      // AHT10/AHT20 — add to platformio.ini:
//                                  //   adafruit/Adafruit AHTX0@^2.0.5
// #include <Adafruit_BMP280.h>     // BMP280 — adafruit/Adafruit BMP280 Library
// #include <DHT.h>                 // DHT11/22 — adafruit/DHT sensor library

// ── Sensor driver instances (uncomment alongside the library above) ───────────
// static Adafruit_AHTX0 _aht;
// static bool           _ahtReady = false;

// ── Global MQTT instance ──────────────────────────────────────────────────────
AuraMQTT MQTT;

// ─────────────────────────────────────────────────────────────────────────────
// customSensorTask
// Reads custom sensor types and writes results directly into Storage.sensors[].
// Runs as a FreeRTOS task so it never blocks the main Arduino loop.
// ─────────────────────────────────────────────────────────────────────────────
static void customSensorTask(void* /*param*/) {
  // Wait for Sensors.begin() / Wire.begin() to complete.
  // WiFiManager.begin() (which triggers this task) runs just before
  // Sensors.begin() in setup(), so a 2-second pause is sufficient.
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  // ── One-time sensor initialisation ─────────────────────────────────────────
  // Uncomment the block for each custom sensor you are using.

  // -- AHT10 / AHT20 --
  // _ahtReady = _aht.begin();
  // if (_ahtReady) Serial.println("[Custom] AHT10 ready");
  // else           Serial.println("[Custom] AHT10 not found");

  // ── Read loop ──────────────────────────────────────────────────────────────
  while (true) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      SensorDef& s = Storage.sensors[i];
      if (!s.enabled) continue;

      // ── AHT10/AHT20 temperature ────────────────────────────────────────
      // if (strcmp(s.type, "aht10_temp") == 0 && _ahtReady) {
      //   sensors_event_t h, t;
      //   _aht.getEvent(&h, &t);
      //   s.last_value = t.temperature;
      //   snprintf(s.last_str, sizeof(s.last_str), "%.1f", t.temperature);
      // }

      // ── AHT10/AHT20 humidity ───────────────────────────────────────────
      // else if (strcmp(s.type, "aht10_hum") == 0 && _ahtReady) {
      //   sensors_event_t h, t;
      //   _aht.getEvent(&h, &t);
      //   s.last_value = h.relative_humidity;
      //   snprintf(s.last_str, sizeof(s.last_str), "%.1f", h.relative_humidity);
      // }

      // ── Add more custom types here ──────────────────────────────────────
      // else if (strcmp(s.type, "my_sensor") == 0) { ... }
    }

    vTaskDelay(SENSOR_READ_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi-connected hook
// Fires when WiFi connects during WiFiManager.begin() in setup().
// At this point Storage is fully loaded — safe to read mqtt_host, etc.
// ─────────────────────────────────────────────────────────────────────────────
static void _onWiFiConnected(WiFiEvent_t /*event*/, WiFiEventInfo_t /*info*/) {
  Serial.println("[Custom] WiFi up — starting MQTT and custom sensor task");
  MQTT.begin();
  xTaskCreate(customSensorTask, "CustomSens", 4096, nullptr, 1, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Static initialiser — runs before setup(), registers the WiFi event hook.
// The lambda trick avoids a named function with external linkage just for init.
// ─────────────────────────────────────────────────────────────────────────────
static const bool _registered = []() -> bool {
  WiFi.onEvent(_onWiFiConnected, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  return true;
}();

#endif // AURA_ESP32
