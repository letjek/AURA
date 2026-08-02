// AURA — on-device intelligence for microcontrollers
// Implementation. See AURA.h for the public API.

#include "AURA.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <vector>
#include <wasm3.h>

static const char *MODEL_PATH = "/model.toon";
static const size_t MAX_UPLOAD_SIZE = 96 * 1024;
static const uint32_t WASM_STACK_BYTES = 16 * 1024;

// wasm3 + TLS need more native stack than the 8 KB Arduino default
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

static AuraClass::Config gCfg;
static WebServer server(80);

static std::vector<uint8_t> uploadBuf;
static bool uploadTooBig = false;

AuraClass AURA;

// ---------------------------------------------------------------- wasm ------

// (module (func (export "fib") (param i32) (result i32) ...)) — classic
// recursive fib, hand-checked wasm binary, 62 bytes.
static const uint8_t FIB_WASM[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,             // \0asm v1
    0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,             // type (i32)->i32
    0x03, 0x02, 0x01, 0x00,                                     // func 0: type 0
    0x07, 0x07, 0x01, 0x03, 0x66, 0x69, 0x62, 0x00, 0x00,       // export "fib"
    0x0a, 0x1f, 0x01, 0x1d, 0x00,                               // code section
    0x20, 0x00, 0x41, 0x02, 0x49, 0x04, 0x40, 0x20, 0x00, 0x0f, 0x0b,
    0x20, 0x00, 0x41, 0x02, 0x6b, 0x10, 0x00,
    0x20, 0x00, 0x41, 0x01, 0x6b, 0x10, 0x00,
    0x6a, 0x0f, 0x0b};

static String runWasmModule(const uint8_t *bytes, size_t len,
                            const char *funcName,
                            uint32_t argc, const char **argv,
                            int64_t *elapsedUs = nullptr) {
  IM3Environment env = m3_NewEnvironment();
  if (!env) return "error: no memory for wasm environment";

  String out;
  IM3Runtime runtime = m3_NewRuntime(env, WASM_STACK_BYTES, NULL);
  IM3Module module = NULL;

  do {
    if (!runtime) {
      out = "error: no memory for wasm runtime";
      break;
    }
    M3Result res = m3_ParseModule(env, &module, bytes, len);
    if (res) {
      out = String("wasm parse error: ") + res;
      break;
    }
    res = m3_LoadModule(runtime, module);
    if (res) {
      out = String("wasm load error: ") + res;
      m3_FreeModule(module);
      break;
    }

    IM3Function f;
    res = m3_FindFunction(&f, runtime, funcName);
    if (res) {
      out = String("function '") + funcName + "' not found (" + res + ")";
      break;
    }

    uint32_t need = m3_GetArgCount(f);
    if (need != argc) {
      out = String("error: '") + funcName + "' expects " + need +
            " argument(s), got " + argc;
      break;
    }

    int64_t t0 = esp_timer_get_time();
    res = m3_CallArgv(f, argc, argv);
    if (elapsedUs) *elapsedUs = esp_timer_get_time() - t0;
    if (res) {
      out = String("wasm runtime error: ") + res;
      break;
    }

    if (m3_GetRetCount(f) == 0) {
      out = "(no return value)";
      break;
    }
    switch (m3_GetRetType(f, 0)) {
      case c_m3Type_i32: {
        int32_t v = 0;
        m3_GetResultsV(f, &v);
        out = String(v);
      } break;
      case c_m3Type_i64: {
        int64_t v = 0;
        m3_GetResultsV(f, &v);
        char b[24];
        snprintf(b, sizeof(b), "%lld", (long long)v);
        out = b;
      } break;
      case c_m3Type_f32: {
        float v = 0;
        m3_GetResultsV(f, &v);
        out = String(v, 6);
      } break;
      case c_m3Type_f64: {
        double v = 0;
        m3_GetResultsV(f, &v);
        out = String(v, 6);
      } break;
      default:
        out = "(unsupported return type)";
    }
  } while (0);

  if (runtime) m3_FreeRuntime(runtime);
  m3_FreeEnvironment(env);
  return out;
}

static String cmdFib(long n) {
  if (n < 0) n = 0;
  if (n > 30)
    return "fib is capped at n=30 here — the interpreter recursion gets slow. "
           "Try `fib 30`.";
  char nbuf[12];
  snprintf(nbuf, sizeof(nbuf), "%ld", n);
  const char *argv[1] = {nbuf};
  int64_t us = 0;
  String r = runWasmModule(FIB_WASM, sizeof(FIB_WASM), "fib", 1, argv, &us);
  char tbuf[48];
  snprintf(tbuf, sizeof(tbuf), "%.2f ms", us / 1000.0);
  return String("wasm3 » fib(") + n + ") = " + r +
         "\n(ran as WebAssembly on-chip in " + tbuf + ")";
}

// ------------------------------------------------- TOON knowledge model -----

struct ModelEntry {
  String t;
  String a;
  std::vector<std::pair<String, int>> k;
};

struct Model {
  String name, author, desc;
  int version = 0, threshold = 2;
  float temperature = 0.5f;  // 0 = always identical wording, 1 = max variety
  std::vector<ModelEntry> entries;
  bool ok = false;
};

static Model gModel;

