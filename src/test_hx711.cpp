#include <Arduino.h>
#include <HX711.h>

HX711 scale;

const int DATA_PIN_A = 4; // GPIO4 (D2)
const int CLK_PIN_A = 5;  // GPIO5 (D1)
const int DATA_PIN_B = CLK_PIN_A;
const int CLK_PIN_B = DATA_PIN_A;

int attempt = 0;
int mode = 0; // 0 = first pin order, 1 = swapped
int gainIdx = 0;
const int gains[] = {128, 64, 32};

void tryInit() {
  int dataPin = (mode == 0) ? DATA_PIN_A : DATA_PIN_B;
  int clkPin = (mode == 0) ? CLK_PIN_A : CLK_PIN_B;
  int gain = gains[gainIdx % (sizeof(gains)/sizeof(gains[0]))];
  Serial.printf("Inicializando HX711: DOUT=%d, SCK=%d, gain=%d\n", dataPin, clkPin, gain);
  scale.begin(dataPin, clkPin);
  scale.set_gain(gain);
  attempt = 0;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("HX711 test: iniciando");
  tryInit();
}

void loop() {
  if (scale.is_ready()) {
    long val = scale.read();
    Serial.print("HX711 pronto, leitura bruta: ");
    Serial.println(val);
    delay(1000);
    return;
  }

  int dataPin = (mode == 0) ? DATA_PIN_A : DATA_PIN_B;
  int doutState = digitalRead(dataPin);
  Serial.printf("HX711 NAO pronto (DOUT=%d) -- tentativa %d\n", doutState, attempt+1);
  attempt++;

  if (attempt >= 5) {
    gainIdx++;
    if (gainIdx % (sizeof(gains)/sizeof(gains[0])) == 0) {
      mode = 1 - mode; // swap pins after cycling gains
    }
    Serial.println("Reiniciando HX711 com nova configuração...");
    tryInit();
  }

  delay(1000);
}
