#include "SensorSync.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

SensorSync::SensorSync()
    : _protocol(NONE), _port(0), _httpStatus(0), _decimals(4), _wifiTimeout(15000), _manageWifi(false),
      _lastWifiAttempt(0), _lastMqttAttempt(0), _count(0), _mqtt(_mqttNet), _debug(nullptr) {
    _host[0] = _path[0] = _token[0] = _mqttUser[0] = _mqttPass[0] = _clientId[0] = _error[0] = 0;
    _ssid[0] = _wifiPass[0] = 0;
    clear();
}

void SensorSync::copy(char* dst, const char* src, size_t size) {
    if (src == nullptr) {
        dst[0] = 0;
        return;
    }
    strncpy(dst, src, size - 1);
    dst[size - 1] = 0;
}

void SensorSync::setToken(const char* token) { copy(_token, token, sizeof(_token)); }

void SensorSync::useHTTP(const char* host, uint16_t port, const char* path) {
    _protocol = HTTP;
    copy(_host, host, sizeof(_host));
    _port = port;
    copy(_path, path, sizeof(_path));
}

void SensorSync::useMQTT(const char* host, uint16_t port, const char* topic) {
    _protocol = MQTT;
    copy(_host, host, sizeof(_host));
    _port = port;
    copy(_path, topic, sizeof(_path));
    _mqtt.setServer(_host, _port);
    _mqtt.setBufferSize(JSON_LEN + sizeof(_path) + 16);
    _mqtt.setSocketTimeout(3);
    _mqtt.setKeepAlive(30);
}

void SensorSync::setMqttCredentials(const char* user, const char* password) {
    copy(_mqttUser, user, sizeof(_mqttUser));
    copy(_mqttPass, password, sizeof(_mqttPass));
}

bool SensorSync::begin(const char* ssid, const char* password) {
    copy(_ssid, ssid, sizeof(_ssid));
    copy(_wifiPass, password, sizeof(_wifiPass));
    _manageWifi = true;
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(_ssid, _wifiPass);
    _lastWifiAttempt = millis();
    makeClientId();

    if (_debug) {
        _debug->print(F("[SensorSync] Conectando ao Wi-Fi "));
        _debug->println(_ssid);
    }
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < _wifiTimeout) {
        delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
        if (_debug) {
            _debug->print(F("[SensorSync] Wi-Fi conectado, IP "));
            _debug->println(WiFi.localIP());
        }
        if (_protocol == MQTT) {
            connectMqtt();
        }
        return true;
    }
    log("Wi-Fi ainda nao conectou; tentando em segundo plano");
    return false;
}

bool SensorSync::begin() {
    _manageWifi = false;
    makeClientId();
    if (_protocol == MQTT && WiFi.status() == WL_CONNECTED) {
        connectMqtt();
    }
    return WiFi.status() == WL_CONNECTED;
}

void SensorSync::makeClientId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(_clientId, sizeof(_clientId), "sensorsync-%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void SensorSync::loop() {
    if (WiFi.status() != WL_CONNECTED) {
        // O proprio ESP reconecta (setAutoReconnect); reforca a cada 15 s caso tenha desistido
        if (_manageWifi && millis() - _lastWifiAttempt > 15000UL) {
            _lastWifiAttempt = millis();
            log("Wi-Fi desconectado; tentando reconectar");
            WiFi.disconnect();
            WiFi.begin(_ssid, _wifiPass);
        }
        return;
    }
    if (_protocol == MQTT) {
        if (!_mqtt.connected()) {
            if (millis() - _lastMqttAttempt > 5000UL) {
                connectMqtt();
            }
        } else {
            _mqtt.loop();
        }
    }
}

bool SensorSync::connectMqtt() {
    _lastMqttAttempt = millis();
    if (_clientId[0] == 0) {
        makeClientId();
    }
    bool ok = _mqttUser[0] ? _mqtt.connect(_clientId, _mqttUser, _mqttPass) : _mqtt.connect(_clientId);
    if (ok) {
        log("Broker MQTT conectado");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "Falha ao conectar no broker MQTT (estado %d)", _mqtt.state());
        setError(msg);
        log(msg);
    }
    return ok;
}

bool SensorSync::isConnected() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    return _protocol != MQTT || _mqtt.connected();
}

