// mqtt.h
#pragma once

#include <Arduino.h>
#include <Client.h>
#include "storage.h"

using IncomingHandler = void (*)(const String &payload);

void mqttInit(Client &networkClient, const DeviceConfig &cfg, IncomingHandler handler);
void mqttLoop();
void mqttEnsureConnected();
bool mqttConnected();
bool mqttPublishPayload(const char *payload, size_t size);
const String &mqttTopicPublish();
const String &mqttTopicCommand();