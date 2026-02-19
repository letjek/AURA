#pragma once

// ═══════════════════════════════════════════════════════════════
//   AURA - Autonomous Universal Reactive Agent
//   Version: 1.0.0
//   Author:  Your Name
//   License: MIT
// ═══════════════════════════════════════════════════════════════
//
//  Configure AURA behavior below, OR use the Web UI at http://aura.local
//  All settings can be changed at runtime via the browser interface.
// ═══════════════════════════════════════════════════════════════

// ── Device Identity ────────────────────────────────────────────
#define AURA_VERSION        "1.0.0"
#define AURA_HOSTNAME       "aura"          // mDNS: http://aura.local
#define AURA_AP_SSID        "AURA-Setup"    // AP mode SSID
#define AURA_AP_PASSWORD    "aura1234"      // AP mode password (min 8 chars)

// ── LLM Provider (set via Web UI or here) ──────────────────────
// Options: "openai" | "gemini" | "anthropic" | "groq" | "openrouter" | "ollama"
#define DEFAULT_LLM_PROVIDER  "openai"
#define DEFAULT_LLM_MODEL     "gpt-4o-mini"  // cost-effective default

// ── Telegram ───────────────────────────────────────────────────
#define TELEGRAM_POLL_INTERVAL_MS  3000     // polling interval

// ── Sensor / Actuator defaults ─────────────────────────────────
#define MAX_SENSORS         8
#define MAX_ACTUATORS       8
#define SENSOR_READ_INTERVAL_MS  5000

// ── I2C Pins (ESP32 defaults, override per board) ──────────────
#ifdef AURA_ESP32
  #define AURA_I2C_SDA   21
  #define AURA_I2C_SCL   22
  #define AURA_ANALOG_PIN A0   // GPIO36
  #define AURA_DIGITAL_PIN 4
  #define AURA_PWM_PIN    5
#endif

#ifdef AURA_STM32
  #define AURA_I2C_SDA   PB7
  #define AURA_I2C_SCL   PB6
  #define AURA_ANALOG_PIN  PA0
  #define AURA_DIGITAL_PIN PB0
  #define AURA_PWM_PIN     PA8
#endif

// ── Storage ────────────────────────────────────────────────────
#define CONFIG_FILE     "/config.json"
#define AGENTS_FILE     "/agents.json"
#define SENSORS_FILE    "/sensors.json"

// ── Debug ──────────────────────────────────────────────────────
#define AURA_DEBUG      1   // 0=off 1=Serial 2=RemoteDebug
