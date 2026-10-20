/*
 * SensorSync - cliente Arduino (ESP8266/ESP32) para a plataforma IoT SensorSync.
 * https://github.com/mwsprotte/sensorsync
 *
 * Uso basico:
 *   SensorSync sensorSync;
 *   sensorSync.setToken("ss_...");
 *   sensorSync.useHTTP("10.0.0.104", 8080);      // ou useMQTT("10.0.0.104", 1883)
 *   sensorSync.begin("rede", "senha");
 *   ...
 *   sensorSync.loop();                           // no loop(), sempre
 *   sensorSync.setValue(0, temperatura);
 *   sensorSync.setValue(1, umidade);
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
#error "A biblioteca SensorSync suporta apenas ESP8266 e ESP32."
#endif

#include <PubSubClient.h>

#define SENSORSYNC_VERSION "1.0.0"

// Quantidade maxima de variaveis por envio (a plataforma aceita ate 20 por projeto).
#ifndef SENSORSYNC_MAX_VALUES
#define SENSORSYNC_MAX_VALUES 20
#endif

class SensorSync {
public:
    enum Protocol : uint8_t { NONE = 0, HTTP = 1, MQTT = 2 };

    SensorSync();

    /** Token do dispositivo, gerado na tela "Dispositivos" do dashboard. */
    void setToken(const char* token);

    /** Envia por HTTP POST para http://host:port/api/ingest. */
    void useHTTP(const char* host, uint16_t port = 8080, const char* path = "/api/ingest");

    /** Publica no broker MQTT (topico padrao "sensorsync"). */
    void useMQTT(const char* host, uint16_t port = 1883, const char* topic = "sensorsync");

    /** Usuario e senha do broker, se o broker exigir autenticacao. */
    void setMqttCredentials(const char* user, const char* password);

    /**
     * Conecta ao Wi-Fi e espera ate o tempo de setWifiTimeout() (padrao 15 s).
     * Retorna true se conectou. Se nao conectar, loop() continua tentando em segundo plano.
     */
    bool begin(const char* ssid, const char* password);

    /** Use quando o seu codigo ja cuida do Wi-Fi (ex.: rede corporativa WPA2-Enterprise). */
    bool begin();

    /** Chame em todo loop(): mantem Wi-Fi e MQTT conectados sem bloquear. */
    void loop();

    /** Define o valor da variavel 'index' (0 = primeira variavel do projeto). NaN = sem leitura. */
    bool setValue(uint8_t index, float value);

    /** Descarta os valores definidos e ainda nao enviados. */
    void clear();

    /** Envia os valores definidos com setValue(). Em caso de sucesso, os valores sao limpos. */
    bool send();

    /** Wi-Fi conectado e, no modo MQTT, broker conectado. */
    bool isConnected();

    /** Descricao do ultimo erro (texto vazio se o ultimo envio deu certo). */
    const char* lastError() const { return _error; }

    /** Status HTTP do ultimo envio (0 se nao houve resposta ou se o modo for MQTT). */
    int lastHttpStatus() const { return _httpStatus; }

    /** Casas decimais enviadas (padrao 4). */
    void setDecimals(uint8_t decimals) { _decimals = decimals > 7 ? 7 : decimals; }

    /** Tempo maximo que begin(ssid, senha) espera pelo Wi-Fi, em ms (padrao 15000). */
    void setWifiTimeout(uint32_t ms) { _wifiTimeout = ms; }

    /** Mensagens de diagnostico (ex.: sensorSync.setDebug(Serial)). */
    void setDebug(Stream& out) { _debug = &out; }

private:
    static const size_t HOST_LEN = 64;
    static const size_t TOKEN_LEN = 64;
    static const size_t PATH_LEN = 48;
    static const size_t JSON_LEN = 96 + TOKEN_LEN + SENSORSYNC_MAX_VALUES * 20;

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
    PubSubClient _mqtt;
    Stream* _debug;

    size_t buildJson(char* out, size_t size, bool includeToken);
    bool sendHttp(const char* json, size_t len);
    bool sendMqtt(const char* json, size_t len);
    bool connectMqtt();
    void makeClientId();
    void setError(const char* msg);
    void log(const char* msg);
    static void copy(char* dst, const char* src, size_t size);
};

#endif  // SENSORSYNC_H
