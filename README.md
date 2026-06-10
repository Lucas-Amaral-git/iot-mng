# Monitoramento de Alimentacao para Gatos

Projeto ESP8266 com PlatformIO para detectar quando o pet se aproxima do comedouro usando um sensor HC-SR04.

## Resumo

- Placa: NodeMCU v2 (ESP8266)
- Sensor: HC-SR04 (ultrassonico)
- Comunicacao: Wi-Fi + MQTT
- Firmware: PlatformIO

Este firmware detecta quando o animal se aproxima do comedouro e publica um evento de "alimentacao" via MQTT. Nao ha mais uso de balanca ou celula de carga.

## Conexoes de hardware

### Conexao HC-SR04 -> NodeMCU

- `HC-SR04 TRIG` -> `NodeMCU D6` (GPIO12)
- `HC-SR04 ECHO` -> `NodeMCU D7` (GPIO13)
- `HC-SR04 VCC` -> `NodeMCU 5V`
- `HC-SR04 GND` -> `NodeMCU GND`

- `Botao de provisionamento` -> `NodeMCU D5` (GPIO14) usar `INPUT_PULLUP`

> Ajuste os pinos se necessario, mas atualize os valores de `Config::HCSR04_TRIG_PIN`, `Config::HCSR04_ECHO_PIN` e `Config::PROVISION_BUTTON_PIN` em `src/main.cpp`.

## Comandos via Serial

- `help` - mostra comandos disponiveis
- `provision` - inicia o AP de provisionamento
- `reset` - reinicia o dispositivo

> O provisionamento também pode ser iniciado pelo botao conectado em `NodeMCU D5`.

## Formato MQTT

O payload enviado ao broker agora contem:

- `device_id`
- `token`
- `timestamp`
- `distance_cm`
- `action` = `alimentacao`
- `event` = `alimentou`

O topico de publicacao e `pet/<device_id>/alimentacao`.

## Configuracao Wi-Fi e MQTT

A configuracao e salva localmente pelo provisionamento. Se nao houver configuracao valida, use `provision` no monitor serial para iniciar o AP e enviar os dados.

## Dependencias

O `platformio.ini` usa:

- `PubSubClient`
- `ArduinoJson`
- `NTPClient`

## Dicas de uso

- Posicione o HC-SR04 de forma que ele detecte a aproximacao do animal ao comedouro.
- Teste a distancia minima e ajuste `Config::HCSR04_NEAR_DISTANCE_CM` para reduzir falsos positivos.
- Verifique se a alimentacao do HC-SR04 e estavel e/ou use uma fonte 5V adequada.
