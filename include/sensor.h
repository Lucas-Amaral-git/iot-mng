// sensor.h
#pragma once

#include <Arduino.h>

void sensorInit(uint8_t doutPin, uint8_t sckPin, float calibration);
bool sensorIsReady();
void sensorTare(uint8_t times = 20);
long sensorReadRaw(uint8_t times = 1);
double sensorReadValue(uint8_t times = 1);
float sensorReadUnits(uint8_t times = 1);