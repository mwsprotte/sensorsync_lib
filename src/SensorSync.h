/*
 * SensorSync - Arduino client (ESP8266/ESP32) for the SensorSync IoT platform.
 * Cliente Arduino (ESP8266/ESP32) para a plataforma IoT SensorSync.
 * https://github.com/mwsprotte/sensorsync
 * Author / Autor: Matheus Willian Sprotte (https://github.com/mwsprotte)
 *
 * Basic usage / Uso basico:
 *   SensorSync sensorSync;
 *   sensorSync.setToken("ss_...");
 *   sensorSync.useHTTP("10.0.0.104", 8080);      // or/ou useMQTT("10.0.0.104", 1883)
 *   // sensorSync.useTLS(CA_CERT);               // server with TLS / servidor com TLS (8443 / 8883)
 *   sensorSync.begin("ssid", "password");
 *   ...
 *   sensorSync.loop();                           // every loop() / em todo loop()
 *   sensorSync.setValue(0, temperature);
 *   sensorSync.setValue(1, humidity);
 *   if (!sensorSync.send()) Serial.println(sensorSync.lastError());
 */
#ifndef SENSORSYNC_H
#define SENSORSYNC_H

#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#else
#error "SensorSync supports only ESP8266 and ESP32 boards (suporta apenas ESP8266 e ESP32)."
#endif

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <sys/time.h>

#define SENSORSYNC_VERSION "1.2.0"

// Maximum number of variables per send (the platform accepts up to 20 per project).
// Quantidade maxima de variaveis por envio (a plataforma aceita ate 20 por projeto).
#ifndef SENSORSYNC_MAX_VALUES
#define SENSORSYNC_MAX_VALUES 20
#endif

class SensorSync {
public:
    enum Protocol : uint8_t { NONE = 0, HTTP = 1, MQTT = 2 };

    SensorSync();

    /** Device token, generated in the dashboard "Devices" tab. / Token do dispositivo (aba "Dispositivos"). */
    void setToken(const char* token);

    /** Send via HTTP POST to http://host:port/api/ingest. / Envia por HTTP POST. */
    void useHTTP(const char* host, uint16_t port = 8080, const char* path = "/api/ingest");

    /** Publish to the MQTT broker (default topic "sensorsync"). / Publica no broker MQTT. */
    void useMQTT(const char* host, uint16_t port = 1883, const char* topic = "sensorsync");

    /** Broker user and password, if required. / Usuario e senha do broker, se exigidos. */
    void setMqttCredentials(const char* user, const char* password);

    /**
     * Encrypts the connection (HTTPS or MQTT over TLS) and only accepts a server signed by this CA
     * (PEM, included in the generated code). Call it after useHTTP()/useMQTT(); the string must stay
     * valid (use a global constant). Sketches that never call useTLS() do not link the TLS code.
     * Criptografa a conexao e so aceita um servidor assinado por esta CA (PEM, vem no codigo gerado).
     */
    void useTLS(const char* caPem);

    /** TLS enabled with useTLS(). / TLS ligado. */
    bool tlsEnabled() const { return _secure != nullptr; }

    /**
     * Keeps the HTTP connection open between sends (default true). With TLS this avoids a new
     * handshake on every send. / Mantem a conexao HTTP aberta entre envios (padrao true).
     */
    void setReuseConnection(bool reuse) { _reuse = reuse; }

    /**
     * Connects to Wi-Fi and waits up to setWifiTimeout() (default 15 s). Returns true if connected;
     * otherwise loop() keeps retrying in the background.
     * Conecta ao Wi-Fi e espera ate setWifiTimeout() (padrao 15 s); se nao conectar, loop() continua tentando.
     */
    bool begin(const char* ssid, const char* password);

    /** Use when your sketch manages Wi-Fi itself (static IP, WPA2-Enterprise...). / Quando o seu codigo ja cuida do Wi-Fi. */
    bool begin();

    /** Call on every loop(): keeps Wi-Fi and MQTT connected. / Chame em todo loop(). */
    void loop();

    /** Sets variable 'index' (0 = first project variable); NAN = no reading. / Define a variavel 'index'; NAN = sem leitura. */
    bool setValue(uint8_t index, float value);

    /** Discards values not sent yet. / Descarta os valores ainda nao enviados. */
    void clear();

    /** Sends the values set with setValue(); clears them on success. / Envia e limpa os valores em caso de sucesso. */
    bool send();

    /** Wi-Fi connected (and broker, in MQTT mode). / Wi-Fi conectado (e broker, no modo MQTT). */
    bool isConnected();