// Factory-default TOON model, written to LittleFS on first boot.
static const char DEFAULT_MODEL[] PROGMEM = R"mdl(name: automation
version: 3
author: Professor Claude
description: Industrial and home automation fundamentals
threshold: 2
temperature: 0.7
entries[23]{t,k,a}:
  "Automation (general)","automation:2 automate:2 automated:2 automatic:1","Automation is the use of technology to run processes with minimal human intervention. Every automation system has the same skeleton: sensors measure the world, a controller runs logic, and actuators act on the world. It spans industrial control (factories, process plants) down to home and building automation."
  "PLC — Programmable Logic Controller","plc:2 programmable:1 logic:1 controller:1 siemens:1 allen:1 bradley:1 omron:1 mitsubishi:1","A PLC is a ruggedized industrial computer that controls machines. It runs a fast scan cycle: read all inputs, execute the user program (often ladder logic), write all outputs — typically every few milliseconds. Major vendors: Siemens, Allen-Bradley (Rockwell), Omron, Mitsubishi, Schneider."
  "SCADA — Supervisory Control and Data Acquisition","scada:2 supervisory:2 acquisition:1 telemetry:1","SCADA is the software layer that supervises an entire plant or distributed infrastructure. It gathers data from PLCs and RTUs, shows live process graphics, records trends and alarms, and lets operators send commands. It sits above the control layer — it supervises, while PLCs do the real-time control."
  "HMI — Human-Machine Interface","hmi:2 touchscreen:1 panel:1 interface:1 operator:1","An HMI is the screen or panel an operator uses to watch and command a machine — usually a touch panel wired to the PLC. It shows process values, states and alarms, and provides buttons and setpoint entry. Think of it as the machine's local dashboard, while SCADA is the plant-wide one."
  "Sensors","sensor:2 sensors:2 proximity:1 temperature:1 pressure:1 level:1 flow:1 encoder:1 measurement:1","A sensor converts a physical quantity — temperature, pressure, level, flow, position, presence — into an electrical signal a controller can read. Signals are digital (on/off, e.g. a proximity switch) or analog (commonly 4-20 mA current or 0-10 V voltage). 4-20 mA is loved industrially because a broken wire (0 mA) is instantly detectable."
  "Actuators","actuator:2 actuators:2 solenoid:1 valve:1 cylinder:1 servo:1 stepper:1","An actuator turns a control signal into physical action: electric motors (induction, servo, stepper), solenoid valves, pneumatic and hydraulic cylinders, heaters, relays. Controller decides, actuator does. Choosing one is a trade-off of force, speed, precision, and energy source available."
  "Relays & Contactors","relay:2 relays:2 contactor:2 coil:1 switching:1","A relay is an electrically operated switch: a small control signal energizes a coil that closes or opens contacts carrying a bigger load — isolating the logic side from the power side. A contactor is a heavy-duty relay built for motors and high currents, usually with arc suppression and auxiliary contacts."
  "VFD — Variable Frequency Drive","vfd:2 inverter:2 drive:1 frequency:1 motor:1 speed:1","A VFD (inverter drive) controls the speed and torque of an AC motor by varying the frequency and voltage it feeds the motor. Benefits: soft starting (no inrush slam), precise speed control, and big energy savings on fans and pumps, where power drops with the cube of speed."
  "PID Control","pid:2 proportional:1 integral:1 derivative:1 tuning:1 setpoint:1","PID is the workhorse feedback algorithm. It computes error = setpoint − measurement, and sums three terms: P reacts to the present error, I to accumulated past error (kills steady-state offset), D to the error's trend (damps overshoot). Tuning the three gains — by hand or methods like Ziegler-Nichols — is what makes a loop stable and fast."
  "Open-loop vs Closed-loop","loop:1 feedback:2 open:1 closed:1 thermostat:1","Open-loop control acts blindly with no feedback — a sprinkler on a timer waters even in the rain. Closed-loop control measures the result and corrects itself — a thermostat heats until the measured temperature reaches the setpoint. Feedback buys accuracy and disturbance rejection at the cost of sensors and tuning."
  "Ladder Logic & IEC 61131-3","ladder:2 rung:1 iec:1 61131:2 structured:1 fbd:2","Ladder logic is the classic graphical PLC language: rungs of contacts (conditions) and coils (outputs) that mimic old relay wiring diagrams, so electricians can read it. The IEC 61131-3 standard also defines Function Block Diagram (FBD), Structured Text (ST), Instruction List, and Sequential Function Charts (SFC)."
  "Modbus","modbus:2 rtu:2 rs485:2 485:1 registers:1 coils:1","Modbus is the simplest, most widespread industrial protocol: a client polls servers for coils (bits) and registers (16-bit words). Modbus RTU runs over RS-485 serial; Modbus TCP runs over Ethernet. It is unencrypted and unauthenticated — fine on a closed control network, never on the open internet."
  "MQTT","mqtt:2 broker:2 publish:1 subscribe:1 mosquitto:1 topic:1","MQTT is a lightweight publish/subscribe messaging protocol made for IoT. Devices publish messages to topics on a central broker (e.g. Mosquitto); anyone subscribed to the topic receives them — no device needs to know another's address. Three QoS levels (0, 1, 2) trade delivery guarantees against overhead. Perfect fit for ESP32 telemetry."
  "OPC UA","opc:2 ua:1 interoperability:1","OPC UA is the modern industrial interoperability standard: platform-independent, secure by design (encryption, certificates), and it carries not just values but a rich information model — data with meaning, units and structure. It is the common language for connecting the plant floor to MES/ERP/cloud (Industry 4.0)."
  "DCS vs PLC","dcs:2 distributed:1 process:1 batch:1","A DCS (Distributed Control System) is built for large continuous processes — chemical plants, refineries, power stations: thousands of analog loops, redundant controllers, one integrated engineering environment. PLC+SCADA suits discrete, machine-oriented automation. The line is blurry now, but the design philosophy differs: DCS is process-loop-centric, PLC is machine-logic-centric."
  "Home Automation","home:2 smart:1 zigbee:2 zwave:2 matter:2 thread:1 esphome:2 assistant:1","Home automation controls lights, climate, security and appliances. Radio protocols: WiFi, Zigbee, Z-Wave, and Matter/Thread (the new interoperability standard). A hub like Home Assistant ties brands together with local automations. The ESP32 is the DIY favorite — ESPHome turns one into a sensor/switch node with a YAML file."
  "ESP32","esp32:2 esp:1 espressif:2 microcontroller:1 mcu:1 arduino:1","The ESP32 is Espressif's WiFi+Bluetooth microcontroller family — the standard choice for connected automation nodes. The chip answering you right now is an ESP32-S3: dual Xtensa LX7 cores @ 240 MHz, 4 MB flash, 2 MB PSRAM, running FreeRTOS. Program it with Arduino, ESP-IDF, or MicroPython."
  "RTOS — Real-Time Operating System","rtos:2 freertos:2 task:1 scheduler:1 realtime:2","An RTOS schedules tasks with predictable, bounded timing — what matters is not raw speed but guaranteed deadlines. FreeRTOS (running on this very chip) provides preemptive priority scheduling, tasks, queues, semaphores and timers. Control loops get high priority; logging and UI get the leftovers."
  "Machine Safety","safety:2 estop:2 emergency:2 interlock:2 curtain:1 13849:1","Safety functions — emergency stops, door interlocks, light curtains — must work even when the normal control system fails, so they use safety-rated relays or safety PLCs with redundant, monitored circuits. Standards ISO 13849 and IEC 62061 define required performance levels. Rule one: an E-stop is hardwired-reliable, never just a bit in software."
  "Industrial Robots","robot:2 robots:2 robotic:2 cobot:2 arm:1 scara:2 welding:1","Industrial robots are programmable manipulators: articulated 6-axis arms (most common), SCARA (fast planar assembly), delta (high-speed picking), and cobots designed to work safely beside people. Their superpower is repeatability — returning to the same point within fractions of a millimeter, all shift, every shift."
  "Industry 4.0 / IIoT","industry:1 iiot:2 iot:1 digital:1 twin:2 predictive:2 maintenance:1","Industry 4.0 is manufacturing made data-driven: machines instrumented with IIoT sensors, data flowing to edge and cloud analytics, digital twins simulating the plant, and predictive maintenance replacing fix-when-broken. The technical foundation is connectivity (OPC UA, MQTT) plus analytics on top."
  "Pneumatics & Hydraulics","pneumatic:2 pneumatics:2 hydraulic:2 hydraulics:2 compressed:1 air:1","Both are fluid power. Pneumatics uses compressed air: fast, clean, cheap components, but limited force and springy motion — ideal for pick-and-place grippers. Hydraulics uses pressurized oil: enormous, stiff force (presses, excavators) at the cost of pumps, hoses and leak management."
  "WebAssembly on this chip","wasm:2 webassembly:2 wasm3:2","This device runs wasm3, a WebAssembly interpreter, so it can execute portable .wasm modules on-chip — try fib 27, or upload your own module in the panel below. WebAssembly gives you sandboxed, language-independent code you can hot-swap without reflashing firmware — the same idea as swapping my knowledge model."
)mdl";

