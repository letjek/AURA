# 🤖 AURA — Autonomous Universal Reactive Agent

> An intelligent IoT agent framework for ESP32 / STM32 microcontrollers.  
> Control sensors & actuators with natural language. Chat via Telegram.  
> Powered by any online LLM.

---

## 💡 Why AURA?

Most IoT projects follow the same rigid pattern: sensors feed data into hardcoded rules, and actuators respond to fixed thresholds. Want to change the logic? Recompile. Want to ask "why is the fan running?" — you can't, not without a separate app.

AURA was built around a different idea: **what if the microcontroller itself could think?**

Instead of programming rules, you describe your setup — sensors, actuators, goals — and let a language model handle the reasoning. AURA continuously reads sensors, builds a context summary, and sends it to an LLM on every interaction. The model decides what to do, and AURA executes it.

This means:
- **No hardcoded thresholds** — the LLM reasons from context, not fixed if/else chains
- **Natural language control** — talk to your hardware like you'd talk to a person
- **Explainable decisions** — ask "why did you turn on the fan?" and get a real answer
- **Rapid reconfiguration** — change behavior by editing a system prompt, not firmware

The tradeoff is latency and API cost on each LLM call. For most IoT use cases — environmental monitoring, home automation, lab equipment — this is a perfectly acceptable trade for dramatically reduced firmware complexity.

---

## ✨ Features

| Feature | Details |
|---------|---------|
| **Multi-LLM** | OpenAI, Gemini, Anthropic (Claude), Groq, OpenRouter, Ollama |
| **Sensors** | Analog, Digital, I2C, Voltage, DHT11/22, DS18B20, custom |
| **Actuators** | Digital ON/OFF, PWM (LED dimming, motor speed), I2C |
| **Connectors** | Any GPIO, I2C (SDA/SCL), Analog (ADC), PWM |
| **Web UI** | Full config & live dashboard at `http://aura.local` |
| **Telegram** | Natural language control + built-in commands |
| **PlatformIO** | ESP32, ESP32-S3, STM32 Blue Pill |
| **AP Mode** | Fallback WiFi setup portal when no credentials stored |

---

## 🚀 Quick Start

### 1. Clone & Open in PlatformIO

```bash
git clone https://github.com/letjek/AURA.git
cd AURA
# Open in VS Code with PlatformIO extension
```

### 2. Select Board

Edit `platformio.ini` or use PlatformIO's "Env" switcher:

| Board | env name |
|-------|----------|
| ESP32 DevKit v1 | `esp32` |
| ESP32-S3 | `esp32s3` |
| STM32 Blue Pill + ESP8266 AT | `stm32bluepill` |

### 3. Flash

```bash
pio run --target upload --environment esp32
```

### 4. First Boot Setup

1. AURA starts in **AP mode** — connect to WiFi `AURA-Setup` (password: `aura1234`)
2. Open browser → `http://192.168.4.1`
3. Go to **📶 WiFi** tab → enter your WiFi credentials → Save & Restart
4. AURA connects to your network — now available at `http://aura.local`

---

## 🔧 Web Interface

Open `http://aura.local` in any browser on your local network.

### Tabs

| Tab | Purpose |
|-----|---------|
| 📊 Dashboard | Live sensor values + actuator controls |
| 💬 Chat | Talk to AURA directly from browser |
| 🔌 I/O Config | Configure sensors & actuators |
| 🤖 LLM | API key, model, system prompt |
| ✈️ Telegram | Bot token + chat ID |
| 📶 WiFi | Network credentials |
| ⚙️ System | Memory, uptime, restart |

---

## 🔌 Wiring / Connector Guide

### ESP32 Default Pins

| Function | Pin |
|----------|-----|
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |
| Analog Input | GPIO 36 (A0) |
| Digital I/O | GPIO 4 |
| PWM Output | GPIO 5 |

You can configure any pin per sensor/actuator in the Web UI.