    /** Last error message, empty if the last send succeeded (messages in Portuguese). / Ultimo erro. */
    const char* lastError() const { return _error; }

    /** HTTP status of the last send (0 = no response or MQTT). / Status HTTP do ultimo envio. */
    int lastHttpStatus() const { return _httpStatus; }

    /** Decimal places sent (default 4). / Casas decimais enviadas (padrao 4). */
    void setDecimals(uint8_t decimals) { _decimals = decimals > 7 ? 7 : decimals; }

    /** Max time begin(ssid, password) waits for Wi-Fi, in ms (default 15000). / Espera maxima do Wi-Fi. */
    void setWifiTimeout(uint32_t ms) { _wifiTimeout = ms; }

    /** Diagnostic messages, e.g. sensorSync.setDebug(Serial). / Mensagens de diagnostico. */
    void setDebug(Stream& out) { _debug = &out; }

    /**
     * Experiments: every message also carries "seq" (1, 2, 3...) and, once the clock is synced by SNTP
     * (the server first, then pool.ntp.org), "ts" (send time, epoch ms), and send() is timed.
     * The server logs both (SENSORSYNC_METRICS=true) to measure latency and loss. See examples/Benchmark.
     * Experimentos: cada mensagem leva "seq" e "ts" e cada send() e cronometrado.
     */
    void enableBenchmark(bool on = true);

    /** Sequence number of the last send() (benchmark mode). / Numero de sequencia do ultimo send(). */
    uint32_t sequence() const { return _seq; }

    /** Duration of the last send() in microseconds. / Duracao do ultimo send() em microssegundos. */
    uint32_t lastSendMicros() const { return _lastSendUs; }

    /** Duration of the last broker connection (MQTT, includes the TLS handshake). / Duracao da ultima conexao ao broker. */
    uint32_t lastConnectMicros() const { return _lastConnectUs; }

    /** Clock synced (SNTP). / Relogio sincronizado. */
    static bool clockSynced();

    /** Current time in epoch ms (0 if the clock is not synced). / Hora atual em ms (0 sem sincronia). */
    static uint64_t epochMillis();

private:
    static const size_t HOST_LEN = 64;
    static const size_t TOKEN_LEN = 64;
    static const size_t PATH_LEN = 48;
    static const size_t JSON_LEN = 96 + TOKEN_LEN + SENSORSYNC_MAX_VALUES * 20 + 48;

    Protocol _protocol;
    char _host[HOST_LEN];
    uint16_t _port;
    char _path[PATH_LEN];          // caminho HTTP ou topico MQTT
    char _token[TOKEN_LEN];
    char _mqttUser[32];
    char _mqttPass[64];
    char _clientId[32];
    char _error[64];
    int _httpStatus;
    uint8_t _decimals;
    uint32_t _wifiTimeout;
    bool _manageWifi;
    char _ssid[33];
    char _wifiPass[65];
    unsigned long _lastWifiAttempt;
    unsigned long _lastMqttAttempt;

    float _values[SENSORSYNC_MAX_VALUES];
    bool _has[SENSORSYNC_MAX_VALUES];
    uint8_t _count;

    WiFiClient _mqttNet;
    WiFiClient _httpNet;
    PubSubClient _mqtt;
    HTTPClient _http;
    bool _reuse;
    Stream* _debug;

    // TLS: allocated only by useTLS(); the TLS-specific calls go through these pointers so that
    // sketches without TLS do not pull BearSSL/mbedTLS into the firmware.
    WiFiClient* _secure;
    void* _trust;
    bool _ntpStarted;
    bool _mflnChecked;
    bool (*_tlsPrepare)(SensorSync& self);
    void (*_tlsError)(SensorSync& self, char* out, size_t size);
    static bool tlsPrepareImpl(SensorSync& self);
    static void tlsErrorImpl(SensorSync& self, char* out, size_t size);
    void connectionError(const char* what);

    // Benchmark mode
    bool _bench;
    uint32_t _seq;
    uint32_t _lastSendUs;
    uint32_t _lastConnectUs;
    void (*_ntpStart)(SensorSync& self);
    static void ntpStartImpl(SensorSync& self);

    size_t buildJson(char* out, size_t size, bool includeToken);
    bool sendInternal();
    bool sendHttp(const char* json, size_t len);
    bool sendMqtt(const char* json, size_t len);
    bool connectMqtt();
    void makeClientId();
    void setError(const char* msg);
    void log(const char* msg);
    static void copy(char* dst, const char* src, size_t size);
};

#endif  // SENSORSYNC_H
