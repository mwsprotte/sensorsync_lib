/*
 * SensorSync - benchmark (experiments) / benchmark (experimentos)
 *
 * EN: Sends TOTAL messages at a fixed interval in benchmark mode (each one carries "seq" and "ts")
 *     and prints one CSV line per send on the serial port (115200). Capture it with
 *     bench/serial_log.py (sensorsync repository) and enable SENSORSYNC_METRICS=true on the server
 *     to cross client and server timings.
 * PT: Envia TOTAL mensagens num intervalo fixo no modo benchmark (cada uma leva "seq" e "ts")
 *     e imprime uma linha CSV por envio na serial (115200). Capture com bench/serial_log.py
 *     (repositorio sensorsync) e ligue SENSORSYNC_METRICS=true no servidor para cruzar os tempos.
 *
 * CSV: seq,ok,send_us,connect_us,heap,max_block,rssi,error
 *   send_us     duration of send() / duracao do send()
 *   connect_us  last broker connection, MQTT only (includes TLS handshake) / ultima conexao ao broker
 *   heap        free heap after the send / heap livre apos o envio
 *   max_block   largest free block / maior bloco livre
 */
#include <SensorSync.h>

// ---- Configuration / Configuracao ------------------------------------------
const char* WIFI_SSID     = "NOME_DA_REDE";
const char* WIFI_PASSWORD = "SENHA_DA_REDE";
const char* SERVIDOR      = "192.168.0.10";
const char* TOKEN         = "ss_COLE_AQUI_O_TOKEN";

const bool USAR_MQTT = false;              // false = HTTP, true = MQTT
const bool USAR_TLS  = false;              // true = HTTPS (8443) / MQTT TLS (8883)
const bool REUSAR_CONEXAO = true;          // HTTP: keep the connection open / manter a conexao aberta
const unsigned long INTERVALO_MS = 1000;   // send period / periodo de envio
const uint32_t TOTAL = 300;                // messages / mensagens
const uint8_t N_VALORES = 5;               // values per message (<= project variables) / valores por mensagem

// Server CA (only with USAR_TLS); the platform shows it in /certificado.html / CA do servidor
static const char CA_CERT[] = R"PEM(
-----BEGIN CERTIFICATE-----
COLE AQUI O CONTEUDO DE ca.crt / PASTE THE CONTENT OF ca.crt HERE
-----END CERTIFICATE-----
)PEM";
// ------------------------------------------------------------------------------

SensorSync sensorSync;
unsigned long ultimoEnvio = 0;
uint32_t enviados = 0, sucessos = 0;
uint64_t somaUs = 0;

static uint32_t maxBlock() {
#if defined(ESP8266)
  return ESP.getMaxFreeBlockSize();
#else
  return ESP.getMaxAllocHeap();
#endif
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.printf("# SensorSync benchmark | lib %s | %s | %s | TLS %s | intervalo %lu ms | %u valores\n",
                SENSORSYNC_VERSION,
#if defined(ESP8266)
                "ESP8266",
#else
                "ESP32",
#endif
                USAR_MQTT ? "MQTT" : "HTTP", USAR_TLS ? "sim" : "nao", INTERVALO_MS, N_VALORES);
  Serial.printf("# heap livre no inicio: %u B\n", (unsigned)ESP.getFreeHeap());

  sensorSync.setToken(TOKEN);
  if (USAR_MQTT) {
    sensorSync.useMQTT(SERVIDOR, USAR_TLS ? 8883 : 1883);
  } else {
    sensorSync.useHTTP(SERVIDOR, USAR_TLS ? 8443 : 8080);
  }
  if (USAR_TLS) {
    sensorSync.useTLS(CA_CERT);
  }
  sensorSync.setReuseConnection(REUSAR_CONEXAO);
  sensorSync.enableBenchmark();
  sensorSync.begin(WIFI_SSID, WIFI_PASSWORD);

  // Waits up to 15 s for SNTP (without it "ts" is not sent) / Espera o SNTP (sem ele "ts" nao vai)
  unsigned long t = millis();
  while (!SensorSync::clockSynced() && millis() - t < 15000) {
    sensorSync.loop();
    delay(100);
  }
  Serial.printf("# relogio sincronizado: %s\n", SensorSync::clockSynced() ? "sim" : "NAO (sem ts)");
  Serial.println("seq,ok,send_us,connect_us,heap,max_block,rssi,error");
}

void loop() {
  sensorSync.loop();
  if (enviados >= TOTAL || millis() - ultimoEnvio < INTERVALO_MS) {
    return;
  }
  ultimoEnvio = millis();

  for (uint8_t i = 0; i < N_VALORES; i++) {
    sensorSync.setValue(i, random(0, 10000) / 100.0);
  }
  bool ok = sensorSync.send();
  enviados++;
  if (ok) {
    sucessos++;
    somaUs += sensorSync.lastSendMicros();
  } else {
    sensorSync.clear();
  }
  Serial.printf("%lu,%d,%lu,%lu,%u,%u,%d,\"%s\"\n", (unsigned long)sensorSync.sequence(), ok ? 1 : 0,
                (unsigned long)sensorSync.lastSendMicros(), (unsigned long)sensorSync.lastConnectMicros(),
                (unsigned)ESP.getFreeHeap(), (unsigned)maxBlock(), (int)WiFi.RSSI(), ok ? "" : sensorSync.lastError());

  if (enviados == TOTAL) {
    Serial.printf("# fim: %lu/%lu ok, send() medio %lu us\n", (unsigned long)sucessos, (unsigned long)enviados,
                  (unsigned long)(sucessos ? somaUs / sucessos : 0));
  }
}
