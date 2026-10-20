# SensorSync — biblioteca Arduino

Cliente para ESP8266 e ESP32 da plataforma IoT self-hosted [SensorSync](https://github.com/mwsprotte/sensorsync).
Envia leituras de sensores por **HTTP** ou **MQTT**, autenticando cada placa com um **token de dispositivo**.

## Instalação

- **IDE Arduino:** *Ferramentas › Gerenciar Bibliotecas*, busque **SensorSync** e instale. A dependência
  [PubSubClient](https://github.com/knolleary/pubsubclient) é instalada junto.
- **Manual:** baixe este repositório como .zip e use *Sketch › Incluir Biblioteca › Adicionar biblioteca .ZIP*
  (instale também a PubSubClient).

Placas testadas na compilação: núcleo ESP8266 3.1.x e núcleo ESP32 3.0.x.

## Uso

```cpp
#include <SensorSync.h>

SensorSync sensorSync;
unsigned long ultimoEnvio = 0;

void setup() {
  Serial.begin(115200);
  sensorSync.setToken("ss_...");              // tela Dispositivos do dashboard
  sensorSync.useHTTP("192.168.0.10", 8080);  // ou useMQTT("192.168.0.10", 1883)
  sensorSync.begin("NOME_DA_REDE", "SENHA_DA_REDE");
}

void loop() {
  sensorSync.loop();                         // sempre: mantém Wi-Fi/MQTT sem bloquear
  if (millis() - ultimoEnvio >= 1000) {
    ultimoEnvio = millis();
    sensorSync.setValue(0, lerTemperatura()); // índice = ordem da variável no projeto
    sensorSync.setValue(1, lerUmidade());
    if (!sensorSync.send()) Serial.println(sensorSync.lastError());
  }
}
```

A plataforma gera esse código já preenchido (servidor, token e variáveis) ao criar um projeto.

### API

| Método | Descrição |
|---|---|
| `setToken(token)` | Token do dispositivo (`ss_...`). |
| `useHTTP(host, porta = 8080, caminho = "/api/ingest")` | Envio por HTTP POST. |
| `useMQTT(host, porta = 1883, tópico = "sensorsync")` | Envio por MQTT. |
| `setMqttCredentials(usuario, senha)` | Se o broker exigir login. |
| `begin(ssid, senha)` | Conecta o Wi-Fi (espera até 15 s; depois continua tentando em `loop()`). |
| `begin()` | Use quando o seu código já conectou o Wi-Fi (IP fixo, rede corporativa…). |
| `loop()` | Chame em todo `loop()`. |
| `setValue(indice, valor)` | Define a variável `indice` (0 = primeira). `NAN` = sem leitura. |
| `send()` | Envia e limpa os valores. Retorna `false` em caso de erro. |
| `lastError()` / `lastHttpStatus()` | Diagnóstico do último envio. |
| `isConnected()` | Wi-Fi conectado (e broker, no modo MQTT). |
| `setDecimals(n)` | Casas decimais enviadas (padrão 4). |
| `setWifiTimeout(ms)` | Espera máxima de `begin(ssid, senha)`. |
| `setDebug(Serial)` | Mensagens de diagnóstico na serial. |

Até 20 variáveis por envio (`#define SENSORSYNC_MAX_VALUES` antes do `#include` para mudar).

### Protocolo

- **HTTP:** `POST /api/ingest`, cabeçalho `Authorization: Bearer <token>`, corpo `{"values":[21.5,60.2,null]}`.
- **MQTT:** tópico `sensorsync`, mensagem `{"token":"<token>","values":[21.5,60.2,null]}`.

A posição no vetor é o índice da variável; `null` significa "sem leitura". O servidor descobre projeto e
dispositivo pelo token, então uma placa não consegue gravar em outro projeto.

O client ID MQTT é `sensorsync-<MAC>`, único por placa.

## Migrando da versão 0.x (pasta `sensor_sync/`)

| 0.x | 1.x |
|---|---|
| `#include "sensor_sync/sensor_sync.hpp"` | `#include <SensorSync.h>` |
| `setup_wifi(ssid, senha)` | `sensorSync.begin(ssid, senha)` |
| `HTTPConnection` / `MQTTConnection` | uma classe `SensorSync` com `useHTTP()` / `useMQTT()` |
| `sendData(projeto, dispositivo, n, desc, dados, url)` | `setValue(i, v)` + `send()`; projeto e dispositivo vêm do token |

Para não parar placas já instaladas durante a troca, o servidor pode aceitar temporariamente o formato
antigo com `SENSORSYNC_ALLOW_LEGACY_INGEST=true`.
