/*
 * SensorSync - envio por MQTT
 *
 * O broker (Mosquitto) normalmente roda na mesma maquina da plataforma, porta 1883.
 * O token vai dentro da mensagem; a plataforma descarta mensagens com token invalido.
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
  sensorSync.useMQTT(SERVIDOR, 1883);   // topico padrao: "sensorsync"
  // sensorSync.setMqttCredentials("usuario", "senha");  // se o broker exigir login
  sensorSync.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  sensorSync.loop();  // mantem a conexao com o broker

  if (millis() - ultimoEnvio >= 1000) {
    ultimoEnvio = millis();
    sensorSync.setValue(0, analogRead(A0));
    if (!sensorSync.send()) {
      Serial.println(sensorSync.lastError());
    }
  }
}
