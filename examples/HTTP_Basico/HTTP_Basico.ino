/*
 * SensorSync - envio por HTTP
 *
 * 1. Na plataforma, crie o projeto e copie o token do dispositivo (tela Dispositivos).
 * 2. Preencha Wi-Fi, endereco do servidor e token abaixo.
 * 3. Cada chamada a send() grava uma leitura de todas as variaveis definidas com setValue().
 */
#include <SensorSync.h>

const char* WIFI_SSID     = "NOME_DA_REDE";
const char* WIFI_PASSWORD = "SENHA_DA_REDE";
const char* SERVIDOR      = "192.168.0.10";   // IP ou nome do servidor SensorSync
const char* TOKEN         = "ss_COLE_AQUI_O_TOKEN";

SensorSync sensorSync;
unsigned long ultimoEnvio = 0;

void setup() {
  Serial.begin(115200);
  sensorSync.setDebug(Serial);          // opcional: mensagens de diagnostico
  sensorSync.setToken(TOKEN);
  sensorSync.useHTTP(SERVIDOR, 8080);
  sensorSync.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  sensorSync.loop();

  if (millis() - ultimoEnvio >= 5000) {
    ultimoEnvio = millis();

    float temperatura = 20.0 + random(0, 100) / 10.0;  // troque pela leitura real
    float umidade = 50.0 + random(0, 200) / 10.0;

    sensorSync.setValue(0, temperatura);  // variavel 1 do projeto
    sensorSync.setValue(1, umidade);      // variavel 2 do projeto

    if (sensorSync.send()) {
      Serial.println("Enviado");
    } else {
      Serial.print("Falha: ");
      Serial.println(sensorSync.lastError());
    }
  }
}
