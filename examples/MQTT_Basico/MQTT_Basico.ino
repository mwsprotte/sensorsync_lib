/*
 * SensorSync - sending over MQTT / envio por MQTT
 *
 * EN: The broker (Mosquitto) usually runs on the same machine as the platform, port 1883.
 *     The token travels inside the message; the platform drops messages with an invalid token.
 * PT: O broker (Mosquitto) normalmente roda na mesma maquina da plataforma, porta 1883.
 *     O token vai dentro da mensagem; a plataforma descarta mensagens com token invalido.
 */
#include <SensorSync.h>

const char* WIFI_SSID     = "NOME_DA_REDE";
const char* WIFI_PASSWORD = "SENHA_DA_REDE";
const char* SERVIDOR      = "192.168.0.10";
const char* TOKEN         = "ss_COLE_AQUI_O_TOKEN";

SensorSync sensorSync;
unsigned long ultimoEnvio = 0;

void setup() {
  Serial.begin(115200);
  sensorSync.setDebug(Serial);
  sensorSync.setToken(TOKEN);
  sensorSync.useMQTT(SERVIDOR, 1883);   // default topic / topico padrao: "sensorsync"
  // sensorSync.setMqttCredentials("user", "password");  // if the broker requires login / se exigir login
  sensorSync.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  sensorSync.loop();  // keeps the broker connection / mantem a conexao com o broker

  if (millis() - ultimoEnvio >= 1000) {
    ultimoEnvio = millis();
    sensorSync.setValue(0, analogRead(A0));
    if (!sensorSync.send()) {
      Serial.println(sensorSync.lastError());
    }
  }
}
