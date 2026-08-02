// AURA Basic example
//
// Boots AURA on an ESP32: web chat UI, primary hardware model (GPIO/ADC/
// PWM/I2C), swappable TOON knowledge model, wasm3 runtime.
//
// 1. Flash this sketch.
// 2. Join the "AURA-Device" WiFi (password "aura1234") or let it join yours.
// 3. Open http://192.168.4.1/ (AP) or http://aura.local/ (same LAN).
// 4. Try: halo · hw · led on · cek suhu · i2c scan · what is a plc

#include <AURA.h>

void setup() {
  AuraClass::Config cfg;
  // Optional: join your WiFi (2.4 GHz). Leave commented for AP-only mode.
  // cfg.ssid = "YourWiFi";
  // cfg.pass = "YourPassword";
  // cfg.hostname = "aura";          // -> http://aura.local/
  // cfg.apSsid = "AURA-Device";     // fallback hotspot name
  // cfg.apPass = "aura1234";
  // cfg.ledPin = 48;                // onboard LED (ESP32-S3 Super Mini)
  AURA.begin(cfg);
}

void loop() {
  AURA.loop();

  // You can also ask AURA from code — same pipeline as the web UI:
  // String answer = AURA.ask("cek suhu sekarang");
}
