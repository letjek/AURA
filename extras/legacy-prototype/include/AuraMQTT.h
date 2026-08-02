#pragma once
#ifdef AURA_ESP32

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "AuraStorage.h"
#include "AuraSensors.h"
#include "AuraConfig.h"

// ── AuraMQTT ─────────────────────────────────────────────────────────────────
// Multi-device MQTT bridge for AURA nodes.
//
// Topic scheme:
//   aura/{chipId}/sensors/{name}   → this device publishes sensor readings
//   aura/{chipId}/cmd/{name}       → this device receives actuator commands
//   aura/{chipId}/status           → heartbeat (IP, heap, uptime)
//
// Remote sensor slots:
//   Configure a sensor slot with type "mqtt_remote" and name "{chipId}/{sensorName}".
//   Incoming readings are written directly into that slot so buildContextForLLM()
//   includes them without any changes to AuraSensors.h.
//
// Remote actuator control:
//   Configure an actuator slot with type "mqtt" and name "{chipId}/{actuatorName}".
//   setActuator() updates a.state for any matched name (existing behaviour).
//   The MQTT task detects the state change and publishes the command to the
//   remote device's cmd topic.

class AuraMQTT {
public:
  // ── Construction ──────────────────────────────────────────────────────────
  AuraMQTT() : _client(_wifiClient) {
    _instance = this;
    _deviceId = "aura-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);
  }

  // ── begin() ───────────────────────────────────────────────────────────────
  // Called from custom.cpp when WiFi connects. Safe to call multiple times.
  void begin() {
    if (_started) return;
    if (!Storage.settings.mqtt_host[0]) {
      Serial.println("[MQTT] No broker configured — skipping");
      return;
    }
    _started = true;
    _client.setServer(Storage.settings.mqtt_host, Storage.settings.mqtt_port);
    _client.setCallback(_staticCallback);
    _reconnect();
    xTaskCreate(_task, "AuraMQTT", 5120, this, 1, nullptr);
    Serial.printf("[MQTT] Started — device id: %s\n", _deviceId.c_str());
  }

  // ── deviceId() ────────────────────────────────────────────────────────────
  const String& deviceId() const { return _deviceId; }

  // ── connected() ───────────────────────────────────────────────────────────
  bool connected() { return _client.connected(); }

private:
  WiFiClient    _wifiClient;
  PubSubClient  _client;
  String        _deviceId;
  bool          _started     = false;
  unsigned long _lastPublish = 0;

  // Singleton pointer — needed for the static PubSubClient callback
  static AuraMQTT* _instance;

  // Shadow of actuator states to detect changes
  bool _prevState[MAX_ACTUATORS] = {};
  int  _prevValue[MAX_ACTUATORS] = {};

  static constexpr unsigned long PUBLISH_MS = SENSOR_READ_INTERVAL_MS;

  // Static callback required by PubSubClient (no captures allowed)
  static void _staticCallback(char* topic, byte* payload, unsigned int len) {
    if (_instance) _instance->_onMessage(topic, payload, len);
  }

  // ── _reconnect() ──────────────────────────────────────────────────────────
  void _reconnect() {
    if (_client.connected()) return;
    Serial.printf("[MQTT] Connecting to %s:%d ...\n",
                  Storage.settings.mqtt_host, Storage.settings.mqtt_port);
    if (_client.connect(_deviceId.c_str())) {
      // Receive actuator commands addressed to this device
      String cmdTopic = "aura/" + _deviceId + "/cmd/+";
      _client.subscribe(cmdTopic.c_str());

      // Receive sensor readings from ALL other AURA nodes
      _client.subscribe("aura/+/sensors/+");

      Serial.println("[MQTT] Connected");
    } else {
      Serial.printf("[MQTT] Failed (rc=%d), will retry\n", _client.state());
    }
  }

  // ── _publishSensors() ─────────────────────────────────────────────────────
  void _publishSensors() {
    if (!_client.connected()) return;
    for (int i = 0; i < MAX_SENSORS; i++) {
      SensorDef& s = Storage.sensors[i];
      if (!s.enabled || strcmp(s.type, "mqtt_remote") == 0) continue;
      String topic = "aura/" + _deviceId + "/sensors/" + s.name;
      _client.publish(topic.c_str(), s.last_str);
    }
    // Heartbeat
    char status[96];
    snprintf(status, sizeof(status),
             "{\"ip\":\"%s\",\"heap\":%u,\"uptime\":%lu}",
             WiFi.localIP().toString().c_str(),
             (unsigned)ESP.getFreeHeap(),
             millis() / 1000UL);
    _client.publish(("aura/" + _deviceId + "/status").c_str(), status);
  }

