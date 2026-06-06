#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HX711.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "storage.h"
#include "provisioning.h"
#include "sensor.h"
#include "mqtt.h"

namespace Config {
constexpr unsigned long SENSOR_READ_INTERVAL_MS = 5000UL;
constexpr unsigned long PROCESS_INTERVAL_MS = 600000UL;
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 10000UL;
constexpr unsigned long NTP_UPDATE_INTERVAL_MS = 60000UL;
constexpr size_t WINDOW_SIZE = 60;
constexpr int TIMEZONE_OFFSET_SECONDS = -3 * 3600;
constexpr bool TEST_MODE_EVERY_SECOND = true;
constexpr bool SERIAL_ONLY_TEST_MODE = false;
constexpr uint8_t HX711_DOUT_PIN = 4;
constexpr uint8_t HX711_SCK_PIN = 5;
constexpr float HX711_CALIBRATION_FACTOR = -7050.0f;
}

WiFiUDP ntpUdp;
NTPClient timeClient(ntpUdp, "pool.ntp.org", Config::TIMEZONE_OFFSET_SECONDS, Config::NTP_UPDATE_INTERVAL_MS);
WiFiClient wifiClient;

DeviceConfig deviceCfg;
bool haveDeviceConfig = false;

float readings[Config::WINDOW_SIZE] = {0.0f};
size_t readingsCount = 0;
size_t readingsIndex = 0;
bool hasLastValidWeight = false;
float lastValidWeight = 0.0f;
long lastValidRaw = 0;

unsigned long lastSensorReadMs = 0;
unsigned long lastProcessingMs = 0;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastNtpUpdateMs = 0;

void connectWiFi();
void ensureWiFiConnected();
void processSerialCommand(String line);
void processIncomingCommand(const String &payload);
void handleCalibrateCommand();
void handleResetCommand();
void handleScaleCommand(const String &payload);
void readSensorSample();
void storeReading(float value);
float calculateMean(const float *values, size_t count);
float calculateMedian(const float *values, size_t count);
bool buildTimestamp(char *buffer, size_t bufferSize);
void publishProcessedData(float median, float mean);
void publishTestReading(float weight);
void processWindowIfReady();
void printWiFiStatus();

const char *DEFAULT_DEVICE_ID = "esp8266_pote_01";
const char *DEFAULT_TOKEN = "TOKEN_SECRETO";
const char *DEFAULT_SSID = "";
const char *DEFAULT_PASS = "";

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("Iniciando monitoramento de racao com ESP8266 + HX711");
  Serial.println("Comandos Serial: help | tare | calibrar <peso_gramas> | scale <peso_gramas> | provision");

  if (Config::SERIAL_ONLY_TEST_MODE) {
    sensorInit(Config::HX711_DOUT_PIN, Config::HX711_SCK_PIN, Config::HX711_CALIBRATION_FACTOR);
    sensorTare();
    lastSensorReadMs = millis();
    Serial.println("[MODO TESTE] Apenas Serial ativo. Wi-Fi/MQTT/provisioning desativados. Leitura a cada 5 segundos.");
    return;
  }

  haveDeviceConfig = loadDeviceConfig(deviceCfg);
  if (!haveDeviceConfig) {
    delay(250);
    Serial.println("Nenhuma configuracao salva.");
    Serial.println("Para iniciar o AP de provisionamento digite 'provision' no Serial monitor ou pressione o botao de provisionamento.");
  }

  sensorInit(Config::HX711_DOUT_PIN, Config::HX711_SCK_PIN, Config::HX711_CALIBRATION_FACTOR);
  sensorTare();

  WiFi.mode(WIFI_STA);
  connectWiFi();

  timeClient.begin();
  timeClient.update();

  mqttInit(wifiClient, deviceCfg, [](const String &payload) { processIncomingCommand(payload); });

  lastSensorReadMs = millis();
  lastProcessingMs = millis();
  lastNtpUpdateMs = millis();

  if (Config::TEST_MODE_EVERY_SECOND) {
    Serial.println("[MODO TESTE] A cada 5 segundos o peso sera lido, impresso no Serial e enviado ao MQTT.");
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
      readSensorSample();
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
    readSensorSample();
  }

  if (!Config::TEST_MODE_EVERY_SECOND && millis() - lastProcessingMs >= Config::PROCESS_INTERVAL_MS) {
    lastProcessingMs = millis();
    processWindowIfReady();
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
    Serial.println("  help                 - mostra esta ajuda");
    Serial.println("  tare                 - zera a balanca");
    Serial.println("  calibrar <peso>      - calcula fator com peso conhecido");
    Serial.println("  scale <peso>         - alias de calibrar <peso>");
    Serial.println("  provision            - inicia o AP de provisionamento");
    return;
  }

  if (line == "tare") {
    Serial.println("Comando Serial recebido: tare");
    sensorTare();
    Serial.println("Tara executada");
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

  if (line.startsWith("calibrar ") || line.startsWith("scale ")) {
    handleScaleCommand(line);
    return;
  }

  Serial.printf("Comando desconhecido: %s\n", line.c_str());
}