// Split one TOON tabular row: fields are quoted (with "" escaping) or bare.
static int splitToonRow(const String &line, String *out, int maxF) {
  int n = 0;
  unsigned int i = 0;
  while (i < line.length() && n < maxF) {
    String field;
    while (i < line.length() && line[i] == ' ') i++;
    if (i < line.length() && line[i] == '"') {
      i++;
      while (i < line.length()) {
        char c = line[i];
        if (c == '"') {
          if (i + 1 < line.length() && line[i + 1] == '"') {
            field += '"';
            i += 2;
          } else {
            i++;
            break;
          }
        } else {
          field += c;
          i++;
        }
      }
      while (i < line.length() && line[i] != ',') i++;
      if (i < line.length()) i++;
    } else {
      while (i < line.length() && line[i] != ',') {
        field += line[i];
        i++;
      }
      if (i < line.length()) i++;
      field.trim();
    }
    out[n++] = field;
  }
  return n;
}

// keyword cell format: "kw:weight kw2:weight ..." (weight defaults to 1)
static void parseKeywordCell(const String &cell, ModelEntry &e) {
  int i = 0, len = cell.length();
  while (i < len) {
    while (i < len && cell[i] == ' ') i++;
    int j = i;
    while (j < len && cell[j] != ' ') j++;
    if (j > i) {
      String tok = cell.substring(i, j);
      int c = tok.indexOf(':');
      if (c > 0)
        e.k.push_back({tok.substring(0, c), tok.substring(c + 1).toInt()});
      else
        e.k.push_back({tok, 1});
    }
    i = j;
  }
}

static bool parseToon(const String &text, Model &m, String &err) {
  m = Model();
  int pos = 0, declared = -1;
  bool inEntries = false;
  while (pos < (int)text.length()) {
    int nl = text.indexOf('\n', pos);
    if (nl < 0) nl = text.length();
    String line = text.substring(pos, nl);
    pos = nl + 1;
    String trimmed = line;
    trimmed.trim();
    if (!trimmed.length() || trimmed.startsWith("#")) continue;
    if (!inEntries) {
      if (trimmed.startsWith("entries[")) {
        int rb = trimmed.indexOf(']');
        if (rb < 0) {
          err = "bad entries header";
          return false;
        }
        declared = trimmed.substring(8, rb).toInt();
        inEntries = true;
        continue;
      }
      int c = trimmed.indexOf(':');
      if (c < 0) continue;
      String key = trimmed.substring(0, c);
      key.trim();
      String val = trimmed.substring(c + 1);
      val.trim();
      if (val.startsWith("\"") && val.endsWith("\"") && val.length() >= 2)
        val = val.substring(1, val.length() - 1);
      if (key == "name") m.name = val;
      else if (key == "version") m.version = val.toInt();
      else if (key == "author") m.author = val;
      else if (key == "description") m.desc = val;
      else if (key == "threshold") m.threshold = val.toInt();
      else if (key == "temperature") m.temperature = val.toFloat();
    } else {
      String f[3];
      if (splitToonRow(trimmed, f, 3) != 3 || !f[0].length() ||
          !f[1].length() || !f[2].length()) {
        err = String("bad entry row near: ") + trimmed.substring(0, 40);
        return false;
      }
      ModelEntry e;
      e.t = f[0];
      e.a = f[2];
      parseKeywordCell(f[1], e);
      if (e.k.empty()) {
        err = String("entry has no keywords: ") + e.t;
        return false;
      }
      m.entries.push_back(e);
    }
  }
  if (!m.name.length()) {
    err = "missing name";
    return false;
  }
  if (m.entries.empty()) {
    err = "no entries";
    return false;
  }
  if (declared >= 0 && (int)m.entries.size() != declared) {
    err = String("entry count mismatch: declared ") + declared + ", found " +
          (int)m.entries.size();
    return false;
  }
  m.ok = true;
  return true;
}

static void ensureModelFile() {
  if (LittleFS.exists("/model.json"))
    LittleFS.remove("/model.json");  // migrate away from the old JSON era
  if (LittleFS.exists(MODEL_PATH)) return;
  Serial.println("[model] no model in FS — installing factory default");
  File f = LittleFS.open(MODEL_PATH, "w");
  if (!f) {
    Serial.println("[model] ERROR: cannot create model file");
    return;
  }
  f.print(FPSTR(DEFAULT_MODEL));
  f.close();
}

static void loadModel() {
  File f = LittleFS.open(MODEL_PATH, "r");
  if (!f) {
    Serial.println("[model] ERROR: model file missing");
    return;
  }
  String text = f.readString();
  f.close();
  String err;
  if (!parseToon(text, gModel, err)) {
    Serial.printf("[model] ERROR: %s\n", err.c_str());
    return;
  }
  Serial.printf("[model] loaded \"%s\" v%d — %u entries (TOON %u bytes), "
                "heap %u KB free\n",
                gModel.name.c_str(), gModel.version,
                (unsigned)gModel.entries.size(), (unsigned)text.length(),
                (unsigned)(ESP.getFreeHeap() / 1024));
}

// Tokenize into lowercase alphanumeric words
static int tokenizePrompt(const String &p, String *toks, int maxTok) {
  int n = 0;
  String cur;
  for (unsigned int i = 0; i <= p.length() && n < maxTok; i++) {
    char c = (i < p.length()) ? p[i] : ' ';
    if (isalnum((unsigned char)c)) {
      cur += (char)tolower((unsigned char)c);
    } else if (cur.length()) {
      toks[n++] = cur;
      cur = "";
    }
  }
  return n;
}

static bool tokenMatches(const String &tok, const String &key) {
  if (tok == key) return true;
  unsigned int kl = key.length();
  // light stemming: prefix match for words of 4+ chars
  if (kl >= 4 && tok.length() > kl && tok.startsWith(key)) return true;
  if (tok.length() >= 4 && kl > tok.length() && key.startsWith(tok))
    return true;
  return false;
}

static String modelTopics() {
  String out;
  for (auto &e : gModel.entries) {
    if (out.length()) out += ", ";
    out += e.t;
  }
  return out;
}

// "temperature" sampling: facts stay curated, presentation varies.
static uint32_t rnd(uint32_t n) { return esp_random() % n; }
static bool roll(float p) {
  if (p <= 0.0f) return false;
  return (esp_random() % 1000) < (uint32_t)(p * 1000.0f);
}