bool SensorSync::setValue(uint8_t index, float value) {
    if (index >= SENSORSYNC_MAX_VALUES) {
        setError("Indice acima de SENSORSYNC_MAX_VALUES");
        return false;
    }
    _values[index] = value;
    _has[index] = true;
    if (index + 1 > _count) {
        _count = index + 1;
    }
    return true;
}

void SensorSync::clear() {
    for (uint8_t i = 0; i < SENSORSYNC_MAX_VALUES; i++) {
        _has[i] = false;
        _values[i] = NAN;
    }
    _count = 0;
}

size_t SensorSync::buildJson(char* out, size_t size, bool includeToken) {
    size_t n = 0;
    int w = includeToken ? snprintf(out, size, "{\"token\":\"%s\",\"values\":[", _token)
                         : snprintf(out, size, "{\"values\":[");
    if (w < 0 || (size_t)w >= size) return 0;
    n = (size_t)w;

    for (uint8_t i = 0; i < _count; i++) {
        char num[24];
        float v = _values[i];
        if (!_has[i] || isnan(v) || isinf(v)) {
            strcpy(num, "null");
        } else {
            dtostrf(v, 1, _decimals, num);
            // dtostrf pode deixar espacos a esquerda
            char* p = num;
            while (*p == ' ') p++;
            if (p != num) memmove(num, p, strlen(p) + 1);
        }
        w = snprintf(out + n, size - n, "%s%s", i ? "," : "", num);
        if (w < 0 || (size_t)w >= size - n) return 0;
        n += (size_t)w;
    }
    w = snprintf(out + n, size - n, "]}");
    if (w < 0 || (size_t)w >= size - n) return 0;
    return n + (size_t)w;
}

bool SensorSync::send() {
    _httpStatus = 0;
    if (_protocol == NONE) {
        setError("Chame useHTTP() ou useMQTT() antes de enviar");
        return false;
    }
    if (_token[0] == 0) {
        setError("Token nao definido (setToken)");
        return false;
    }
    if (_count == 0) {
        setError("Nenhum valor definido (setValue)");
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        setError("Wi-Fi desconectado");
        return false;
    }

    char json[JSON_LEN];
    size_t len = buildJson(json, sizeof(json), _protocol == MQTT);
    if (len == 0) {
        setError("Payload maior que o buffer");
        return false;
    }
    if (_debug) {
        // No MQTT o payload contem o token; mostra so a quantidade de valores
        _debug->print(F("[SensorSync] Enviando "));
        _debug->print(_count);
        _debug->println(_protocol == MQTT ? F(" valor(es) via MQTT") : F(" valor(es) via HTTP"));
    }

    bool ok = _protocol == HTTP ? sendHttp(json, len) : sendMqtt(json, len);
    if (ok) {
        _error[0] = 0;
        clear();
    }
    return ok;
}

bool SensorSync::sendHttp(const char* json, size_t len) {
    char url[HOST_LEN + PATH_LEN + 24];
    snprintf(url, sizeof(url), "http://%s:%u%s", _host, (unsigned)_port, _path);
    char auth[TOKEN_LEN + 8];
    snprintf(auth, sizeof(auth), "Bearer %s", _token);

    WiFiClient net;
    HTTPClient http;
    http.setTimeout(5000);
    if (!http.begin(net, url)) {
        setError("URL invalida");
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", auth);
    _httpStatus = http.POST((uint8_t*)json, len);
    http.end();

    if (_httpStatus == 200) {
        return true;
    }
    if (_httpStatus == 401) {
        setError("Token recusado pelo servidor (401)");
    } else if (_httpStatus < 0) {
        setError("Servidor inacessivel");
    } else {
        char msg[48];
        snprintf(msg, sizeof(msg), "Servidor respondeu HTTP %d", _httpStatus);
        setError(msg);
    }
    return false;
}

bool SensorSync::sendMqtt(const char* json, size_t len) {
    if (!_mqtt.connected() && !connectMqtt()) {
        return false;
    }
    if (!_mqtt.publish(_path, (const uint8_t*)json, (unsigned int)len, false)) {
        setError("Falha ao publicar no broker MQTT");
        return false;
    }
    _mqtt.loop();
    return true;
}

void SensorSync::setError(const char* msg) { copy(_error, msg, sizeof(_error)); }

void SensorSync::log(const char* msg) {
    if (_debug) {
        _debug->print(F("[SensorSync] "));
        _debug->println(msg);
    }
}