### Supported Sensor Types

| Type | Wiring |
|------|--------|
| `analog` | Signal → ADC pin (3.3V max!) |
| `digital` | Signal → any GPIO |
| `voltage` | Signal → ADC pin, auto-converts to volts |
| `i2c_raw` | SDA → GPIO21, SCL → GPIO22 |
| `dht11/dht22` | Data → any GPIO (enable DHT lib) |
| `ds18b20` | Data → any GPIO (enable OneWire lib) |
| `mock` | No hardware needed (testing) |

### Supported Actuator Types

| Type | Wiring |
|------|--------|
| `digital` | GPIO → Relay / LED / transistor |
| `pwm` | GPIO → LED / motor driver (0–255) |

---

## 🤖 LLM Providers

| Provider | Free Tier | Speed | Notes |
|----------|-----------|-------|-------|
| **Groq** | ✅ Yes | ⚡ Very Fast | Best for real-time IoT |
| **OpenAI** | ❌ Paid | Fast | gpt-4o-mini is cheap |
| **Gemini** | ✅ Yes | Fast | gemini-1.5-flash |
| **Anthropic** | ❌ Paid | Fast | claude-haiku is affordable |
| **OpenRouter** | ✅ Free models | Varies | 200+ models |
| **Ollama** | ✅ Local | Depends on HW | No internet needed |

> **Recommended for IoT:** Groq (free, ultra-fast) or Ollama (local, no API cost)

---

## ✈️ Telegram Setup

1. Open Telegram → search `@BotFather`
2. Send `/newbot` → follow prompts → copy the **token**
3. In AURA Web UI → Telegram tab → paste token → Save
4. Send `/start` to your bot
5. Get your Chat ID: visit `https://api.telegram.org/bot<TOKEN>/getUpdates`

### Telegram Commands

| Command | Action |
|---------|--------|
| `/start` or `/help` | Show available commands |
| `/status` | Current sensor readings |
| `/clear` | Clear conversation history |
| `/reset` | Restart AURA |
| Any message | Sent to LLM for AI response |

### LLM Actuator Control via Telegram

AURA parses JSON action blocks from LLM responses:

```
You: "Turn on the LED and set the fan to 50%"
AURA: Sure! I'm turning on the LED and setting the fan speed to 50%.
      {"actions":[{"name":"LED","state":true},{"name":"Fan","pwm":128}]}
```

---

## 📦 Adding a Custom Sensor

AURA uses a **plugin file** (`src/custom.cpp`) so you never need to edit the core headers. All custom sensor types, libraries, and init code go in one place.

### How it works

`custom.cpp` spawns a FreeRTOS task (`customSensorTask`) that runs alongside the main loop. It reads your custom sensor and writes results directly into `Storage.sensors[]` — `buildContextForLLM()` picks them up automatically.

### Steps

**1. Name your sensor type in the Web UI**

Go to `http://aura.local` → **I/O Config** → add a sensor slot and set its **Type** to your chosen type string (e.g. `aht10_temp`). The name is arbitrary — you match it in code.

**2. Add the library to `platformio.ini`**

```ini
lib_deps =
  ; ... existing deps ...
  adafruit/Adafruit AHTX0@^2.0.5   ; ← add your library here
```

**3. Edit `src/custom.cpp`**

```cpp
// ① Uncomment (or add) the include at the top
#include <Adafruit_AHTX0.h>

// ② Declare the driver instance (file-scope, before the task function)
static Adafruit_AHTX0 _aht;
static bool           _ahtReady = false;

// ③ In customSensorTask — one-time init block (runs once before the loop)
_ahtReady = _aht.begin();
Serial.println(_ahtReady ? "[Custom] AHT10 ready" : "[Custom] AHT10 not found");

// ④ In the read loop — match your type string and write into the slot
if (strcmp(s.type, "aht10_temp") == 0 && _ahtReady) {
  sensors_event_t h, t;
  _aht.getEvent(&h, &t);
  s.last_value = t.temperature;
  snprintf(s.last_str, sizeof(s.last_str), "%.1f", t.temperature);
}
else if (strcmp(s.type, "aht10_hum") == 0 && _ahtReady) {
  sensors_event_t h, t;
  _aht.getEvent(&h, &t);
  s.last_value = h.relative_humidity;
  snprintf(s.last_str, sizeof(s.last_str), "%.1f", h.relative_humidity);
}
```

