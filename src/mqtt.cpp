#include "mqtt.h"

#include <Arduino.h>
#include <PubSubClient.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

static WiFiClient *wifiClientPtr = nullptr;
static PubSubClient *mqttPtr = nullptr;
static DeviceConfig cfg;
static IncomingHandler incomingHandler = nullptr;
static String topicPub;
static String topicCmd;

static void internalCallback(char *topic, byte *payload, unsigned int length) {
  String message;
  message.reserve(length + 1);
  for (unsigned int i = 0; i < length; ++i) {
    message += static_cast<char>(payload[i]);
  }
  if (incomingHandler) incomingHandler(message);
}

void mqttInit(Client &networkClient, const DeviceConfig &deviceCfg, IncomingHandler handler) {
  wifiClientPtr = &static_cast<WiFiClient &>(networkClient);
  if (mqttPtr == nullptr) {
    mqttPtr = new PubSubClient(*wifiClientPtr);
  }

  cfg = deviceCfg;
  incomingHandler = handler;

  const char *host = cfg.mqtt_host.length() ? cfg.mqtt_host.c_str() : "broker.hivemq.com";
  uint16_t port = cfg.mqtt_port ? cfg.mqtt_port : 1183;
  mqttPtr->setServer(host, port);
  mqttPtr->setCallback(internalCallback);

  const char *did = cfg.device_id.length() ? cfg.device_id.c_str() : "esp8266_pote_01";
  topicPub = String("pet/") + did + "/peso";
  topicCmd = String("pet/") + did + "/comando";
}

static const char *mqttStateToString(int state) {
  switch (state) {
    case MQTT_CONNECTION_TIMEOUT: return "MQTT_CONNECTION_TIMEOUT";
    case MQTT_CONNECTION_LOST: return "MQTT_CONNECTION_LOST";
    case MQTT_CONNECT_FAILED: return "MQTT_CONNECT_FAILED";
    case MQTT_DISCONNECTED: return "MQTT_DISCONNECTED";
    case MQTT_CONNECTED: return "MQTT_CONNECTED";
    case MQTT_CONNECT_BAD_PROTOCOL: return "MQTT_CONNECT_BAD_PROTOCOL";
    case MQTT_CONNECT_BAD_CLIENT_ID: return "MQTT_CONNECT_BAD_CLIENT_ID";
    case MQTT_CONNECT_UNAVAILABLE: return "MQTT_CONNECT_UNAVAILABLE";
    case MQTT_CONNECT_BAD_CREDENTIALS: return "MQTT_CONNECT_BAD_CREDENTIALS";
    case MQTT_CONNECT_UNAUTHORIZED: return "MQTT_CONNECT_UNAUTHORIZED";
    default: return "MQTT_STATE_UNKNOWN";
  }
}

void mqttLoop() {
  if (mqttPtr && mqttPtr->connected()) mqttPtr->loop();
}

bool mqttConnected() {
  return mqttPtr && mqttPtr->connected();
}

void mqttEnsureConnected() {
  if (!mqttPtr) return;
  if (mqttPtr->connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  const char *deviceId = cfg.device_id.length() ? cfg.device_id.c_str() : "esp8266_pote_01";
  const char *user = cfg.mqtt_user.length() ? cfg.mqtt_user.c_str() : "";
  const char *pass = cfg.mqtt_pass.length() ? cfg.mqtt_pass.c_str() : "";

    Serial.println("=== Tentativa de conexao MQTT ===");
    Serial.print("Broker: ");
    Serial.println(cfg.mqtt_host);
    Serial.print("User: ");
    Serial.println(user);
    Serial.print("Pass: ");
    Serial.println(pass);

  bool connected = false;
  if (strlen(user) > 0) {
    connected = mqttPtr->connect(deviceId, user, pass, topicCmd.c_str(), 0, true, "offline");
  } else {
    connected = mqttPtr->connect(deviceId);
  }

  if (connected) {
    mqttPtr->subscribe(topicCmd.c_str());
    Serial.println("MQTT conectado com sucesso");
    Serial.print("Broker: ");
    Serial.println(cfg.mqtt_host);
    Serial.print("Topico pub: ");
    Serial.println(topicPub);
    Serial.print("Topico cmd: ");
    Serial.println(topicCmd);
  } else {
    int state = mqttPtr->state();
    Serial.printf("Falha ao conectar MQTT, estado=%d (%s)\n", state, mqttStateToString(state));
  }
}

bool mqttPublishPayload(const char *payload, size_t size) {
  if (!mqttPtr) return false;
  if (!mqttPtr->connected()) return false;
  return mqttPtr->publish(topicPub.c_str(), payload, size);
}

const String &mqttTopicPublish() { return topicPub; }
const String &mqttTopicCommand() { return topicCmd; }