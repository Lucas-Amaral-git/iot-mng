#include "sensor.h"

#include <Arduino.h>
#include <HX711.h>
#include <climits>

static HX711 loadCell;

void sensorInit(uint8_t doutPin, uint8_t sckPin, float calibration) {
  loadCell.begin(doutPin, sckPin);
  loadCell.set_scale(calibration);
}

bool sensorIsReady() {
  return loadCell.is_ready();
}

void sensorTare(uint8_t times) {
  if (!loadCell.is_ready()) return;
  loadCell.tare(times);
}

long sensorReadRaw(uint8_t times) {
  if (!loadCell.is_ready()) return LONG_MIN;
  return loadCell.read_average(times);
}

double sensorReadValue(uint8_t times) {
  if (!loadCell.is_ready()) return 0.0;
  return loadCell.get_value(times);
}

float sensorReadUnits(uint8_t times) {
  if (!loadCell.is_ready()) return 0.0f;
  return loadCell.get_units(times);
}