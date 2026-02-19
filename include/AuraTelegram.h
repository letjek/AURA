#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#ifdef AURA_ESP32
  #include <WiFiClientSecure.h>
  #include <HTTPClient.h>
#endif

#include "AuraStorage.h"
#include "AuraLLM.h"
#include "AuraSensors.h"

class AuraTelegram {
public:
  long lastUpdateId = 0;
  unsigned long lastPollMs = 0;
  bool enabled = false;

  void begin() {
    if (strlen(Storage.settings.telegram_token) > 0) {
      enabled = true;
      Serial.println("[Telegram] Bot enabled");
      sendMessage("🤖 AURA is online! Send me a message to get started.");
    }
  }

  void loop() {
    if (!enabled) return;
    if (millis() - lastPollMs < TELEGRAM_POLL_INTERVAL_MS) return;
    lastPollMs = millis();
    poll();
  }

private:
  void poll() {
    #ifdef AURA_ESP32
    String url = "https://api.telegram.org/bot";
    url += Storage.settings.telegram_token;
    url += "/getUpdates?offset=" + String(lastUpdateId + 1) + "&timeout=0&limit=5";

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    http.begin(client, url);
    http.setTimeout(10000);
    int code = http.GET();

    if (code != 200) { http.end(); return; }

    String body = http.getString();
    http.end();

    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, body)) return;
    if (!doc["ok"]) return;

    JsonArray results = doc["result"].as<JsonArray>();
    for (JsonObject update : results) {
      long uid = update["update_id"].as<long>();
      if (uid > lastUpdateId) lastUpdateId = uid;

      // Handle message
      if (update.containsKey("message")) {
        JsonObject msg = update["message"];
        long chatId    = msg["chat"]["id"].as<long>();
        const char* text = msg["text"] | "";

        // Whitelist check
        if (strlen(Storage.settings.telegram_chat_id) > 0) {
          String allowed = String(Storage.settings.telegram_chat_id);
          if (allowed.indexOf(String(chatId)) == -1) {
            sendMessageToChat(chatId, "⛔ Unauthorized.");
            continue;
          }
        }

        Serial.printf("[Telegram] From %ld: %s\n", chatId, text);
        handleCommand(chatId, String(text));
      }
    }
    #endif
  }

  void handleCommand(long chatId, String text) {
    text.trim();

    // Built-in commands
    if (text == "/start" || text == "/help") {
      String help = "🤖 *AURA Commands*\n\n"
                    "/status - Sensor readings\n"
                    "/sensors - List sensors\n"
                    "/clear - Clear chat history\n"
                    "/reset - Restart AURA\n\n"
                    "Or just chat naturally!";
      sendMessageToChat(chatId, help);
      return;
    }

    if (text == "/status") {
      Sensors.readAll();
      String status = "📊 *Current Status*\n\n";
      for (int i = 0; i < 8; i++) {
        SensorDef& s = Storage.sensors[i];
        if (!s.enabled || strlen(s.name) == 0) continue;
        status += "• " + String(s.name) + ": `" + String(s.last_str);
        if (strlen(s.unit) > 0) status += " " + String(s.unit);
        status += "`\n";
      }
      sendMessageToChat(chatId, status);
      return;
    }

    if (text == "/clear") {
      LLM.clearHistory();
      sendMessageToChat(chatId, "🗑 Conversation history cleared.");
      return;
    }

    if (text == "/reset") {
      sendMessageToChat(chatId, "🔄 Restarting AURA...");
      delay(500);
      ESP.restart();
      return;
    }

    // Otherwise: send to LLM
    Sensors.readAll();
    String context = Sensors.buildContextForLLM();
    sendMessageToChat(chatId, "⏳ Thinking...");

    String reply = LLM.chat(text, context);

    // Parse and execute any actions in the reply
    parseAndExecuteActions(reply);

    sendMessageToChat(chatId, reply);
  }

  void parseAndExecuteActions(const String& reply) {
    // Look for JSON action block in LLM reply
    int start = reply.indexOf("{\"actions\"");
    if (start == -1) return;
    int end = reply.indexOf("}", start);
    if (end == -1) return;

    String jsonStr = reply.substring(start, end + 1);
    // Try wrapping in case nested
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, jsonStr)) return;

    if (!doc.containsKey("actions")) return;
    JsonArray actions = doc["actions"].as<JsonArray>();
    for (JsonObject action : actions) {
      const char* name = action["name"] | "";
      bool state = action["state"] | false;
      int pwm    = action["pwm"] | -1;
      Sensors.setActuator(name, state, pwm);
    }
  }

public:
  void sendMessage(const String& text) {
    // Send to configured chat_id
    if (!enabled || strlen(Storage.settings.telegram_chat_id) == 0) return;
    long chatId = String(Storage.settings.telegram_chat_id).toInt();
    if (chatId != 0) sendMessageToChat(chatId, text);
  }

  void sendMessageToChat(long chatId, const String& text) {
    #ifdef AURA_ESP32
    String url = "https://api.telegram.org/bot";
    url += Storage.settings.telegram_token;
    url += "/sendMessage";

    DynamicJsonDocument doc(2048);
    doc["chat_id"]    = chatId;
    doc["text"]       = text;
    doc["parse_mode"] = "Markdown";

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);
    int code = http.POST(body);
    http.end();
    Serial.printf("[Telegram] Send -> %d\n", code);
    #endif
  }
};

extern AuraTelegram Telegram;
