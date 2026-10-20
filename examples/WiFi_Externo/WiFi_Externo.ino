/*
 * SensorSync com Wi-Fi gerenciado pelo seu codigo.
 *
 * Use este modelo quando a conexao exigir algo alem de SSID e senha
 * (IP fixo, rede corporativa, WiFiManager etc.): conecte o Wi-Fi do seu jeito
 * e chame sensorSync.begin() sem argumentos.
 */
#include <SensorSync.h>

const char* SERVIDOR = "192.168.0.10";
const char* TOKEN    = "ss_COLE_AQUI_O_TOKEN";

SensorSync sensorSync;
unsigned long ultimoEnvio = 0;

void conectarWiFi() {
  // Exemplo com IP fixo; substitua pela forma de conexao da sua rede
  IPAddress ip(192, 168, 0, 50), gateway(192, 168, 0, 1), mascara(255, 255, 255, 0);
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, mascara);
  WiFi.begin("NOME_DA_REDE", "SENHA_DA_REDE");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  conectarWiFi();
  sensorSync.setToken(TOKEN);
  sensorSync.useHTTP(SERVIDOR, 8080);
  sensorSync.begin();  // Wi-Fi ja conectado acima
}

void loop() {
  sensorSync.loop();
  if (millis() - ultimoEnvio >= 10000) {
    ultimoEnvio = millis();
    sensorSync.setValue(0, WiFi.RSSI());  // ex.: intensidade do sinal
    if (!sensorSync.send()) Serial.println(sensorSync.lastError());
  }
}