**4. Flash**

```bash
pio run --target upload --environment esp32
```

No changes to `main.cpp` or `AuraSensors.h` needed.

---

## 🌡️ Example: AHT10 + Relay (Temperature & Humidity Automation)

This example wires an **AHT10** I2C temperature/humidity sensor to a **relay** that controls a fan or humidifier. The LLM decides when to switch the relay based on live readings.

### Wiring

```
AHT10           ESP32
──────────────────────
VCC  ──────────  3.3V
GND  ──────────  GND
SDA  ──────────  GPIO 21
SCL  ──────────  GPIO 22

Relay module    ESP32
──────────────────────
VCC  ──────────  5V (or 3.3V depending on module)
GND  ──────────  GND
IN   ──────────  GPIO 26
```

### 1. Add library to `platformio.ini`

```ini
lib_deps =
  me-no-dev/ESPAsyncWebServer@^1.2.3
  me-no-dev/AsyncTCP@^1.1.1
  ArduinoJson@^6.21.3
  knolleary/PubSubClient@^2.8
  Wire
  adafruit/Adafruit AHTX0@^2.0.5      ; ← add this
```

### 2. Edit `src/custom.cpp`

Uncomment the relevant lines (the file ships with AHT10 as the reference example):

```cpp
#include <Adafruit_AHTX0.h>

static Adafruit_AHTX0 _aht;
static bool           _ahtReady = false;
```

In the one-time init block inside `customSensorTask`:

```cpp
_ahtReady = _aht.begin();
if (_ahtReady) Serial.println("[Custom] AHT10 ready");
else           Serial.println("[Custom] AHT10 not found");
```

In the read loop:

```cpp
if (strcmp(s.type, "aht10_temp") == 0 && _ahtReady) {
  sensors_event_t h, t;
  _aht.getEvent(&h, &t);
  s.last_value = t.temperature;
  snprintf(s.last_str, sizeof(s.last_str), "%.1f", t.temperature);
}
else if (strcmp(s.type, "aht10_hum") == 0 && _ahtReady) {
  sensors_event_t h, t;
  _aht.getEvent(&h, &t);
  s.last_value = h.relative_humidity;
  snprintf(s.last_str, sizeof(s.last_str), "%.1f", h.relative_humidity);
}
```

### 3. Configure via Web UI

Go to `http://aura.local` → **I/O Config** tab and set up:

**Sensors:**

| Name | Type | Pin | Unit |
|------|------|-----|------|
| Temperature | `aht10_temp` | 21 (I2C) | °C |
| Humidity | `aht10_hum` | 21 (I2C) | % |

**Actuator:**

| Name | Type | Pin |
|------|------|-----|
| Relay | `digital` | 26 |

### 4. Set a system prompt in the LLM tab

```
You are AURA, an IoT controller managing a climate relay.

Rules:
- Turn ON the Relay if temperature > 30°C OR humidity > 70%
- Turn OFF the Relay if temperature < 26°C AND humidity < 60%
- In ambiguous conditions, keep the current state

Always respond with a brief explanation and, if you want to change
the relay state, include a JSON action block like:
{"actions":[{"name":"Relay","state":true}]}
```

### 5. How it works at runtime

Every time AURA receives a message or Telegram poll, it reads all sensors and builds a context block like:

```
Sensor readings:
- Temperature: 32.4 °C
- Humidity: 74.2 %

Actuator states:
- Relay: OFF
```

