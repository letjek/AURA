// AURA dev example — ESP32-S3 Super Mini
//
// The entire engine lives in the AURA library (../../src). This sketch just
// configures WiFi and starts it. Knowledge model ships from data/model.toon
// via `pio run -t uploadfs`.

#include <AURA.h>

void setup()
{
  AuraClass::Config cfg;
  cfg.ssid = "sample"; // 2.4 GHz station WiFi
  cfg.pass = "samplepass";
  cfg.apSsid = "ESP32-SuperMini-WASM"; // always-on fallback hotspot
  cfg.apPass = "prompt123";
  cfg.hostname = "esp32wasm"; // -> http://esp32wasm.local/
  AURA.begin(cfg);
}

void loop() { AURA.loop(); }
