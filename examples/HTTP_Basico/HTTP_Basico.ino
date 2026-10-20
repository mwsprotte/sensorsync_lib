/*
 * SensorSync - sending over HTTP / envio por HTTP
 *
 * EN: 1. Create the project on the platform and copy the device token (Devices tab).
 *     2. Fill in Wi-Fi, server address and token below.
 *     3. Each send() stores one reading of every variable set with setValue().
 *
 * PT: 1. Na plataforma, crie o projeto e copie o token do dispositivo (aba Dispositivos).
 *     2. Preencha Wi-Fi, endereco do servidor e token abaixo.
 *     3. Cada send() grava uma leitura de todas as variaveis definidas com setValue().
 */
#include <SensorSync.h>

const char* WIFI_SSID     = "NOME_DA_REDE";
const char* WIFI_PASSWORD = "SENHA_DA_REDE";
const char* SERVIDOR      = "192.168.0.10";   // SensorSync server IP or hostname / IP ou nome do servidor
const char* TOKEN         = "ss_COLE_AQUI_O_TOKEN";

SensorSync sensorSync;
unsigned long ultimoEnvio = 0;

void setup() {
  Serial.begin(115200);
  sensorSync.setDebug(Serial);          // optional diagnostics / diagnostico opcional
  sensorSync.setToken(TOKEN);
  sensorSync.useHTTP(SERVIDOR, 8080);
  sensorSync.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  sensorSync.loop();

  if (millis() - ultimoEnvio >= 5000) {
    ultimoEnvio = millis();

    float temperatura = 20.0 + random(0, 100) / 10.0;  // replace with a real reading / leitura real
    float umidade = 50.0 + random(0, 200) / 10.0;

    sensorSync.setValue(0, temperatura);  // project variable 1 / variavel 1 do projeto
    sensorSync.setValue(1, umidade);      // project variable 2 / variavel 2 do projeto

    if (sensorSync.send()) {
      Serial.println("OK");
    } else {
      Serial.print("Erro/Error: ");
      Serial.println(sensorSync.lastError());
    }
  }
}
