// storage.h
#pragma once

#include <Arduino.h>

struct DeviceConfig {
  String ssid;
  String pass;
  String mqtt_host;
  uint16_t mqtt_port = 1183;
  String mqtt_user;
  String mqtt_pass;
  String device_id;
  String token;
  bool valid = false;
};

bool loadDeviceConfig(DeviceConfig &cfg);
void saveDeviceConfig(const DeviceConfig &cfg);
void clearDeviceConfig();