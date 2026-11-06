#include "SensorSync.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(ESP8266)
typedef BearSSL::WiFiClientSecure SecureClient;
#else
typedef WiFiClientSecure SecureClient;
#endif

#if defined(ESP8266)
namespace {

// ESP8266 (BearSSL) checks certificate dates; the ESP32 core is built without that check
// (CONFIG_MBEDTLS_HAVE_TIME_DATE off), so this is only needed here.
// Moment the library was compiled (UTC-ish; the build machine's local time). Boards without a valid
// clock use it as "now" to check certificate dates. Because the server certificate is valid from the
// creation of the CA (which happens before any code is generated) for 20 years, a firmware built
// afterwards always falls inside the validity window. The 2-day margin absorbs the time zone.
time_t buildEpoch() {
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* d = __DATE__;  // "Nov  6 2026"
    const char* t = __TIME__;  // "12:00:00"
    char mon[4] = {d[0], d[1], d[2], 0};
    const char* found = strstr(months, mon);
    long m = found ? (found - months) / 3 + 1 : 1;
    long day = atol(d + 4);
    long y = atol(d + 7);
    // days_from_civil (H. Hinnant)
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = era * 146097 + doe - 719468;
    return (time_t)days * 86400 + atol(t) * 3600 + atol(t + 3) * 60 + atol(t + 6) + 2 * 86400;
}

}  // namespace
#endif

SensorSync::SensorSync()
    : _protocol(NONE), _port(0), _httpStatus(0), _decimals(4), _wifiTimeout(15000), _manageWifi(false),
      _lastWifiAttempt(0), _lastMqttAttempt(0), _count(0), _mqtt(_mqttNet), _reuse(true), _debug(nullptr),
      _secure(nullptr), _trust(nullptr), _ntpStarted(false), _mflnChecked(false), _tlsPrepare(nullptr),
      _tlsError(nullptr), _bench(false), _seq(0), _lastSendUs(0), _lastConnectUs(0), _ntpStart(nullptr) {
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

void SensorSync::useTLS(const char* caPem) {
    if (_secure != nullptr || caPem == nullptr) {
        return;
    }
    SecureClient* c = new SecureClient();
#if defined(ESP8266)
    BearSSL::X509List* trust = new BearSSL::X509List(caPem);
    c->setTrustAnchors(trust);
    _trust = trust;
#else
    c->setCACert(caPem);
    c->setHandshakeTimeout(15);
#endif
    _secure = c;
    _tlsPrepare = &SensorSync::tlsPrepareImpl;
    _tlsError = &SensorSync::tlsErrorImpl;
    log("TLS ligado");
}

void SensorSync::ntpStartImpl(SensorSync& self) {
    if (!self._ntpStarted && self._host[0]) {
        // Tenta o proprio servidor SensorSync (chrony) e depois a internet
        configTime(0, 0, self._host, "pool.ntp.org");
        self._ntpStarted = true;
        self.log("SNTP iniciado");
    }
}

void SensorSync::enableBenchmark(bool on) {
    _bench = on;
    if (on) {
        _ntpStart = &SensorSync::ntpStartImpl;
        if (WiFi.status() == WL_CONNECTED) {
            _ntpStart(*this);
        }
    }
}

bool SensorSync::clockSynced() { return time(nullptr) > 1600000000; }

uint64_t SensorSync::epochMillis() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    if (tv.tv_sec < 1600000000) {
        return 0;
    }
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
}

bool SensorSync::tlsPrepareImpl(SensorSync& self) {
    SecureClient* c = static_cast<SecureClient*>(self._secure);
    // Hora certa e opcional para o TLS (ver buildEpoch)
    ntpStartImpl(self);
#if defined(ESP8266)
    // O BearSSL confere as datas do certificado contra esta hora
    time_t now = time(nullptr);
    time_t minTime = buildEpoch();
    c->setX509Time(now > minTime ? now : minTime);
    if (!self._mflnChecked) {
        // Buffers de 512 B economizam ~20 KB de RAM, mas so funcionam se o servidor aceitar MFLN
        self._mflnChecked = true;
        if (SecureClient::probeMaxFragmentLength(self._host, self._port, 512)) {
            c->setBufferSizes(512, 512);
            self.log("TLS: servidor aceita fragmentos de 512 B (buffers reduzidos)");
        }
    }
#else
    (void)c;
#endif
    return true;
}