static String askModel(const String &prompt) {
  if (!gModel.ok)
    return "No knowledge model is loaded — install one in the Model panel "
           "below.";

  String toks[24];
  int nTok = tokenizePrompt(prompt, toks, 24);

  int bestScore = 0, secondScore = 0;
  const ModelEntry *best = nullptr, *second = nullptr;
  for (auto &e : gModel.entries) {
    int score = 0;
    for (auto &kv : e.k) {
      for (int i = 0; i < nTok; i++) {
        if (tokenMatches(toks[i], kv.first)) {
          score += kv.second;
          break;  // each keyword counts once
        }
      }
    }
    if (score > bestScore) {
      secondScore = bestScore;
      second = best;
      bestScore = score;
      best = &e;
    } else if (score > secondScore) {
      secondScore = score;
      second = &e;
    }
  }

  float T = gModel.temperature;
  Serial.printf("[model] query scored %d (threshold %d, temperature %.1f)\n",
                bestScore, gModel.threshold, T);

  if (!best || bestScore < gModel.threshold) {
    switch (roll(T) ? rnd(3) : 0) {
      case 1:
        return String("Hmm, that's outside my domain — \"") + gModel.name +
               " v" + gModel.version +
               "\" has nothing reliable on it, and I'd rather decline than "
               "guess.\n\nAsk me about: " + modelTopics() + ".";
      case 2:
        return String("I'd love to help, but my loaded model \"") +
               gModel.name +
               "\" doesn't cover that — and a good professor never "
               "improvises facts.\n\nTopics I do know: " + modelTopics() +
               ".\n\n(You can install another model in the Model panel "
               "below.)";
      default:
        return String("I must decline — my loaded knowledge model \"") +
               gModel.name + " v" + gModel.version +
               "\" doesn't cover that topic.\n\nAsk me about: " +
               modelTopics() +
               ".\n\n(Or install a different model in the Model panel below.)";
    }
  }

  String body;
  switch (roll(T) ? rnd(4) : 0) {
    case 1:
      body = String("Let me explain ") + best->t + ".\n\n" + best->a;
      break;
    case 2:
      body = best->a + "\n\n(topic: " + best->t + ")";
      break;
    case 3:
      body = String("Good question — this is about ") + best->t + ":\n\n" +
             best->a;
      break;
    default:
      body = String("📚 ") + best->t + "\n\n" + best->a;
  }
  if (second && secondScore >= gModel.threshold && roll(T * 0.6f))
    body += String("\n\nRelated topic in my model: ") + second->t +
            " — ask me about it.";
  switch (roll(T) ? rnd(3) : 0) {
    case 1:
      body += String("\n\n— ") + gModel.name + " v" + gModel.version;
      break;
    case 2:
      break;  // sometimes no footer at all
    default:
      body += String("\n\n— ") + gModel.name + " model v" + gModel.version +
              ", score " + bestScore;
  }
  return body;
}

static String cmdModelInfo() {
  String out = "PRIMARY model (built-in, irreplaceable): greetings + hardware "
               "integration\n  GPIO / ADC / PWM / temperature / I2C — type "
               "`hw`\n\n";
  if (!gModel.ok) return out + "ADDITIONAL model: none loaded";
  File f = LittleFS.open(MODEL_PATH, "r");
  size_t sz = f ? f.size() : 0;
  if (f) f.close();
  return out + "ADDITIONAL model (swappable): " + gModel.name + " v" +
         gModel.version + "\n  author: " + gModel.author + "\n  " +
         gModel.desc + "\n  entries: " + (int)gModel.entries.size() +
         " | TOON file: " + (int)sz + " bytes | threshold: " +
         gModel.threshold + "\n\ntopics: " + modelTopics();
}

// -------------------------------------------- PRIMARY model (built-in) ------
// Greetings + hardware integration. Compiled into firmware: survives every
// knowledge-model swap. Generic over whatever sensors/actuators are wired.

static const uint8_t SAFE_PINS[] = {1,  2,  4,  5,  6,  7,  8,  9,  10,
                                    11, 12, 13, 14, 15, 16, 17, 18, 21,
                                    38, 39, 40, 41, 42, 47, 48};

// 0 = untouched, 1 = output, 2 = input, 3 = pwm
static uint8_t gPinMode[49] = {0};

static bool pinAllowed(int p) {
  for (uint8_t sp : SAFE_PINS)
    if (sp == p) return true;
  return false;
}

static String pinStateStr(int p) {
  const char *m[] = {"untouched", "OUTPUT", "INPUT", "PWM"};
  String s = String("GPIO ") + p + " [" + m[gPinMode[p]] + "]";
  if (gPinMode[p] == 1 || gPinMode[p] == 2)
    s += String(" = ") + (digitalRead(p) ? "HIGH" : "LOW");
  return s;
}

// Live temperature: internal chip sensor now; extend here when an external
// sensor (I2C/analog) is wired so answers use its data instead.
static String readTemperature() {
  return String("⚙ current temperature: ") + String(temperatureRead(), 1) +
         " °C (internal chip sensor)\nNo external temperature sensor detected "
         "yet — wire one via I2C and run `i2c scan` to integrate it.";
}

static String cmdHw() {
  String out = "⚙ PRIMARY hardware model (built-in)\n";
  out += String("chip temperature: ") + String(temperatureRead(), 1) + " °C\n";
  out += String("safe GPIOs: 1 2 4-18 21 38-42 47 48 (LED = ") + gCfg.ledPin +
         ")\n";
  out += "analog sensors (adc): GPIO 1-10, 12-bit raw + mV\n";
  out += "actuators: pin N on|off, pwm N 0-255\n";
  out += String("i2c scan: SDA=") + SDA + " SCL=" + SCL +
         " (or: i2c scan <sda> <scl>)\n";
  String cfg;
  for (uint8_t p : SAFE_PINS)
    if (gPinMode[p]) cfg += "  " + pinStateStr(p) + "\n";
  out += cfg.length() ? "configured pins:\n" + cfg
                      : "no pins configured yet this session";
  return out;
}

static String cmdI2cScan(int sda, int scl) {
  if (sda >= 0 && scl >= 0) {
    if (!pinAllowed(sda) || !pinAllowed(scl))
      return "i2c: those pins are not in the safe list";
    Wire.end();
    Wire.begin(sda, scl);
  } else {
    Wire.begin();
    sda = SDA;
    scl = SCL;
  }
  String found;
  int n = 0;
  for (uint8_t a = 0x08; a <= 0x77; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      char b[8];
      snprintf(b, sizeof(b), "0x%02X", a);
      if (n++) found += ", ";
      found += b;
    }
  }
  String out = String("⚙ I2C scan on SDA=") + sda + " SCL=" + scl + ": ";
  if (n == 0)
    out += "no devices found — check wiring (SDA/SCL swapped? pull-ups?)";
  else
    out += String(n) + " device(s): " + found;
  return out;
}