This is sent to the LLM alongside your message. The LLM reasons against the system prompt rules and replies, for example:

```
Temperature is 32.4°C (above 30°C threshold) and humidity is 74.2%
(above 70% threshold). Turning on the Relay.
{"actions":[{"name":"Relay","state":true}]}
```

AURA parses the JSON block and sets GPIO 26 HIGH — relay on.

You can also control it conversationally via Telegram:

```
You:  "What's the temperature and is the fan running?"
AURA: "Temperature is 32.4°C and humidity is 74.2%. The relay
       (fan) is currently ON — conditions exceeded the threshold."

You:  "Turn off the relay, I'll open a window instead."
AURA: "Understood. Turning off the relay."
      {"actions":[{"name":"Relay","state":false}]}
```

---

## 🔗 Multi-Device MQTT

Multiple AURA nodes on the same network can share sensor readings and control each other's actuators through any MQTT broker (Mosquitto, HiveMQ, etc.). No cloud required — a Raspberry Pi running Mosquitto on the same LAN works perfectly.

### Topic Scheme

| Topic | Direction | Payload |
|-------|-----------|---------|
| `aura/{chipId}/sensors/{name}` | Device publishes | Raw reading string |
| `aura/{chipId}/cmd/{name}` | Device receives | `{"state":true}` or `{"state":true,"pwm":128}` |
| `aura/{chipId}/status` | Device publishes | `{"ip":"...","heap":...,"uptime":...}` |

Each device's `chipId` is `aura-` followed by the lower 32 bits of its MAC address (e.g. `aura-1a2b3c4d`). It is printed to serial on boot:

```
[MQTT] Started — device id: aura-1a2b3c4d
```

You can also see it live on the `aura/+/status` topic.

### Setup

**1. Configure the broker in the Web UI**

Go to `http://aura.local` → **⚙️ System** tab → enter your broker host and port (default `1883`) → Save & Restart.

**2. Enable MQTT in `custom.cpp`**

`custom.cpp` already calls `MQTT.begin()` on WiFi connect — nothing extra needed. Every local sensor is automatically published at the `SENSOR_READ_INTERVAL_MS` rate.

### Consuming a Remote Sensor

Add a sensor slot on the *receiving* node:

| Field | Value |
|-------|-------|
| Name | `{remoteChipId}/{sensorName}` (e.g. `aura-1a2b3c4d/Temperature`) |
| Type | `mqtt_remote` |
| Unit | whatever the remote publishes |

When the remote node publishes `aura/aura-1a2b3c4d/sensors/Temperature`, the value is written into that slot automatically and appears in the LLM context and Web UI dashboard.

### Controlling a Remote Actuator

Add an actuator slot on the *controlling* node:

| Field | Value |
|-------|-------|
| Name | `{remoteChipId}/{actuatorName}` (e.g. `aura-1a2b3c4d/Fan`) |
| Type | `mqtt` |

When the LLM (or Web UI) calls `setActuator("aura-1a2b3c4d/Fan", true)`, the MQTT task detects the state change and publishes:

```
aura/aura-1a2b3c4d/cmd/Fan  →  {"state":true}
```

The remote node receives it and activates its local `Fan` actuator.

### Example: Two Nodes, Shared Climate Control

```
Node A (sensor node)                Node B (actuator node)
─────────────────────               ──────────────────────
Sensor: Temperature  aht10_temp     Actuator: Fan  digital  GPIO26
Sensor: Humidity     aht10_hum      Actuator: Pump digital  GPIO27

                        MQTT Broker
                       ─────────────
              aura/aura-AAAA/sensors/Temperature  →  Node B reads as mqtt_remote
              aura/aura-AAAA/sensors/Humidity     →  Node B reads as mqtt_remote
              aura/aura-BBBB/cmd/Fan              ←  Node B controls locally
```

On Node B, configure:

**Sensors (remote):**