void readSensorSample() {
  int doutState = digitalRead(Config::HX711_DOUT_PIN);
  const char *signalState = (doutState == LOW) ? "BAIXO" : "ALTO";

  if (!sensorIsReady()) {
    if (hasLastValidWeight) {
      Serial.printf("%s HX711 sem dado pronto | DOUT=%s (%d) | ultimo peso valido=%.2f | ultimo bruto=%ld\n",
                    Config::TEST_MODE_EVERY_SECOND ? "[MODO TESTE]" : "",
                    signalState,
                    doutState,
                    lastValidWeight,
                    lastValidRaw);
    } else {
      Serial.printf("%s HX711 sem dado pronto | DOUT=%s (%d) | ainda sem leitura valida\n",
                    Config::TEST_MODE_EVERY_SECOND ? "[MODO TESTE]" : "",
                    signalState,
                    doutState);
    }
    return;
  }

  long raw = sensorReadRaw(1);
  float weight = sensorReadUnits(1);
  if (raw != LONG_MIN) {
    lastValidRaw = raw;
  }
  lastValidWeight = weight;
  hasLastValidWeight = true;
  storeReading(weight);

  if (!Config::TEST_MODE_EVERY_SECOND) {
    return;
  }

  Serial.printf("[MODO TESTE] HX711 pronto | DOUT=%s (%d) | bruto=%ld | peso=%.2f\n",
                signalState,
                doutState,
                lastValidRaw,
                weight);
  publishTestReading(weight);
}

void storeReading(float value) {
  readings[readingsIndex] = value;
  readingsIndex = (readingsIndex + 1) % Config::WINDOW_SIZE;

  if (readingsCount < Config::WINDOW_SIZE) {
    ++readingsCount;
  }
}

float calculateMean(const float *values, size_t count) {
  if (count == 0) return 0.0f;
  float sum = 0.0f;
  for (size_t i = 0; i < count; ++i) sum += values[i];
  return sum / static_cast<float>(count);
}

