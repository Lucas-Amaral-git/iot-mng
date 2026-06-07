#include "sensor.h"

#include <Arduino.h>

static uint8_t trigPin = 0;
static uint8_t echoPin = 0;
static float nearThresholdCm = 0.0f;
static bool initialized = false;

void sensorInit(uint8_t trig, uint8_t echo, float thresholdCm) {
  trigPin = trig;
  echoPin = echo;
  nearThresholdCm = thresholdCm;
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(trigPin, LOW);
  initialized = true;
}

bool sensorIsReady() {
  return initialized;
}

static float readSingleDistanceCm() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, 30000UL);
  if (duration == 0) {
    return -1.0f;
  }

  float distanceCm = static_cast<float>(duration) / 58.0f;
  if (distanceCm <= 0.0f || distanceCm > 400.0f) {
    return -1.0f;
  }
  return distanceCm;
}

float sensorReadDistanceCm(uint8_t samples) {
  if (!initialized) {
    return -1.0f;
  }

  float sum = 0.0f;
  uint8_t validCount = 0;
  for (uint8_t i = 0; i < samples; ++i) {
    float distance = readSingleDistanceCm();
    if (distance > 0.0f) {
      sum += distance;
      ++validCount;
    }
    delay(25);
  }

  if (validCount == 0) {
    return -1.0f;
  }

  return sum / static_cast<float>(validCount);
}