// Returns non-empty reply if the prompt belongs to the primary model.
// English and Indonesian are understood.
static String tryPrimary(const String &p) {
  String toks[24];
  int n = tokenizePrompt(p, toks, 24);
  if (n == 0) return "";

  // --- greetings ---
  bool greet = false, indo = false;
  for (int i = 0; i < n && n <= 5; i++) {
    String &t = toks[i];
    if (t == "hello" || t == "hi" || t == "hey") greet = true;
    if (t == "halo" || t == "hai" || t == "hei") { greet = true; indo = true; }
    if (t == "kabar" || t == "pagi" || t == "siang" || t == "sore" ||
        t == "malam") { greet = true; indo = true; }
  }
  if (greet) {
    String kn = gModel.ok ? gModel.name : (indo ? "(kosong)" : "(none)");
    uint32_t v = rnd(3);
    if (indo) {
      if (v == 1)
        return String("Hai juga! 👋 Saya AURA. Ada yang bisa saya bantu? "
                      "Coba `hw` untuk hardware, `cek suhu` untuk sensor, "
                      "atau tanya soal \"") + kn +
               "\" — ketik `model` untuk daftar topik.";
      if (v == 2)
        return String("Halo! Kabar baik — chip jalan normal, suhu ") +
               String(temperatureRead(), 1) +
               " °C 😄\nSaya bisa: kontrol hardware (`hw`, `led on`, "
               "`nyalakan pin 5`) atau menjawab dari model \"" + kn + "\".";
      return String("Halo! 👋 Saya AURA — on-device intelligence.\n"
                    "Model UTAMA (bawaan): kontrol hardware — coba `hw`, "
                    "`led on`, `cek suhu`, `i2c scan`, `nyalakan pin 5`.\n"
                    "Model TAMBAHAN: pengetahuan \"") + kn +
             "\" — tanya sesuatu dari domainnya, atau ketik `model`.";
    }
    if (v == 1)
      return String("Hey! 👋 I'm AURA. What can I do for you? `hw` for "
                    "hardware, `temp` for live sensors, or ask me about \"") +
             kn + "\" — type `model` for the topic list.";
    if (v == 2)
      return String("Hello there! All systems normal — chip at ") +
             String(temperatureRead(), 1) +
             " °C 😄\nI can drive hardware (`hw`, `led on`, `pin 5 on`) or "
             "answer from my \"" + kn + "\" knowledge model.";
    return String("Hello! 👋 I'm AURA — on-device intelligence.\n"
                  "PRIMARY model (built-in): hardware — try `hw`, `led on`, "
                  "`temp`, `i2c scan`.\nADDITIONAL model: \"") + kn +
           "\" knowledge — ask me something from its domain, or type `model`.";
  }

  // --- hardware ---
  if (toks[0] == "hw" || toks[0] == "hardware") return cmdHw();
  if (toks[0] == "i2c") {
    int sda = -1, scl = -1;
    if (n >= 4) {
      sda = toks[2].toInt();
      scl = toks[3].toInt();
    }
    return cmdI2cScan(sda, scl);
  }

  int pin = -1;
  bool isLed = false, isAdc = false, isTemp = false, isPwm = false;
  int pwmVal = -1;
  bool actOn = false, actOff = false, actRead = false, actCheck = false;

  for (int i = 0; i < n; i++) {
    String &t = toks[i];
    if (t == "pin" || t == "gpio") {
      if (i + 1 < n) pin = toks[i + 1].toInt();
    } else if (t.startsWith("pin") && t.length() > 3 &&
               isdigit((unsigned char)t[3])) {
      pin = t.substring(3).toInt();
    } else if (t.startsWith("gpio") && t.length() > 4 &&
               isdigit((unsigned char)t[4])) {
      pin = t.substring(4).toInt();
    } else if (t == "led" || t == "lampu") {
      isLed = true;
    } else if (t == "adc" || t == "analog") {
      isAdc = true;
      if (i + 1 < n) pin = toks[i + 1].toInt();
    } else if (t == "pwm") {
      isPwm = true;
      if (i + 1 < n) pin = toks[i + 1].toInt();
      if (i + 2 < n) pwmVal = toks[i + 2].toInt();
    } else if (t == "temp" || t == "suhu" || t.startsWith("temperatur")) {
      isTemp = true;
    } else if (t == "on" || t == "nyalakan" || t == "hidupkan" ||
               t == "nyala" || t == "hidup" || t == "high") {
      actOn = true;
    } else if (t == "off" || t == "matikan" || t == "mati" || t == "low") {
      actOff = true;
    } else if (t == "read" || t == "baca") {
      actRead = true;
    } else if (t == "cek" || t == "check" || t == "berapa" || t == "coba" ||
               t == "ukur" || t == "measure" || t == "skrg" ||
               t == "sekarang" || t == "now") {
      actCheck = true;
    }
  }

  // "cek suhu sekarang" answers with live sensor data; conceptual questions
  // ("what is a temperature sensor") fall through to the knowledge model.
  if (isTemp && (actCheck || actRead || n <= 2)) return readTemperature();

  if (isAdc) {
    if (pin < 1 || pin > 10)
      return "⚙ adc: analog sensors go on GPIO 1-10 (ADC1) — e.g. `adc 4`. "
             "(GPIO 11-20 is ADC2, unusable while WiFi runs.)";
    int raw = analogRead(pin);
    int mv = analogReadMilliVolts(pin);
    gPinMode[pin] = 2;
    return String("⚙ analog GPIO ") + pin + " = " + raw + " raw (12-bit) ≈ " +
           mv + " mV";
  }

  if (isLed && pin < 0) pin = gCfg.ledPin;
  if (pin < 0) return "";  // not a hardware prompt — knowledge model's turn

  if (!pinAllowed(pin))
    return String("⚙ GPIO ") + pin +
           " is not in the safe list (strapping/flash/USB pins are "
           "protected). Safe: 1 2 4-18 21 38-42 47 48.";

  if (isPwm) {
    if (pwmVal < 0)
      return "⚙ pwm needs a duty value 0-255, e.g. `pwm 5 128`";
    if (pwmVal > 255) pwmVal = 255;
    analogWrite(pin, pwmVal);
    gPinMode[pin] = 3;
    return String("⚙ PWM on GPIO ") + pin + " → duty " + pwmVal + "/255 (" +
           (pwmVal * 100 / 255) + "%)";
  }
  if (actOn || actOff) {
    pinMode(pin, OUTPUT);
    gPinMode[pin] = 1;
    digitalWrite(pin, actOn ? HIGH : LOW);
    String what = (pin == gCfg.ledPin && isLed)
                      ? String("LED (GPIO ") + pin + ")"
                      : String("GPIO ") + pin;
    return String("⚙ ") + what + " → " + (actOn ? "ON (HIGH)" : "OFF (LOW)") +
           ((pin == gCfg.ledPin && isLed)
                ? "\n(onboard LED; some boards wire it inverted)"
                : "");
  }
  // no explicit action: report pin state (reads a wired digital sensor)
  if (gPinMode[pin] == 0) {
    pinMode(pin, INPUT);
    gPinMode[pin] = 2;
  }
  return String("⚙ ") + pinStateStr(pin) +
         (gPinMode[pin] == 2 ? "  (floating unless something is wired)" : "");
}

