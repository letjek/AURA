// AURA — on-device intelligence for microcontrollers
// https://github.com/letjek/AURA
//
// Two-layer model architecture:
//   PRIMARY model (built-in, irreplaceable): greetings + hardware
//   integration — GPIO actuators, digital/analog sensors, PWM, temperature,
//   I2C discovery. Compiled into firmware, survives every model swap.
//   ADDITIONAL models (swappable): TOON knowledge packs in LittleFS,
//   installed from the web page, fetched from a URL, or flashed via the
//   PlatformIO data/ folder. A wasm3 WebAssembly runtime runs portable
//   modules on-chip.

#pragma once
#include <Arduino.h>

class AuraClass {
 public:
  struct Config {
    const char *ssid = nullptr;       // station WiFi; nullptr = AP-only mode
    const char *pass = nullptr;
    const char *apSsid = "AURA-Device";  // always-on fallback access point
    const char *apPass = "aura1234";
    const char *hostname = "aura";    // mDNS -> http://aura.local
    int ledPin = 48;                  // onboard LED (ESP32-S3 Super Mini)
    bool heartbeatLog = true;         // 15 s serial heartbeat
  };

  // Start AURA: filesystem, models, wasm runtime, WiFi, web server.
  void begin();                                  // defaults, AP-only
  void begin(const char *ssid, const char *pass);
  void begin(const Config &cfg);

  // Call from loop() — services HTTP and background reconnects.
  void loop();

  // Run a prompt through the models programmatically (same pipeline the
  // web UI uses): primary hardware model first, then knowledge model.
  String ask(const String &prompt);
};

extern AuraClass AURA;
