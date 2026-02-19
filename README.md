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

## 📦 Adding New Sensor Libraries

Edit `src/main.cpp` and `include/AuraSensors.h`:

```cpp
// 1. Add to platformio.ini lib_deps:
//    adafruit/DHT sensor library@^1.4.4

// 2. In AuraSensors.h, uncomment:
#include <DHT.h>
DHT dht(4, DHT22);

// 3. In readSensor(), add case:
} else if (strcmp(s.type, "dht22") == 0) {
  float temp = dht.readTemperature();
  s.last_value = temp;
  snprintf(s.last_str, sizeof(s.last_str), "%.1f", temp);
}
```

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

### 2. Add AHT10 driver to `include/AuraSensors.h`

At the top of the file, after the existing includes:

```cpp
#include <Adafruit_AHTX0.h>
Adafruit_AHTX0 aht;
bool ahtReady = false;
```

In the `begin()` or `initPins()` method, initialize the sensor:

```cpp
ahtReady = aht.begin();
if (!ahtReady) Serial.println("[Sensors] AHT10 not found");
```

In the `readSensor()` method, add a case for type `"aht10"`:

```cpp
} else if (strcmp(s.type, "aht10_temp") == 0) {
  if (ahtReady) {
    sensors_event_t hum, temp;
    aht.getEvent(&hum, &temp);
    s.last_value = temp.temperature;
    snprintf(s.last_str, sizeof(s.last_str), "%.1f", temp.temperature);
  }
} else if (strcmp(s.type, "aht10_hum") == 0) {
  if (ahtReady) {
    sensors_event_t hum, temp;
    aht.getEvent(&hum, &temp);
    s.last_value = hum.relative_humidity;
    snprintf(s.last_str, sizeof(s.last_str), "%.1f", hum.relative_humidity);
  }
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

## 🗂 Project Structure

```
AURA/
├── src/
│   └── main.cpp              # Entry point
├── include/
│   ├── AuraConfig.h          # Constants & pin definitions
│   ├── AuraStorage.h         # SPIFFS config persistence
│   ├── AuraSensors.h         # Sensor & actuator manager
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
- [ ] WhatsApp integration (via WhatsApp Business API)
- [ ] MQTT publish/subscribe
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
