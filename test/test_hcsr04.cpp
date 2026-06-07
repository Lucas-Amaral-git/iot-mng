#include <Arduino.h>
#include "sensor.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("HC-SR04 test: iniciando");
  sensorInit(12, 13, 50.0f);
}

void loop() {
  if (!sensorIsReady()) {
    Serial.println("Sensor HC-SR04 nao inicializado");
    delay(1000);
    return;
  }

  float distance = sensorReadDistanceCm(3);
  if (distance < 0.0f) {
    Serial.println("Distancia invalida ou sem eco");
  } else {
    Serial.printf("Distancia: %.2f cm
", distance);
  }
  delay(1000);
}
