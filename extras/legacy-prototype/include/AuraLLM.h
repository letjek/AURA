#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#ifdef AURA_ESP32
  #include <WiFiClientSecure.h>
  #include <HTTPClient.h>
#endif

#include "AuraStorage.h"

// ─── Conversation history (rolling buffer) ───────────────────────────────────
#define MAX_HISTORY  10

struct ChatMessage {
  char role[16];    // "user" | "assistant" | "system"
  char content[512];
};

class AuraLLM {
public:
  ChatMessage history[MAX_HISTORY];
  int historyCount = 0;

  void clearHistory() { historyCount = 0; }

  void addMessage(const char* role, const char* content) {
    if (historyCount >= MAX_HISTORY) {
      // Shift oldest messages out (keep system at [0] if present)
      int start = (strcmp(history[0].role, "system") == 0) ? 1 : 0;
      memmove(&history[start], &history[start + 1],
              sizeof(ChatMessage) * (MAX_HISTORY - start - 1));
      historyCount--;
    }
    strlcpy(history[historyCount].role, role, sizeof(history[0].role));
    strlcpy(history[historyCount].content, content, sizeof(history[0].content));
    historyCount++;
  }

  // ── Main chat function ────────────────────────────────────────────────────
  String chat(const String& userMessage, const String& extraContext = "") {
    AuraSettings& cfg = Storage.settings;

    // Build system prompt with sensor context
    String systemPrompt = String(cfg.system_prompt);
    if (extraContext.length() > 0) {
      systemPrompt += "\n\n" + extraContext;
    }

    // Determine endpoint
    String provider = String(cfg.llm_provider);
    provider.toLowerCase();

    if (provider == "openai" || provider == "groq" || provider == "openrouter") {
      return chatOpenAICompatible(userMessage, systemPrompt, provider);
    } else if (provider == "gemini") {
      return chatGemini(userMessage, systemPrompt);
    } else if (provider == "anthropic") {
      return chatAnthropic(userMessage, systemPrompt);
    } else if (provider == "ollama") {
      return chatOllama(userMessage, systemPrompt);
    }

    return "Error: Unknown LLM provider '" + provider + "'";
  }

private:
  // ── OpenAI / Groq / OpenRouter (same API format) ─────────────────────────
  String chatOpenAICompatible(const String& userMsg, const String& sysPrompt,
                               const String& provider) {
    AuraSettings& cfg = Storage.settings;
    String baseUrl;

    if (provider == "groq") {
      baseUrl = "https://api.groq.com/openai/v1";
    } else if (provider == "openrouter") {
      baseUrl = "https://openrouter.ai/api/v1";
    } else if (strlen(cfg.llm_base_url) > 0) {
      baseUrl = String(cfg.llm_base_url);
    } else {
      baseUrl = "https://api.openai.com/v1";
    }

    // Build messages array
    DynamicJsonDocument doc(4096);
    doc["model"]       = cfg.llm_model;
    doc["max_tokens"]  = cfg.llm_max_tokens;
    doc["temperature"] = cfg.llm_temperature;

    JsonArray msgs = doc.createNestedArray("messages");

    // System message
    JsonObject sys = msgs.createNestedObject();
    sys["role"]    = "system";
    sys["content"] = sysPrompt;

    // History
    for (int i = 0; i < historyCount; i++) {
      if (strcmp(history[i].role, "system") == 0) continue;
      JsonObject m = msgs.createNestedObject();
      m["role"]    = history[i].role;
      m["content"] = history[i].content;
    }

    // New user message
    JsonObject um = msgs.createNestedObject();
    um["role"]    = "user";
    um["content"] = userMsg;

    String body;
    serializeJson(doc, body);

    String response = httpPost(baseUrl + "/chat/completions",
                               "Bearer " + String(cfg.llm_api_key),
                               body);

    return parseOpenAIResponse(response, userMsg);
  }

