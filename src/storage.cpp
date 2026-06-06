#include "storage.h"

#include <ArduinoJson.h>

#if defined(ESP8266)
#include <LittleFS.h>
static const char *CONFIG_PATH = "/config.json";

static bool ensureFilesystemMounted() {
  static bool mounted = false;
  static bool attempted = false;
  if (!attempted) {
    attempted = true;
    mounted = LittleFS.begin();
    if (!mounted) {
      mounted = LittleFS.begin();
    }
  }
  return mounted;
}

bool loadDeviceConfig(DeviceConfig &cfg) {
  if (!ensureFilesystemMounted()) {
    return false;
  }
  if (!LittleFS.exists(CONFIG_PATH)) {
    return false;
  }

  File file = LittleFS.open(CONFIG_PATH, "r");
  if (!file) {
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    return false;
  }

  cfg.ssid = doc["ssid"] | "";
  cfg.pass = doc["pass"] | "";
  cfg.mqtt_host = doc["mqtt_host"] | "";
  cfg.mqtt_port = static_cast<uint16_t>(doc["mqtt_port"] | 1183);
  cfg.mqtt_user = doc["mqtt_user"] | "";
  cfg.mqtt_pass = doc["mqtt_pass"] | "";
  cfg.device_id = doc["device_id"] | "";
  cfg.token = doc["token"] | "";
  cfg.valid = cfg.ssid.length() > 0;
  return cfg.valid;
}

void saveDeviceConfig(const DeviceConfig &cfg) {
  if (!ensureFilesystemMounted()) {
    return;
  }

  JsonDocument doc;
  doc["ssid"] = cfg.ssid;
  doc["pass"] = cfg.pass;
  doc["mqtt_host"] = cfg.mqtt_host;
  doc["mqtt_port"] = cfg.mqtt_port;
  doc["mqtt_user"] = cfg.mqtt_user;
  doc["mqtt_pass"] = cfg.mqtt_pass;
  doc["device_id"] = cfg.device_id;
  doc["token"] = cfg.token;

  File file = LittleFS.open(CONFIG_PATH, "w");
  if (!file) {
    return;
  }

  serializeJsonPretty(doc, file);
  file.close();
}

void clearDeviceConfig() {
  if (!ensureFilesystemMounted()) {
    return;
  }
  if (LittleFS.exists(CONFIG_PATH)) {
    LittleFS.remove(CONFIG_PATH);
  }
}

#else

#include <Preferences.h>

static const char *NAMESPACE = "config";

bool loadDeviceConfig(DeviceConfig &cfg) {
  Preferences prefs;
  prefs.begin(NAMESPACE, true);

  String ssid = prefs.getString("ssid", "");
  if (ssid.length() == 0) {
    prefs.end();
    return false;
  }

  cfg.ssid = ssid;
  cfg.pass = prefs.getString("pass", "");
  cfg.mqtt_host = prefs.getString("mqtt_host", "");
  cfg.mqtt_port = static_cast<uint16_t>(prefs.getInt("mqtt_port", 1183));
  cfg.mqtt_user = prefs.getString("mqtt_user", "");
  cfg.mqtt_pass = prefs.getString("mqtt_pass", "");
  cfg.device_id = prefs.getString("device_id", "");
  cfg.token = prefs.getString("token", "");
  cfg.valid = true;

  prefs.end();
  return true;
}

void saveDeviceConfig(const DeviceConfig &cfg) {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);

  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.pass);
  prefs.putString("mqtt_host", cfg.mqtt_host);
  prefs.putInt("mqtt_port", cfg.mqtt_port);
  prefs.putString("mqtt_user", cfg.mqtt_user);
  prefs.putString("mqtt_pass", cfg.mqtt_pass);
  prefs.putString("device_id", cfg.device_id);
  prefs.putString("token", cfg.token);

  prefs.end();
}

void clearDeviceConfig() {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.clear();
  prefs.end();
}

#endif