// sensor.h
#pragma once

#include <Arduino.h>

void sensorInit(uint8_t trigPin, uint8_t echoPin, float nearThresholdCm);
bool sensorIsReady();
float sensorReadDistanceCm(uint8_t samples = 3);
