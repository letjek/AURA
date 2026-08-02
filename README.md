# AURA — on-device intelligence for microcontrollers

*What if the microcontroller itself could think?*

AURA turns an ESP32 into a small agent that answers natural prompts — English
or Indonesian — **entirely on-device**. No API keys, no cloud round-trip, no
hallucinated facts: AURA answers from curated, swappable knowledge models and
declines everything outside its domain.

```
You:  nyalakan pin 5
AURA: ⚙ GPIO 5 → ON (HIGH)

You:  cek suhu sekarang
AURA: ⚙ current temperature: 36.4 °C (internal chip sensor)

You:  what is a plc
AURA: 📚 PLC — Programmable Logic Controller
      A PLC is a ruggedized industrial computer that controls machines...

You:  what is the meaning of life
AURA: I must decline — my loaded knowledge model "automation v3"
      doesn't cover that topic.
```

## Architecture: two models + a wasm runtime

| Layer | What | Swappable? |
|---|---|---|
| **PRIMARY model** | Greetings + hardware integration: GPIO actuators, digital/analog sensors, PWM, temperature, I2C discovery. Enforces a safe-pin allowlist. | No — compiled into firmware, survives every model swap |
| **ADDITIONAL model** | A [TOON](https://toonformat.dev) knowledge pack in LittleFS: weighted-keyword retrieval, decline threshold, per-model `temperature` for varied phrasing. | Yes — web upload, URL fetch, or `data/` folder |
| **wasm3 runtime** | Runs portable WebAssembly modules on-chip (`fib 27`, or upload your own `.wasm`). | Yes — modules are just files |

## Quick start (PlatformIO)

```ini
; platformio.ini
[env:esp32s3]
platform = espressif32
board = lolin_s3_mini
framework = arduino
board_build.filesystem = littlefs
board_build.partitions = huge_app.csv
lib_deps = https://github.com/letjek/AURA.git
```

```cpp
#include <AURA.h>

void setup() {
  AuraClass::Config cfg;
  cfg.ssid = "YourWiFi";     // optional — omit for AP-only mode
  cfg.pass = "YourPassword";
  AURA.begin(cfg);
}

void loop() { AURA.loop(); }
```

Flash, then open **http://192.168.4.1/** (join the `AURA-Device` hotspot,
password `aura1234`) or **http://aura.local/** on your LAN. Full working
project: [`examples/ESP32S3-SuperMini`](examples/ESP32S3-SuperMini).

You can also ask AURA from code: `String a = AURA.ask("cek suhu");`

## Prompt reference

**Primary model (always available)**
`hw` · `pin 5 on|off` · `pin 5 read` · `adc 4` · `pwm 5 128` ·
`led on|off` · `temp` / `cek suhu` · `i2c scan` — plus greetings
(`halo`, `hi`, `apa kabar`) and Indonesian verbs (`nyalakan`, `matikan`,
`baca`, `berapa`).

**Knowledge model** — any other prompt is matched against the loaded pack;
below-threshold prompts are declined. `model` shows what is loaded.

**Utility** — `status`, `fib <n>` (WebAssembly on-chip), `echo <text>`,
`help`.

## Knowledge models (TOON)

```toon
name: automation
version: 3
author: Professor Claude
threshold: 2
temperature: 0.7
entries[23]{t,k,a}:
  "PLC — Programmable Logic Controller","plc:2 programmable:1 logic:1","A PLC is..."
```

- `k` is `keyword:weight ...`; a prompt's matched weights are summed and
  compared to `threshold` — below it, AURA declines.
- `temperature` (0–1) varies the *presentation* (openings, footers, related-
  topic hints). Facts never vary.
- Install: web page → *Install & reboot*, paste a raw URL → *Fetch & reboot*,
  or drop the file at `data/model.toon` and `pio run -t uploadfs`.
- Sample pack: [`extras/models/automation.toon`](extras/models/automation.toon).

## HTTP API

| Endpoint | Method | Purpose |
|---|---|---|
| `/api/prompt` | POST (text) | ask AURA |
| `/api/model` | GET / POST | download / install knowledge model |
| `/api/model/fetch` | POST `url=` | device downloads a model itself |
| `/api/model/info` | GET | one-line model summary |
| `/api/wasm` | POST (multipart) | run an uploaded `.wasm` on-chip |

## Compatibility

ESP32-class devices with WiFi, ≥4 MB flash and a LittleFS partition.
Reference board: **ESP32-S3 Super Mini** (safe-pin list is S3-based).
Small AVR boards (Arduino Nano/Uno) are **not** supported — not enough
RAM/flash for the wasm runtime and web stack.

## Ecosystem (roadmap)

- **AURA-drivers** — public registry + CI that compiles sensor/actuator
  integrations (AHT10, BMP280, …) into downloadable `.wasm` driver modules.
  Firmware exposes `i2c/gpio/delay` host functions; drivers export
  `init`/`read`. I2C/SPI sensors fit wasm; timing-critical protocols (DHT11
  1-wire) stay in the primary model.
- **AURA-knowledge** — public registry of TOON knowledge packs.
- Public and private entries: private models/drivers encrypted per-creator,
  unlocked on-device with a key from the creator.
- The original cloud-LLM prototype (multi-LLM, Telegram, MQTT) lives in
  [`extras/legacy-prototype`](extras/legacy-prototype) and is the base for a
  future optional cloud-reasoning mode.

## License

MIT