// --------------------------------------------------------- commands ---------

static String cmdStatus() {
  bool sta = (WiFi.status() == WL_CONNECTED);
  char buf[704];
  snprintf(buf, sizeof(buf),
           "AURA on %s rev %d @ %lu MHz\n"
           "flash: %u KB | psram: %u KB (free %u KB)\n"
           "heap: %u KB free | fs: %u/%u KB used\n"
           "uptime: %lu s\n"
           "wifi: %s (%s) | ip: %s | rssi: %d dBm\n"
           "wasm: wasm3 v" M3_VERSION "\n"
           "primary model: hardware (built-in) | additional: %s v%d (%u topics)",
           ESP.getChipModel(), ESP.getChipRevision(),
           (unsigned long)ESP.getCpuFreqMHz(),
           (unsigned)(ESP.getFlashChipSize() / 1024),
           (unsigned)(ESP.getPsramSize() / 1024),
           (unsigned)(ESP.getFreePsram() / 1024),
           (unsigned)(ESP.getFreeHeap() / 1024),
           (unsigned)(LittleFS.usedBytes() / 1024),
           (unsigned)(LittleFS.totalBytes() / 1024),
           (unsigned long)(millis() / 1000),
           sta ? gCfg.ssid : gCfg.apSsid, sta ? "station" : "fallback AP",
           sta ? WiFi.localIP().toString().c_str()
               : WiFi.softAPIP().toString().c_str(),
           sta ? (int)WiFi.RSSI() : 0,
           gModel.ok ? gModel.name.c_str() : "none", gModel.version,
           (unsigned)gModel.entries.size());
  return String(buf);
}

static String processPrompt(String p) {
  p.trim();
  if (p.length() == 0) return "say something :)";
  String low = p;
  low.toLowerCase();

  if (low == "help")
    return "I am AURA, on-device intelligence with two models:\n\n"
           "PRIMARY model — greetings + hardware (built-in, irreplaceable):\n"
           "  hw               device hardware overview + pin states\n"
           "  pin 5 on|off     drive an actuator (nyalakan/matikan pin 5)\n"
           "  pin 5 read       read a digital sensor (baca pin 5)\n"
           "  adc 4            read analog sensor, GPIO 1-10\n"
           "  pwm 5 128        PWM duty 0-255 (dimmer, motor)\n"
           "  led on|off       onboard LED\n"
           "  cek suhu / temp  live temperature from sensor\n"
           "  i2c scan         discover connected I2C sensor modules\n\n"
           "ADDITIONAL model — knowledge domain (swappable, TOON format):\n"
           "  model            show the loaded knowledge model\n"
           "  ...any question  answered if in-domain, declined if not\n\n"
           "Other: status, fib <n> (wasm on-chip), echo <txt>. Swap knowledge "
           "models in the Model panel below.";
  if (low == "status") return cmdStatus();
  if (low == "model" || low == "models") return cmdModelInfo();
  if (low == "fib") return cmdFib(24);
  if (low.startsWith("fib ")) return cmdFib(p.substring(4).toInt());
  if (low.startsWith("echo ")) return p.substring(5);

  String prim = tryPrimary(low);
  if (prim.length()) {
    Serial.printf("[primary] %s\n", p.c_str());
    return prim;
  }

  return askModel(p);
}

String AuraClass::ask(const String &prompt) { return processPrompt(prompt); }

// ------------------------------------------------------------- http ---------

static void handlePrompt() {
  String body = server.arg("plain");
  Serial.printf("[prompt] %s\n", body.c_str());
  server.send(200, "text/plain; charset=utf-8", processPrompt(body));
}

static void handleUploadChunk() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    uploadBuf.clear();
    uploadTooBig = false;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadBuf.size() + up.currentSize <= MAX_UPLOAD_SIZE)
      uploadBuf.insert(uploadBuf.end(), up.buf, up.buf + up.currentSize);
    else
      uploadTooBig = true;
  } else if (up.status == UPLOAD_FILE_END) {
    Serial.printf("[upload] received %u bytes\n", (unsigned)uploadBuf.size());
  }
}

static void handleWasmRun() {
  if (uploadTooBig) {
    uploadBuf.clear();
    server.send(413, "text/plain", "module too big (max 96 KB)");
    return;
  }
  if (uploadBuf.empty()) {
    server.send(400, "text/plain", "no .wasm module received");
    return;
  }
  String fn = server.arg("func");
  if (fn.length() == 0) fn = "fib";

  String toks[8];
  const char *argv[8];
  uint32_t argc = 0;
  String s = server.arg("args");
  int i = 0, len = s.length();
  while (i < len && argc < 8) {
    while (i < len && (s[i] == ' ' || s[i] == ',')) i++;
    int j = i;
    while (j < len && s[j] != ' ' && s[j] != ',') j++;
    if (j > i) {
      toks[argc] = s.substring(i, j);
      argv[argc] = toks[argc].c_str();
      argc++;
    }
    i = j;
  }

  Serial.printf("[wasm] run %s(%s) from %u-byte upload\n", fn.c_str(),
                s.c_str(), (unsigned)uploadBuf.size());
  int64_t us = 0;
  String r = runWasmModule(uploadBuf.data(), uploadBuf.size(), fn.c_str(), argc,
                           argv, &us);
  uploadBuf.clear();
  uploadBuf.shrink_to_fit();

  char tbuf[48];
  snprintf(tbuf, sizeof(tbuf), "%.2f ms", us / 1000.0);
  server.send(200, "text/plain; charset=utf-8",
              fn + "(" + s + ") = " + r + "\n(your .wasm, run on-chip in " +
                  tbuf + ")");
}

// Validate TOON model bytes, save to FS, respond, reboot with the new brain.
static void installModel(const uint8_t *bytes, size_t len) {
  String text;
  text.concat((const char *)bytes, len);
  Model m;
  String err;
  if (!parseToon(text, m, err)) {
    server.send(422, "text/plain", String("rejected: ") + err);
    return;
  }
  File f = LittleFS.open(MODEL_PATH, "w");
  if (!f) {
    server.send(500, "text/plain", "cannot write model file");
    return;
  }
  f.write(bytes, len);
  f.close();
  Serial.printf("[model] installed \"%s\" v%d (%u entries) — rebooting\n",
                m.name.c_str(), m.version, (unsigned)m.entries.size());
  server.send(200, "text/plain; charset=utf-8",
              String("knowledge model \"") + m.name + "\" v" + m.version +
                  " installed (" + (int)m.entries.size() +
                  " entries, TOON). Rebooting — the page reconnects in a few "
                  "seconds. The primary hardware model is unaffected.");
  delay(500);
  ESP.restart();
}

static void handleModelUpload() {
  if (uploadTooBig) {
    uploadBuf.clear();
    server.send(413, "text/plain", "model too big (max 96 KB)");
    return;
  }
  if (uploadBuf.empty()) {
    server.send(400, "text/plain", "no model file received");
    return;
  }
  installModel(uploadBuf.data(), uploadBuf.size());
  uploadBuf.clear();
  uploadBuf.shrink_to_fit();
}

