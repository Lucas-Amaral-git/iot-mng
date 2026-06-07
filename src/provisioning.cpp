#include "provisioning.h"
#include "storage.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#if defined(ESP8266)
#include <ESP8266WebServer.h>
using WebServerType = ESP8266WebServer;
#include <ESP8266WiFi.h>
#else
#include <WebServer.h>
using WebServerType = WebServer;
#include <WiFi.h>
#endif

static WebServerType server(80);
static bool active = false;

static String buildFormHtml() {
  String html = "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'/></head><body>";
  html += "<h3>Provisionamento ESP8266</h3>";
  html += "<form action='/save' method='POST'>";
  html += "SSID:<br><input name='ssid'><br>";
  html += "Senha:<br><input name='pass'><br>";
  html += "MQTT Host:<br><input name='mqtt_host'><br>";
  html += "MQTT Porta:<br><input name='mqtt_port' value='1183'><br>";
  html += "MQTT Usuario:<br><input name='mqtt_user'><br>";
  html += "MQTT Senha:<br><input name='mqtt_pass'><br>";
  html += "Device ID:<br><input name='device_id'><br>";
  html += "Token:<br><input name='token'><br>";
  html += "<br><input type='submit' value='Salvar'>";
  html += "</form>";
  html += "<p><a href='/clear'>Esquecer credenciais</a></p>";
  html += "</body></html>";
  return html;
}

void handleRoot() {
  String html = buildFormHtml();
  server.send(200, "text/html", html);
}

void handleSave() {
  DeviceConfig cfg;
  cfg.ssid = server.arg("ssid");
  cfg.pass = server.arg("pass");
  cfg.mqtt_host = server.arg("mqtt_host");
  cfg.mqtt_port = static_cast<uint16_t>(server.arg("mqtt_port").toInt());
  cfg.mqtt_user = server.arg("mqtt_user");
  cfg.mqtt_pass = server.arg("mqtt_pass");
  cfg.device_id = server.arg("device_id");
  cfg.token = server.arg("token");
  cfg.valid = true;

  saveDeviceConfig(cfg);

  server.send(200, "text/html", "Configuracao salva. Reiniciando...");
  delay(1000);
  ESP.restart();
}

void handleClear() {
  clearDeviceConfig();
  server.send(200, "text/html", "Credenciais apagadas. Reiniciando...");
  delay(1000);
  ESP.restart();
}

void handleInfo() {
  DeviceConfig cfg;
  String html = "<html><body>";
  if (loadDeviceConfig(cfg)) {
    html += "<h3>Configuracao atual</h3>";
    html += "SSID: "; html += cfg.ssid; html += "<br>";
    html += "MQTT Host: "; html += cfg.mqtt_host; html += "<br>";
    html += "MQTT Port: "; html += String(cfg.mqtt_port); html += "<br>";
    html += "MQTT User: "; html += String(cfg.mqtt_user); html += "<br>";
    html += "MQTT Pass: "; html += String(cfg.mqtt_pass); html += "<br>";
    html += "Device ID: "; html += cfg.device_id; html += "<br>";
  } else {
    html += "<p>Nenhuma configuracao salva.</p>";
  }
  html += "<p><a href='/'>Voltar</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSample() {
  DeviceConfig cfg;
  JsonDocument doc;
  if (loadDeviceConfig(cfg)) {
    doc["device_id"] = cfg.device_id;
    doc["token"] = cfg.token;
  } else {
    doc["device_id"] = "esp8266_teste";
    doc["token"] = "TOKEN_DE_TESTE";
  }
  doc["timestamp"] = "2026-05-26T12:00:00.000Z";
  doc["distance_cm"] = 25.0;
  doc["action"] = "alimentacao";
}

void startProvisioningAP() {
  if (active) return;

  Serial.println("Iniciando AP de provisionamento: ESP8266-Setup");
  WiFi.mode(WIFI_AP);
#if defined(ESP32)
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
#endif
  WiFi.softAP("ESP8266-Setup");
  Serial.print("IP do AP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/clear", HTTP_GET, handleClear);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/sample", HTTP_GET, handleSample);
  server.begin();
  active = true;
}

bool isProvisioningActive() {
  return active;
}

void stopProvisioning() {
  if (!active) return;
  server.stop();
  WiFi.softAPdisconnect(true);
  active = false;
}

void provisioningLoop() {
  if (active) server.handleClient();
}