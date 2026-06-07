#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "storage.h"
#include "provisioning.h"
#include "sensor.h"
#include "mqtt.h"

namespace Config {
constexpr unsigned long SENSOR_READ_INTERVAL_MS = 2000UL;
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 10000UL;
constexpr unsigned long NTP_UPDATE_INTERVAL_MS = 60000UL;
constexpr unsigned long MIN_EVENT_INTERVAL_MS = 30000UL;
constexpr int TIMEZONE_OFFSET_SECONDS = -3 * 3600;
constexpr bool TEST_MODE_EVERY_SECOND = true;
constexpr bool SERIAL_ONLY_TEST_MODE = false;
constexpr uint8_t HCSR04_TRIG_PIN = 12;
constexpr uint8_t HCSR04_ECHO_PIN = 13;
constexpr float HCSR04_NEAR_DISTANCE_CM = 10.0f;
constexpr uint8_t HCSR04_SAMPLE_COUNT = 3;
}

WiFiUDP ntpUdp;
NTPClient timeClient(ntpUdp, "pool.ntp.org", Config::TIMEZONE_OFFSET_SECONDS, Config::NTP_UPDATE_INTERVAL_MS);
WiFiClient wifiClient;

DeviceConfig deviceCfg;
bool haveDeviceConfig = false;
bool lastNearState = false;
unsigned long lastEventMs = 0;
unsigned long lastSensorReadMs = 0;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastNtpUpdateMs = 0;

void connectWiFi();
void ensureWiFiConnected();
void processSerialCommand(String line);
void processIncomingCommand(const String &payload);
void handleResetCommand();
void readSensorDistance();
bool buildTimestamp(char *buffer, size_t bufferSize);
void publishFeedingEvent(float distanceCm);
void printWiFiStatus();

const char *DEFAULT_DEVICE_ID = "esp8266_pote_01";
const char *DEFAULT_TOKEN = "TOKEN_SECRETO";
const char *DEFAULT_SSID = "";
const char *DEFAULT_PASS = "";

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("Iniciando monitoramento de alimentacao com ESP8266 + HC-SR04");
  Serial.println("Comandos Serial: help | provision | reset");

  if (Config::SERIAL_ONLY_TEST_MODE) {
    sensorInit(Config::HCSR04_TRIG_PIN, Config::HCSR04_ECHO_PIN, Config::HCSR04_NEAR_DISTANCE_CM);
    lastSensorReadMs = millis();
    Serial.println("[MODO TESTE] Apenas Serial ativo. Leitura de distancia a cada 2 segundos.");
    return;
  }

  haveDeviceConfig = loadDeviceConfig(deviceCfg);
  if (!haveDeviceConfig) {
    delay(250);
    Serial.println("Nenhuma configuracao salva.");
    Serial.println("Para iniciar o AP de provisionamento digite 'provision' no Serial monitor ou pressione o botao de provisionamento.");
  }

  sensorInit(Config::HCSR04_TRIG_PIN, Config::HCSR04_ECHO_PIN, Config::HCSR04_NEAR_DISTANCE_CM);

  WiFi.mode(WIFI_STA);
  connectWiFi();

  timeClient.begin();
  timeClient.update();

  mqttInit(wifiClient, deviceCfg, [](const String &payload) { processIncomingCommand(payload); });

  lastSensorReadMs = millis();
  lastNtpUpdateMs = millis();

  if (Config::TEST_MODE_EVERY_SECOND) {
    Serial.println("[MODO TESTE] O sensor sera lido a cada 2 segundos e eventos de alimentacao serao publicados.");
  }
}

void loop() {
  if (Serial && Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    line.toLowerCase();
    processSerialCommand(line);
  }

  if (Config::SERIAL_ONLY_TEST_MODE) {
    if (millis() - lastSensorReadMs >= Config::SENSOR_READ_INTERVAL_MS) {
      lastSensorReadMs = millis();
      readSensorDistance();
    }
    return;
  }

  if (isProvisioningActive()) {
    provisioningLoop();
    return;
  }

  ensureWiFiConnected();

  if (WiFi.status() == WL_CONNECTED && millis() - lastNtpUpdateMs >= Config::NTP_UPDATE_INTERVAL_MS) {
    lastNtpUpdateMs = millis();
    timeClient.update();
  }

  mqttEnsureConnected();
  mqttLoop();

  if (millis() - lastSensorReadMs >= Config::SENSOR_READ_INTERVAL_MS) {
    lastSensorReadMs = millis();
    readSensorDistance();
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  const char *ssid = DEFAULT_SSID;
  const char *pass = DEFAULT_PASS;
  if (haveDeviceConfig && deviceCfg.ssid.length() > 0) {
    ssid = deviceCfg.ssid.c_str();
    pass = deviceCfg.pass.c_str();
  }

  Serial.printf("Conectando ao Wi-Fi: %s\n", ssid);
  WiFi.begin(ssid, pass);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000UL) {
    delay(500);
    Serial.print('.');
  }

  Serial.println();
  printWiFiStatus();
}