void SensorSync::tlsErrorImpl(SensorSync& self, char* out, size_t size) {
    SecureClient* c = static_cast<SecureClient*>(self._secure);
    out[0] = 0;
#if defined(ESP8266)
    c->getLastSSLError(out, size);
#else
    // -1 ("Generic error") = a conexao TCP nem abriu: nao e problema de TLS
    if (c->lastError(out, size) == -1) {
        out[0] = 0;
    }
#endif
}

void SensorSync::connectionError(const char* what) {
    char msg[sizeof(_error)];
    char detail[48] = {0};
    if (_tlsError) {
        _tlsError(*this, detail, sizeof(detail));
    }
    if (detail[0]) {
        snprintf(msg, sizeof(msg), "%s: TLS %s", what, detail);
    } else {
        snprintf(msg, sizeof(msg), "%s", what);
    }
    setError(msg);
    log(msg);
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
    if (_ntpStart) {
        _ntpStart(*this);
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
    if (_secure) {
        _mqtt.setClient(*_secure);
        _mqtt.setSocketTimeout(10);  // o handshake TLS pode levar alguns segundos no ESP8266
        _tlsPrepare(*this);
    }
    uint32_t t0 = micros();
    bool ok = _mqttUser[0] ? _mqtt.connect(_clientId, _mqttUser, _mqttPass) : _mqtt.connect(_clientId);
    _lastConnectUs = micros() - t0;
    if (ok) {
        log("Broker MQTT conectado");
    } else {
        char msg[48];
        if (_mqtt.state() == MQTT_CONNECT_FAILED) {
            snprintf(msg, sizeof(msg), "Broker inacessivel (desligado ou firewall?)");
        } else {
            snprintf(msg, sizeof(msg), "Falha no broker MQTT (estado %d)", _mqtt.state());
        }
        connectionError(msg);
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
    w = snprintf(out + n, size - n, "]");
    if (w < 0 || (size_t)w >= size - n) return 0;
    n += (size_t)w;
    if (_bench) {
        // "seq" e "ts" (epoch ms, so com relogio sincronizado); uint64 escrito a mao (printf %llu nao e garantido)
        char ts[24] = "";
        uint64_t ms = epochMillis();
        if (ms) {
            char tmp[21];
            int i = 0;
            while (ms && i < 20) {
                tmp[i++] = (char)('0' + (ms % 10));
                ms /= 10;
            }
            int j = 0;
            ts[j++] = ',';
            const char* key = "\"ts\":";
            while (*key) ts[j++] = *key++;
            while (i) ts[j++] = tmp[--i];
            ts[j] = 0;
        }
        w = snprintf(out + n, size - n, ",\"seq\":%lu%s", (unsigned long)_seq, ts);
        if (w < 0 || (size_t)w >= size - n) return 0;
        n += (size_t)w;
    }
    w = snprintf(out + n, size - n, "}");
    if (w < 0 || (size_t)w >= size - n) return 0;
    return n + (size_t)w;
}

bool SensorSync::send() {
    uint32_t t0 = micros();
    if (_bench) {
        _seq++;  // conta toda tentativa: um seq que nao chega ao servidor e uma perda
    }
    bool ok = sendInternal();
    _lastSendUs = micros() - t0;
    return ok;
}

bool SensorSync::sendInternal() {
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
    snprintf(url, sizeof(url), "%s://%s:%u%s", _secure ? "https" : "http", _host, (unsigned)_port, _path);
    char auth[TOKEN_LEN + 8];
    snprintf(auth, sizeof(auth), "Bearer %s", _token);

    WiFiClient& net = _secure ? *_secure : _httpNet;
    if (_secure) {
        _tlsPrepare(*this);
    }
    // Com reuso, a conexao pode ter sido fechada pelo servidor desde o ultimo envio: tenta mais uma vez
    for (uint8_t attempt = 0; attempt < 2; attempt++) {
        _http.setReuse(_reuse);
        _http.setTimeout(5000);
        if (!_http.begin(net, url)) {
            setError("URL invalida");
            return false;
        }
        _http.addHeader("Content-Type", "application/json");
        _http.addHeader("Authorization", auth);
        _httpStatus = _http.POST((uint8_t*)json, len);
        _http.end();
        if (_httpStatus > 0 || !_reuse) {
            break;
        }
        net.stop();
    }
    if (!_reuse) {
        net.stop();
    }

    if (_httpStatus == 200) {
        return true;
    }
    if (_httpStatus == 401) {
        setError("Token recusado pelo servidor (401)");
    } else if (_httpStatus == 301 || _httpStatus == 308) {
        setError("Servidor pede HTTPS: gere o codigo de novo");
    } else if (_httpStatus < 0) {
        connectionError("Servidor inacessivel (desligado ou firewall?)");
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