  String parseOpenAIResponse(const String& raw, const String& userMsg) {
    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, raw)) return "Error: JSON parse failed\n" + raw.substring(0,200);
    if (doc.containsKey("error")) {
      return "LLM Error: " + String(doc["error"]["message"].as<const char*>());
    }
    const char* content = doc["choices"][0]["message"]["content"];
    if (!content) return "Error: No content in response";

    addMessage("user", userMsg.c_str());
    addMessage("assistant", content);
    return String(content);
  }

  // ── Gemini ────────────────────────────────────────────────────────────────
  String chatGemini(const String& userMsg, const String& sysPrompt) {
    AuraSettings& cfg = Storage.settings;
    String url = "https://generativelanguage.googleapis.com/v1beta/models/";
    url += cfg.llm_model;
    url += ":generateContent?key=";
    url += cfg.llm_api_key;

    DynamicJsonDocument doc(4096);
    JsonArray contents = doc.createNestedArray("contents");

    // System instruction
    JsonObject sysInst = doc.createNestedObject("systemInstruction");
    JsonArray sysParts = sysInst.createNestedArray("parts");
    JsonObject sysPart = sysParts.createNestedObject();
    sysPart["text"] = sysPrompt;

    // User message
    JsonObject userContent = contents.createNestedObject();
    userContent["role"] = "user";
    JsonArray parts = userContent.createNestedArray("parts");
    JsonObject part = parts.createNestedObject();
    part["text"] = userMsg;

    JsonObject genConfig = doc.createNestedObject("generationConfig");
    genConfig["maxOutputTokens"] = cfg.llm_max_tokens;
    genConfig["temperature"]     = cfg.llm_temperature;

    String body;
    serializeJson(doc, body);

    String response = httpPost(url, "", body);

    DynamicJsonDocument resp(4096);
    if (deserializeJson(resp, response)) return "Error: JSON parse failed";
    if (resp.containsKey("error")) {
      return "LLM Error: " + String(resp["error"]["message"].as<const char*>());
    }
    const char* text = resp["candidates"][0]["content"]["parts"][0]["text"];
    if (!text) return "Error: No text in Gemini response";

    addMessage("user", userMsg.c_str());
    addMessage("assistant", text);
    return String(text);
  }

  // ── Anthropic (Claude) ────────────────────────────────────────────────────
  String chatAnthropic(const String& userMsg, const String& sysPrompt) {
    AuraSettings& cfg = Storage.settings;

    DynamicJsonDocument doc(4096);
    doc["model"]      = cfg.llm_model;
    doc["max_tokens"] = cfg.llm_max_tokens;
    doc["system"]     = sysPrompt;

    JsonArray msgs = doc.createNestedArray("messages");
    for (int i = 0; i < historyCount; i++) {
      if (strcmp(history[i].role, "system") == 0) continue;
      JsonObject m = msgs.createNestedObject();
      m["role"] = history[i].role;
      JsonArray parts = m.createNestedArray("content");
      JsonObject p = parts.createNestedObject();
      p["type"] = "text";
      p["text"] = history[i].content;
    }
    JsonObject um = msgs.createNestedObject();
    um["role"] = "user";
    JsonArray up = um.createNestedArray("content");
    JsonObject p2 = up.createNestedObject();
    p2["type"] = "text";
    p2["text"] = userMsg;

    String body;
    serializeJson(doc, body);

    // Anthropic uses x-api-key header
    String response = httpPostAnthropic("https://api.anthropic.com/v1/messages",
                                        String(cfg.llm_api_key), body);

    DynamicJsonDocument resp(4096);
    if (deserializeJson(resp, response)) return "Error: JSON parse failed";
    if (resp.containsKey("error")) {
      return "LLM Error: " + String(resp["error"]["message"].as<const char*>());
    }
    const char* text = resp["content"][0]["text"];
    if (!text) return "Error: No content in Anthropic response";

    addMessage("user", userMsg.c_str());
    addMessage("assistant", text);
    return String(text);
  }

  // ── Ollama (local network) ────────────────────────────────────────────────
  String chatOllama(const String& userMsg, const String& sysPrompt) {
    AuraSettings& cfg = Storage.settings;
    String baseUrl = strlen(cfg.llm_base_url) > 0
                     ? String(cfg.llm_base_url)
                     : "http://192.168.1.100:11434";

    DynamicJsonDocument doc(4096);
    doc["model"]  = cfg.llm_model;
    doc["stream"] = false;

    JsonArray msgs = doc.createNestedArray("messages");
    JsonObject sm = msgs.createNestedObject();
    sm["role"]    = "system";
    sm["content"] = sysPrompt;

    for (int i = 0; i < historyCount; i++) {
      if (strcmp(history[i].role, "system") == 0) continue;
      JsonObject m = msgs.createNestedObject();
      m["role"]    = history[i].role;
      m["content"] = history[i].content;
    }
    JsonObject um = msgs.createNestedObject();
    um["role"]    = "user";
    um["content"] = userMsg;

    String body;
    serializeJson(doc, body);

    String response = httpPost(baseUrl + "/api/chat", "", body);
    return parseOpenAIResponse(response, userMsg); // Ollama uses compatible format
  }

  // ── HTTP helpers ──────────────────────────────────────────────────────────
  String httpPost(const String& url, const String& authHeader, const String& body) {
    #ifdef AURA_ESP32
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); // For simplicity; use cert pinning in production

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    if (authHeader.length() > 0) {
      http.addHeader("Authorization", authHeader);
    }
    http.setTimeout(30000);

    int code = http.POST(body);
    String response = (code > 0) ? http.getString() : "HTTP Error: " + String(code);
    http.end();
    Serial.printf("[LLM] POST %s -> %d\n", url.c_str(), code);
    return response;
    #else
    return "Error: HTTP not implemented for this board";
    #endif
  }

  String httpPostAnthropic(const String& url, const String& apiKey, const String& body) {
    #ifdef AURA_ESP32
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", apiKey);
    http.addHeader("anthropic-version", "2023-06-01");
    http.setTimeout(30000);

    int code = http.POST(body);
    String response = (code > 0) ? http.getString() : "HTTP Error: " + String(code);
    http.end();
    return response;
    #else
    return "Error: HTTP not implemented for this board";
    #endif
  }
};

extern AuraLLM LLM;
