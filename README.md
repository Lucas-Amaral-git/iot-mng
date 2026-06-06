# Monitoramento Nutricional para Gatos

Projeto ESP8266 com PlatformIO para monitorar o peso da ração usando um módulo HX711 e uma célula de carga.

## Resumo

- Placa: NodeMCU v2 (ESP8266)
- Sensor: HX711 + célula de carga (load cell)
- Comunicação: Wi-Fi + MQTT
- Firmware: PlatformIO

> Observação: este projeto usa um sensor de peso (célula de carga) com HX711. Não há um sensor PIR de presença implementado no código atual.

## Conexões de hardware

### Conexão HX711 -> NodeMCU

- `HX711 DOUT` -> `NodeMCU D4` (GPIO2)
- `HX711 SCK` -> `NodeMCU D5` (GPIO14)
- `HX711 VCC` -> `NodeMCU 3V3`
- `HX711 GND` -> `NodeMCU GND`

### Conexão da célula de carga ao HX711

As cores podem variar conforme o modelo da célula de carga. Um esquema comum é:

- `célula de carga +V` (excitação) -> `HX711 E+`
- `célula de carga -V` (excitação) -> `HX711 E-`
- `célula de carga +S` (sinal) -> `HX711 A+` ou `HX711 B+`
- `célula de carga -S` (sinal) -> `HX711 A-` ou `HX711 B-`

> Ajuste a fiação conforme o seu módulo HX711 e a sua célula de carga. O importante no firmware é que o HX711 seja alimentado corretamente e conectado nos pinos D4/D5 do NodeMCU.

## Pinos usados no firmware

No arquivo `src/main.cpp` está configurado:

- `Config::HX711_DOUT_PIN = 4` (NodeMCU D4)
- `Config::HX711_SCK_PIN = 5` (NodeMCU D5)
- `Config::HX711_CALIBRATION_FACTOR = -7050.0f`

Se você trocar os pinos, atualize essas constantes.

## Como compilar e enviar

1. Abra o projeto em PlatformIO.
2. Conecte o NodeMCU ao computador via USB.
3. Execute `Upload` no PlatformIO.
4. Abra o monitor serial a `115200` baud.

## Comandos via Serial

No monitor serial, você pode usar os seguintes comandos:

- `help` — mostrar comandos disponíveis
- `tare` — zerar a balança
- `calibrar <peso_gramas>` — calibrar com um peso conhecido
- `scale <peso_gramas>` — ajustar escala
- `provision` — iniciar modo de provisionamento Wi-Fi, se disponível

## Configuração Wi-Fi e MQTT

O projeto carrega a configuração salva no dispositivo usando o módulo de provisionamento. Se não houver configuração, o firmware entrará em modo de provisionamento quando você usar o comando serial `provision`.

Os valores padrão em `src/main.cpp` são:

- `DEFAULT_DEVICE_ID = "esp8266_pote_01"`
- `DEFAULT_TOKEN = "TOKEN_SECRETO"`
- `DEFAULT_SSID = ""`
- `DEFAULT_PASS = ""`

A configuração real de Wi-Fi e MQTT é salva localmente após o provisionamento.

## Dependências

O arquivo `platformio.ini` já inclui estas bibliotecas:

- `PubSubClient`
- `ArduinoJson`
- `HX711`
- `NTPClient`

## Dicas de uso

- Coloque a célula de carga em uma base estável para evitar ruído.
- Faça o `tare` sem peso para zerar corretamente.
- Use um peso conhecido para calibrar o fator de conversão.
- Se o HX711 não responder, verifique a alimentação e os cabos `DOUT/SCK`.

## Observações

Se você precisar usar um sensor de presença (PIR) no futuro, será necessário adicionar o sensor ao hardware e ajustar o código para ler um pino digital separado. Atualmente, o firmware só usa o HX711 para monitorar peso.