static void handleModelFetch() {
  String url = server.arg("url");
  if (!url.length()) {
    server.send(400, "text/plain", "missing url");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, "text/plain",
                "not connected to a WiFi network — no internet to fetch from");
    return;
  }
  Serial.printf("[model] fetching %s\n", url.c_str());
  HTTPClient http;
  WiFiClientSecure tls;
  WiFiClient plainClient;
  bool ok;
  if (url.startsWith("https")) {
    tls.setInsecure();  // model registry TLS without a cert bundle
    ok = http.begin(tls, url);
  } else {
    ok = http.begin(plainClient, url);
  }
  if (!ok) {
    server.send(400, "text/plain", "bad url");
    return;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    server.send(502, "text/plain", String("fetch failed: HTTP ") + code);
    return;
  }
  String body = http.getString();
  http.end();
  if (body.length() == 0 || body.length() > MAX_UPLOAD_SIZE) {
    server.send(422, "text/plain",
                String("bad size: ") + body.length() + " bytes (max 96 KB)");
    return;
  }
  installModel((const uint8_t *)body.c_str(), body.length());
}

static void handleModelGet() {
  File f = LittleFS.open(MODEL_PATH, "r");
  if (!f) {
    server.send(404, "text/plain", "no model file");
    return;
  }
  server.streamFile(f, "text/plain; charset=utf-8");
  f.close();
}

static void handleModelInfo() {
  if (!gModel.ok) {
    server.send(200, "text/plain", "hw + no knowledge model");
    return;
  }
  server.send(200, "text/plain; charset=utf-8",
              String("hw + ") + gModel.name + " v" + gModel.version + " · " +
                  (int)gModel.entries.size() + " topics");
}

// ------------------------------------------------------------- page ---------