void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (millis() - lastWifiAttemptMs < Config::WIFI_RETRY_INTERVAL_MS) {
    return;
  }

  lastWifiAttemptMs = millis();
  connectWiFi();
}

void processSerialCommand(String line) {
  if (line.length() == 0) {
    return;
  }

  if (line == "help" || line == "?") {
    Serial.println("Comandos disponiveis:");
    Serial.println("  help      - mostra esta ajuda");
    Serial.println("  provision - inicia o AP de provisionamento");
    Serial.println("  reset     - reinicia o dispositivo");
    return;
  }

  if (line == "provision" || line == "startap") {
    if (!isProvisioningActive()) {
      Serial.println("Comando Serial recebido: iniciando AP de provisionamento...");
      startProvisioningAP();
    } else {
      Serial.println("Provisionamento ja ativo");
    }
    return;
  }

  if (line == "reset") {
    handleResetCommand();
    return;
  }

  Serial.printf("Comando desconhecido: %s\n", line.c_str());
}

void readSensorDistance() {
  if (!sensorIsReady()) {
    Serial.println("Sensor HC-SR04 nao inicializado");
    return;
  }

  float distance = sensorReadDistanceCm(Config::HCSR04_SAMPLE_COUNT);
  bool near = (distance > 0.0f && distance <= Config::HCSR04_NEAR_DISTANCE_CM);

  if (Config::TEST_MODE_EVERY_SECOND) {
    if (distance < 0.0f) {
      Serial.println("[MODO TESTE] Distancia invalida ou sem eco");
    } else {
      Serial.printf("[MODO TESTE] Distancia=%.2f cm | estado=%s\n", distance, near ? "PERTO" : "LONGE");
    }
  }

  if (near && !lastNearState && millis() - lastEventMs >= Config::MIN_EVENT_INTERVAL_MS) {
    lastEventMs = millis();
    publishFeedingEvent(distance);
  }

  lastNearState = near;
}

bool buildTimestamp(char *buffer, size_t bufferSize) {
  if (!timeClient.isTimeSet()) return false;
  time_t epochTime = static_cast<time_t>(timeClient.getEpochTime());
  struct tm timeInfo;
  gmtime_r(&epochTime, &timeInfo);
  int written = strftime(buffer, bufferSize, "%Y-%m-%dT%H:%M:%S", &timeInfo);
  if (written <= 0) return false;
  size_t len = strlen(buffer);
  if (len + 5 < bufferSize) strcat(buffer, ".000Z");
  return true;
}

void publishFeedingEvent(float distanceCm) {
  if (!mqttConnected()) {
    Serial.println("MQTT desconectado, evento nao enviado");
    return;
  }

  char timestamp[32] = {0};
  if (!buildTimestamp(timestamp, sizeof(timestamp))) strcpy(timestamp, "sem_timestamp");

  JsonDocument doc;
  const char *deviceId = DEFAULT_DEVICE_ID;
  const char *token = DEFAULT_TOKEN;
  if (haveDeviceConfig) {
    if (deviceCfg.device_id.length() > 0) deviceId = deviceCfg.device_id.c_str();
    if (deviceCfg.token.length() > 0) token = deviceCfg.token.c_str();
  }

  doc["device_id"] = deviceId;
  doc["token"] = token;
  doc["timestamp"] = timestamp;
  doc["distance_cm"] = distanceCm;
  doc["action"] = "alimentacao";
  doc["event"] = "alimentou";

  char payload[384];
  size_t payloadSize = serializeJson(doc, payload, sizeof(payload));
  if (payloadSize == 0) {
    Serial.println("Falha ao serializar JSON");
    return;
  }

  bool sent = mqttPublishPayload(payload, payloadSize);
  if (sent) {
    Serial.printf("Evento publicado: %s\n", payload);
  } else {
    Serial.println("Falha ao publicar evento MQTT");
  }
}

void printWiFiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi conectado com sucesso");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    return;
  }
  Serial.println("Falha na conexao Wi-Fi");
}

void processIncomingCommand(const String &payload) {
  String command = payload;
  command.trim();
  if (command.startsWith("{")) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, command);
    if (error) {
      Serial.println("JSON de comando invalido");
      return;
    }
    if (doc["device_id"].is<const char*>()) {
      const char *did = doc["device_id"];
      const char *expected = DEFAULT_DEVICE_ID;
      if (haveDeviceConfig && deviceCfg.device_id.length() > 0) expected = deviceCfg.device_id.c_str();
      if (strcmp(did, expected) != 0) {
        Serial.println("Comando destinado a outro dispositivo — ignorando");
        return;
      }
    }
    if (doc["comando"].is<const char*>()) {
      command = doc["comando"].as<String>();
      command.trim();
    }
  }

  command.toLowerCase();
  if (command == "reset") {
    handleResetCommand();
    return;
  }
  Serial.printf("Comando desconhecido: %s\n", command.c_str());
}

void handleResetCommand() {
  Serial.println("Executando comando reset: reiniciando ESP8266");
  delay(1000);
  ESP.restart();
}