| Name | Type | Unit |
|------|------|------|
| `aura-AAAA/Temperature` | `mqtt_remote` | °C |
| `aura-AAAA/Humidity` | `mqtt_remote` | % |

**Actuators (local):**

| Name | Type | Pin |
|------|------|-----|
| Fan | `digital` | 26 |
| Pump | `digital` | 27 |

Node B's LLM context will include the remote sensor values and can control the local actuators in response.

---

## 🗂 Project Structure

```
AURA/
├── src/
│   ├── main.cpp              # Entry point — do not edit for custom sensors
│   └── custom.cpp            # Plugin file — add custom sensors & MQTT here
├── include/
│   ├── AuraConfig.h          # Constants & pin definitions
│   ├── AuraStorage.h         # SPIFFS config persistence
│   ├── AuraSensors.h         # Sensor & actuator manager (built-in types)
│   ├── AuraMQTT.h            # Multi-device MQTT bridge
│   ├── AuraLLM.h             # Multi-provider LLM client
│   ├── AuraTelegram.h        # Telegram bot
│   ├── AuraWiFi.h            # WiFi + AP captive portal
│   ├── AuraWebServer.h       # Async web server + REST API
│   └── AuraHTML.h            # Embedded web UI (single file)
├── platformio.ini            # Board configurations
└── README.md
```

---

## 🔮 Roadmap

### Features
- [x] MQTT publish/subscribe (multi-device sensor sharing & remote actuator control)
- [ ] WhatsApp integration (via WhatsApp Business API)
- [ ] Voice commands (I2S microphone)
- [ ] OTA firmware updates from Web UI
- [ ] BLE configuration (no WiFi needed for setup)
- [ ] Rule engine (if temp > 30 → turn on fan)
- [ ] More sensor libraries (BMP280, SHT31, VEML7700)
- [ ] Dashboard charts (historical data)

### 🔒 Security Hardening
- [ ] **Web UI authentication** — HTTP Basic Auth or token-based login to protect the config panel from unauthorized access on the local network
- [ ] **HTTPS / TLS** — serve the web interface over TLS using a self-signed cert stored in SPIFFS, preventing plaintext credential exposure
- [ ] **MQTT TLS** — encrypted broker connection with client certificate support (`PubSubClient` + `WiFiClientSecure`)
- [ ] **API key storage encryption** — encrypt LLM API keys and Telegram tokens at rest in SPIFFS using AES (ESP32 hardware accelerated)
- [ ] **Prompt injection hardening** — sanitize and length-limit incoming Telegram/chat messages before they are forwarded to the LLM, preventing prompt manipulation attacks
- [ ] **LLM response validation** — verify that action JSON emitted by the LLM only targets declared actuators and valid pin states before execution
- [ ] **Rate limiting** — cap Telegram and web API requests per time window to prevent abuse or unintentional command flooding
- [ ] **Actuator safety guards** — configurable min/max bounds per actuator (e.g. PWM never exceeds a safe duty cycle) that the firmware enforces regardless of LLM output
- [ ] **Secure AP mode** — replace the hardcoded `aura1234` AP password with a device-unique default derived from chip ID
- [ ] **Audit log** — append-only SPIFFS log of actuator state changes with timestamps, source (Telegram / Web / LLM), and triggering message

---

## 🤝 Contributing

Contributions are welcome! Here's how:

1. Fork the repo on GitHub: [github.com/letjek/AURA](https://github.com/letjek/AURA)
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Commit your changes: `git commit -m "Add my feature"`
4. Push and open a Pull Request

Ideas for contributions: new sensor drivers, LLM provider integrations, UI improvements, MQTT support, OTA updates — see the Roadmap above.

---

## 📄 License

MIT — do whatever you want with it.

---

*Built with ❤️ for the maker community. Flash it, extend it, make it yours.*
*Maintained by [@letjek](https://github.com/letjek)*