static const char INDEX_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AURA</title>
<style>
:root{--bg:#0f1115;--panel:#181b22;--line:#2a2f3a;--tx:#e6e9ef;--mut:#8b93a3;--ac:#6ea8fe}
*{box-sizing:border-box;margin:0}
body{background:var(--bg);color:var(--tx);font:15px/1.5 system-ui,-apple-system,sans-serif;height:100dvh;display:flex;flex-direction:column}
header{padding:12px 16px;border-bottom:1px solid var(--line);display:flex;align-items:center}
header b{font-size:15px}
header span{color:var(--mut);font-size:12px;margin-left:auto}
#log{flex:1;overflow-y:auto;padding:16px;display:flex;flex-direction:column;gap:10px}
.msg{max-width:82%;padding:8px 12px;border-radius:12px;white-space:pre-wrap;word-break:break-word}
.you{align-self:flex-end;background:#274690}
.esp{align-self:flex-start;background:var(--panel);border:1px solid var(--line)}
.esp.err{border-color:#a33}
.chips{padding:6px 16px;display:flex;gap:6px;flex-wrap:wrap}
.chip{background:var(--panel);border:1px solid var(--line);border-radius:999px;padding:2px 10px;color:var(--mut);cursor:pointer;font-size:12px}
form#f{display:flex;gap:8px;padding:12px 16px;border-top:1px solid var(--line)}
#inp{flex:1;background:var(--panel);border:1px solid var(--line);border-radius:10px;color:var(--tx);padding:10px 12px;font-size:15px;outline:none}
#inp:focus{border-color:var(--ac)}
button{background:var(--ac);border:0;border-radius:10px;color:#0b1020;font-weight:600;padding:8px 16px;cursor:pointer}
details{border-top:1px solid var(--line);padding:10px 16px;font-size:13px}
details summary{cursor:pointer;color:var(--mut)}
details p{color:var(--mut);margin-top:6px}
.wrow{display:flex;gap:8px;margin-top:10px;flex-wrap:wrap;align-items:center}
.wrow input[type=text]{background:var(--panel);border:1px solid var(--line);border-radius:8px;color:var(--tx);padding:6px 10px}
</style></head><body>
<header><b>AURA · on-device intelligence</b><span id="mi">model: ...</span></header>
<div id="log"></div>
<div class="chips">
<span class="chip" onclick="q('halo')">halo</span>
<span class="chip" onclick="q('hw')">hw</span>
<span class="chip" onclick="q('led on')">led on</span>
<span class="chip" onclick="q('cek suhu sekarang')">cek suhu</span>
<span class="chip" onclick="q('i2c scan')">i2c scan</span>
<span class="chip" onclick="q('what is a plc')">what is a plc</span>
<span class="chip" onclick="q('model')">model</span>
<span class="chip" onclick="q('fib 27')">fib 27</span>
</div>
<form id="f"><input id="inp" placeholder="Ask or command... e.g. nyalakan pin 5 / what is scada" autocomplete="off"><button>Send</button></form>
<details><summary>Knowledge model (TOON) — view / swap the brain</summary>
<p>The PRIMARY hardware model is built into firmware and never replaced. Knowledge models are TOON packs in flash: install one and the chip reboots with a new domain. <a href="/api/model" style="color:var(--ac)" download="model.toon">Download current model</a></p>
<div class="wrow">
<input type="file" id="mf" accept=".toon,.txt,text/plain">
<button type="button" onclick="upModel()">Install &amp; reboot</button>
</div>
<div class="wrow">
<input type="text" id="murl" placeholder="https://... model.toon URL" size="34">
<button type="button" onclick="fetchModel()">Fetch &amp; reboot</button>
</div>
</details>
<details><summary>Run your own .wasm on the chip</summary>
<p>Freestanding module (no WASI/imports), exported function, numeric args. Max 96 KB.</p>
<div class="wrow">
<input type="file" id="wf" accept=".wasm">
<input type="text" id="wfn" placeholder="function" value="fib" size="10">
<input type="text" id="wargs" placeholder="args e.g. 24" size="12">
<button type="button" onclick="runWasm()">Run on device</button>
</div>
</details>
<script>
var log=document.getElementById('log'),inp=document.getElementById('inp'),f=document.getElementById('f');
function add(cls,txt){var d=document.createElement('div');d.className='msg '+cls;d.textContent=txt;log.appendChild(d);log.scrollTop=log.scrollHeight;return d}
function q(t){inp.value=t;f.requestSubmit()}
f.onsubmit=async function(e){e.preventDefault();var t=inp.value.trim();if(!t)return;inp.value='';add('you',t);var w=add('esp','...');
try{var r=await fetch('/api/prompt',{method:'POST',headers:{'Content-Type':'text/plain'},body:t});w.textContent=await r.text();if(!r.ok)w.classList.add('err')}
catch(err){w.textContent='network error: '+err;w.classList.add('err')}};
function rebootWait(w){w.textContent+='\n\nWaiting for reboot...';setTimeout(function(){location.reload()},7000)}
async function upModel(){var file=document.getElementById('mf').files[0];
if(!file){add('esp','choose a model .toon file first').classList.add('err');return}
add('you','[install model] '+file.name);var w=add('esp','installing...');
var fd=new FormData();fd.append('model',file,'model.toon');
try{var r=await fetch('/api/model',{method:'POST',body:fd});w.textContent=await r.text();
if(r.ok)rebootWait(w);else w.classList.add('err')}
catch(err){w.textContent='network error: '+err;w.classList.add('err')}}
async function fetchModel(){var u=document.getElementById('murl').value.trim();
if(!u){add('esp','enter a model URL first').classList.add('err');return}
add('you','[fetch model] '+u);var w=add('esp','fetching on-chip...');
try{var r=await fetch('/api/model/fetch',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'url='+encodeURIComponent(u)});
w.textContent=await r.text();if(r.ok)rebootWait(w);else w.classList.add('err')}
catch(err){w.textContent='network error: '+err;w.classList.add('err')}}
async function runWasm(){var file=document.getElementById('wf').files[0];
if(!file){add('esp','choose a .wasm file first').classList.add('err');return}
var fn=document.getElementById('wfn').value.trim()||'fib';
var args=document.getElementById('wargs').value.trim();
add('you','[upload] '+file.name+' -> '+fn+'('+args+')');var w=add('esp','running on chip...');
var fd=new FormData();fd.append('func',fn);fd.append('args',args);fd.append('module',file,'m.wasm');
try{var r=await fetch('/api/wasm',{method:'POST',body:fd});w.textContent=await r.text();if(!r.ok)w.classList.add('err')}
catch(err){w.textContent='network error: '+err;w.classList.add('err')}}
fetch('/api/model/info').then(function(r){return r.text()}).then(function(t){document.getElementById('mi').textContent='model: '+t});
add('esp','Halo! 👋 I am AURA — on-device intelligence.\nPRIMARY model (built-in): hardware — try: hw, led on, cek suhu, i2c scan, nyalakan pin 5\nADDITIONAL model (swappable): knowledge — try: what is a plc — or type help');
</script></body></html>)html";

// ------------------------------------------------------------- begin --------

void AuraClass::begin() { begin(Config()); }

void AuraClass::begin(const char *ssid, const char *pass) {
  Config c;
  c.ssid = ssid;
  c.pass = pass;
  begin(c);
}

void AuraClass::begin(const Config &cfg) {
  gCfg = cfg;
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== AURA · on-device intelligence ===");

  if (!LittleFS.begin(true)) {
    Serial.println("[fs] ERROR: LittleFS mount failed");
  } else {
    Serial.printf("[fs] mounted, %u/%u KB used\n",
                  (unsigned)(LittleFS.usedBytes() / 1024),
                  (unsigned)(LittleFS.totalBytes() / 1024));
    ensureModelFile();
    loadModel();
    // ship a newer factory model? upgrade the on-flash copy in place
    if (gModel.ok && gModel.name == "automation" &&
        gModel.author == "Professor Claude" && gModel.version < 3) {
      Serial.printf("[model] upgrading factory model v%d -> v3\n",
                    gModel.version);
      File f = LittleFS.open(MODEL_PATH, "w");
      if (f) {
        f.print(FPSTR(DEFAULT_MODEL));
        f.close();
        loadModel();
      }
    }
  }

  const char *argv[1] = {"24"};
  int64_t us = 0;
  String r = runWasmModule(FIB_WASM, sizeof(FIB_WASM), "fib", 1, argv, &us);
  Serial.printf("wasm self-test: fib(24) = %s in %.2f ms — %s\n", r.c_str(),
                us / 1000.0, r == "46368" ? "OK" : "UNEXPECTED");
  if (gModel.ok) {
    String a = askModel("what is a plc");
    a.replace("\n", " ");
    Serial.printf("knowledge self-test: %.90s...\n", a.c_str());
  }
  String hwt = tryPrimary("cek suhu sekarang");
  hwt.replace("\n", " ");
  Serial.printf("primary self-test: %.90s\n", hwt.c_str());

  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(gCfg.hostname);
  WiFi.onEvent(
      [](WiFiEvent_t e, WiFiEventInfo_t info) {
        int rr = info.wifi_sta_disconnected.reason;
        const char *hint = "";
        if (rr == WIFI_REASON_NO_AP_FOUND)
          hint = " (SSID not found — 2.4 GHz only? name exact?)";
        else if (rr == WIFI_REASON_AUTH_FAIL ||
                 rr == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                 rr == WIFI_REASON_HANDSHAKE_TIMEOUT)
          hint = " (auth failed — wrong password?)";
        Serial.printf("[wifi] disconnected, reason %d%s\n", rr, hint);
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(
      [](WiFiEvent_t e, WiFiEventInfo_t info) {
        Serial.printf("[wifi] connected — open http://%s/\n",
                      WiFi.localIP().toString().c_str());
      },
      ARDUINO_EVENT_WIFI_STA_GOT_IP);
  if (gCfg.ssid && gCfg.ssid[0]) {
    WiFi.setAutoReconnect(true);
    WiFi.begin(gCfg.ssid, gCfg.pass);
    Serial.printf("connecting to \"%s\"", gCfg.ssid);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();
  } else {
    Serial.println("no station WiFi configured — AP-only mode");
  }
  // Fallback AP stays up either way; STA keeps retrying in loop()
  WiFi.softAP(gCfg.apSsid, gCfg.apPass);
  Serial.printf("fallback AP \"%s\" pass \"%s\" — http://%s/\n", gCfg.apSsid,
                gCfg.apPass, WiFi.softAPIP().toString().c_str());
  if (MDNS.begin(gCfg.hostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS: http://%s.local/\n", gCfg.hostname);
  }

  server.on("/", HTTP_GET,
            []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/prompt", HTTP_POST, handlePrompt);
  server.on("/api/wasm", HTTP_POST, handleWasmRun, handleUploadChunk);
  server.on("/api/model", HTTP_GET, handleModelGet);
  server.on("/api/model", HTTP_POST, handleModelUpload, handleUploadChunk);
  server.on("/api/model/fetch", HTTP_POST, handleModelFetch);
  server.on("/api/model/info", HTTP_GET, handleModelInfo);
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });
  server.begin();
  Serial.println("web server running on port 80");
}

void AuraClass::loop() {
  server.handleClient();
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 15000) {
    lastBeat = millis();
    bool sta = (WiFi.status() == WL_CONNECTED);
    if (gCfg.heartbeatLog)
      Serial.printf("[alive] uptime %lus, heap %u KB, ip %s\n",
                    (unsigned long)(millis() / 1000),
                    (unsigned)(ESP.getFreeHeap() / 1024),
                    sta ? WiFi.localIP().toString().c_str()
                        : WiFi.softAPIP().toString().c_str());
    if (!sta && gCfg.ssid && gCfg.ssid[0]) {
      Serial.printf("[wifi] retrying \"%s\"...\n", gCfg.ssid);
      WiFi.begin(gCfg.ssid, gCfg.pass);
    }
  }
  delay(2);
}