  // ── _checkMqttActuators() ─────────────────────────────────────────────────
  // Polls actuators with type "mqtt". If state/value changed since last check,
  // publish a command to the remote device encoded in the actuator name.
  // Actuator name format: "{remoteChipId}/{remoteActuatorName}"
  void _checkMqttActuators() {
    if (!_client.connected()) return;
    for (int i = 0; i < MAX_ACTUATORS; i++) {
      ActuatorDef& a = Storage.actuators[i];
      if (!a.enabled || strcmp(a.type, "mqtt") != 0) continue;
      if (a.state == _prevState[i] && a.value == _prevValue[i]) continue;

      // State changed — find the slash separator in name: "{chipId}/{actuatorName}"
      char* slash = strchr(a.name, '/');
      if (!slash) continue;
      String remoteChipId = String(a.name).substring(0, slash - a.name);
      String remoteName   = String(slash + 1);

      // Build command payload
      char payload[48];
      if (strcmp(a.type, "pwm") == 0) {
        snprintf(payload, sizeof(payload), "{\"state\":%s,\"pwm\":%d}",
                 a.state ? "true" : "false", a.value);
      } else {
        snprintf(payload, sizeof(payload), "{\"state\":%s}",
                 a.state ? "true" : "false");
      }

      String cmdTopic = "aura/" + remoteChipId + "/cmd/" + remoteName;
      _client.publish(cmdTopic.c_str(), payload);
      Serial.printf("[MQTT] → %s : %s\n", cmdTopic.c_str(), payload);

      _prevState[i] = a.state;
      _prevValue[i] = a.value;
    }
  }

  // ── _onMessage() ──────────────────────────────────────────────────────────
  // Called by PubSubClient when a subscribed message arrives.
  void _onMessage(char* topic, byte* payload, unsigned int len) {
    String t(topic);
    String msg((char*)payload, len);

    // ── Actuator command for THIS device ─────────────────────────────────
    // Topic: aura/{thisId}/cmd/{actuatorName}
    String cmdPrefix = "aura/" + _deviceId + "/cmd/";
    if (t.startsWith(cmdPrefix)) {
      String actuatorName = t.substring(cmdPrefix.length());
      StaticJsonDocument<128> doc;
      if (deserializeJson(doc, msg) == DeserializationError::Ok) {
        bool  state = doc["state"] | false;
        int   pwm   = doc["pwm"]   | -1;
        Sensors.setActuator(actuatorName.c_str(), state, pwm);
        Serial.printf("[MQTT] ← cmd %s state=%d pwm=%d\n",
                      actuatorName.c_str(), state, pwm);
      }
      return;
    }

    // ── Remote sensor reading ─────────────────────────────────────────────
    // Topic: aura/{remoteId}/sensors/{sensorName}
    // Ignore our own readings echoed back
    String ownPrefix = "aura/" + _deviceId + "/sensors/";
    if (t.startsWith(ownPrefix)) return;

    if (t.startsWith("aura/") && t.indexOf("/sensors/") > 0) {
      int start  = 5; // after "aura/"
      int sepA   = t.indexOf('/', start);
      if (sepA < 0) return;
      String remoteId = t.substring(start, sepA);
      int    sensOff  = t.indexOf("/sensors/", sepA);
      if (sensOff < 0) return;
      String sensorName = t.substring(sensOff + 9);

      // Slot name convention for mqtt_remote sensors: "{remoteId}/{sensorName}"
      String slotName = remoteId + "/" + sensorName;

      for (int i = 0; i < MAX_SENSORS; i++) {
        SensorDef& s = Storage.sensors[i];
        if (!s.enabled || strcmp(s.type, "mqtt_remote") != 0) continue;
        if (strcasecmp(s.name, slotName.c_str()) == 0) {
          s.last_value = msg.toFloat();
          strlcpy(s.last_str, msg.c_str(), sizeof(s.last_str));
          return;
        }
      }
    }
  }

  // ── FreeRTOS task ─────────────────────────────────────────────────────────
  static void _task(void* param) {
    AuraMQTT* self = static_cast<AuraMQTT*>(param);
    while (true) {
      if (!self->_client.connected()) self->_reconnect();
      self->_client.loop();
      self->_checkMqttActuators();

      unsigned long now = millis();
      if (now - self->_lastPublish >= PUBLISH_MS) {
        self->_publishSensors();
        self->_lastPublish = now;
      }
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }
};

extern AuraMQTT MQTT;

// Static member definition — placed in the header so the linker finds it in
// the single translation unit (custom.cpp) that includes this header.
// If you ever include AuraMQTT.h from a second .cpp, move this line there.
#ifdef AURA_MQTT_IMPL
AuraMQTT* AuraMQTT::_instance = nullptr;
#endif

#endif // AURA_ESP32