float calculateMedian(const float *values, size_t count) {
  if (count == 0) return 0.0f;
  float sorted[Config::WINDOW_SIZE];
  for (size_t i = 0; i < count; ++i) sorted[i] = values[i];
  for (size_t i = 0; i < count - 1; ++i) {
    for (size_t j = i + 1; j < count; ++j) {
      if (sorted[j] < sorted[i]) {
        float t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t;
      }
    }
  }
  if (count % 2 == 0) return (sorted[count / 2 - 1] + sorted[count / 2]) / 2.0f;
  return sorted[count / 2];
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

void publishTestReading(float weight) {
  char timestamp[32] = {0};
  if (!buildTimestamp(timestamp, sizeof(timestamp))) strcpy(timestamp, "sem_timestamp");

  Serial.printf("[MODO TESTE] peso=%.2f | timestamp=%s\n", weight, timestamp);

  if (!mqttConnected()) {
    Serial.println("[MODO TESTE] MQTT desconectado, leitura nao enviada");
    return;
  }

  JsonDocument doc;
  const char *deviceId = DEFAULT_DEVICE_ID;
  const char *token = DEFAULT_TOKEN;
  if (haveDeviceConfig) {
    if (deviceCfg.device_id.length() > 0) deviceId = deviceCfg.device_id.c_str();
    if (deviceCfg.token.length() > 0) token = deviceCfg.token.c_str();
  }

  doc["device_id"] = deviceId;
  doc["token"] = token;
  doc["peso"] = weight;
  doc["peso_media"] = weight;
  doc["timestamp"] = timestamp;
  doc["action"] = "teste_leitura_por_segundo";

  char payload[384];
  size_t payloadSize = serializeJson(doc, payload, sizeof(payload));
  if (payloadSize == 0) {
    Serial.println("Falha ao serializar JSON");
    return;
  }

  bool sent = mqttPublishPayload(payload, payloadSize);
  if (sent) {
    Serial.printf("[MODO TESTE] Leitura a cada 1s -> peso=%.2f | timestamp=%s | payload=%s\n", weight, timestamp, payload);
  } else {
    Serial.printf("[MODO TESTE] Falha ao publicar leitura: peso=%.2f\n", weight);
  }
}

void publishProcessedData(float median, float mean) {
  if (!mqttConnected()) {
    Serial.println("MQTT desconectado, envio adiado");
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
  doc["peso"] = median;
  doc["peso_media"] = mean;
  doc["timestamp"] = timestamp;
  doc["action"] = "estabilidade";

  char payload[384];
  size_t payloadSize = serializeJson(doc, payload, sizeof(payload));
  if (payloadSize == 0) {
    Serial.println("Falha ao serializar JSON");
    return;
  }

  bool sent = mqttPublishPayload(payload, payloadSize);
  if (sent) Serial.printf("Payload MQTT enviado: %s\n", payload);
  else Serial.println("Falha ao publicar payload MQTT");
}

void processWindowIfReady() {
  if (readingsCount == 0) {
    Serial.println("Janela de leitura ainda vazia");
    return;
  }

  float values[Config::WINDOW_SIZE];
  for (size_t i = 0; i < readingsCount; ++i) {
    size_t idx = (readingsIndex + Config::WINDOW_SIZE - readingsCount + i) % Config::WINDOW_SIZE;
    values[i] = readings[idx];
  }

  float mean = calculateMean(values, readingsCount);
  float median = calculateMedian(values, readingsCount);
  Serial.println("==============================");
  Serial.printf("[MODO PRODUCAO] Processamento da janela (%u leituras)\n", (unsigned)readingsCount);
  Serial.printf("Media: %.2f\n", mean);
  Serial.printf("Mediana: %.2f\n", median);
  Serial.println("==============================");
  publishProcessedData(median, mean);
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
      Serial.println("JSON de comando inválido");
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
    if (!doc["dados"].isNull()) {
      Serial.println("Dados do comando:");
      serializeJson(doc["dados"], Serial);
      Serial.println();
    }
  }

  command.toLowerCase();
  if (command == "calibrar" || command == "calibrar_sensor") {
    handleCalibrateCommand();
    return;
  }
  if (command == "reset") {
    handleResetCommand();
    return;
  }
  Serial.printf("Comando desconhecido: %s\n", command.c_str());
}

void handleCalibrateCommand() {
  Serial.println("Executando comando calibrar: tare da celula de carga");
  sensorTare();
}

void handleScaleCommand(const String &payload) {
  String command = payload;
  command.trim();
  int spaceIndex = command.indexOf(' ');
  if (spaceIndex < 0) {
    Serial.println("Uso: calibrar <peso_em_gramas>");
    return;
  }

  float knownWeight = command.substring(spaceIndex + 1).toFloat();
  if (knownWeight <= 0.0f) {
    Serial.println("Peso informado invalido");
    return;
  }

  if (!sensorIsReady()) {
    Serial.println("HX711 nao pronto; nao foi possivel calcular o fator de calibracao");
    return;
  }

  double value = sensorReadValue(10);
  float factor = static_cast<float>(value / knownWeight);
  Serial.println("=== Calibracao HX711 ===");
  Serial.printf("Peso conhecido: %.2f\n", knownWeight);
  Serial.printf("Leitura sem tara (media): %.2f\n", value);
  Serial.printf("Fator sugerido para HX711_CALIBRATION_FACTOR: %.2f\n", factor);
  Serial.println("Copie esse fator para Config::HX711_CALIBRATION_FACTOR depois de zerar a balanca com tare.");
}

void handleResetCommand() {
  Serial.println("Executando comando reset: reiniciando ESP8266");
  delay(1000);
  ESP.restart();
}