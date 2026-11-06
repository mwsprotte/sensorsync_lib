# SensorSync — Arduino library

[English](#english) · [Português](#português)

Arduino client for ESP8266 and ESP32 boards of the self-hosted IoT platform
[SensorSync](https://github.com/mwsprotte/sensorsync).
Author: [Matheus Willian Sprotte](https://github.com/mwsprotte).

---

## English

Sends sensor readings over **HTTP** or **MQTT**, authenticating each board with a **device token**.

### Installation

- **Arduino IDE:** *Tools › Manage Libraries*, search for **SensorSync** and install it. The
  [PubSubClient](https://github.com/knolleary/pubsubclient) dependency is installed automatically.
- **PlatformIO:** `lib_deps = https://github.com/mwsprotte/sensorsync_lib.git` (a `library.json` manifest is included).
- **Manual:** download this repository as .zip and use *Sketch › Include Library › Add .ZIP Library*
  (also install PubSubClient).

Build-tested with ESP8266 core 3.1.x and ESP32 core 3.0.x.

### Usage

```cpp
#include <SensorSync.h>

SensorSync sensorSync;
unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  sensorSync.setToken("ss_...");              // dashboard › Devices tab
  sensorSync.useHTTP("192.168.0.10", 8080);  // or useMQTT("192.168.0.10", 1883)
  sensorSync.begin("WIFI_SSID", "WIFI_PASSWORD");
}

void loop() {
  sensorSync.loop();                         // always: keeps Wi-Fi/MQTT alive without blocking
  if (millis() - lastSend >= 1000) {
    lastSend = millis();
    sensorSync.setValue(0, readTemperature()); // index = variable order in the project
    sensorSync.setValue(1, readHumidity());
    if (!sensorSync.send()) Serial.println(sensorSync.lastError());
  }
}
```

The platform generates this sketch already filled in (server, token and variables) when a project is created.

### API

| Method | Description |
|---|---|
| `setToken(token)` | Device token (`ss_...`). |
| `useHTTP(host, port = 8080, path = "/api/ingest")` | Send via HTTP POST. |
| `useMQTT(host, port = 1883, topic = "sensorsync")` | Send via MQTT. |
| `setMqttCredentials(user, password)` | If the broker requires login. |
| `useTLS(caPem)` | Encrypts the connection and only accepts a server signed by this CA (see TLS below). |
| `setReuseConnection(bool)` | Keeps the HTTP(S) connection open between sends (default `true`). |
| `begin(ssid, password)` | Connects Wi-Fi (waits up to 15 s, then keeps retrying in `loop()`). |
| `begin()` | When your sketch already connected Wi-Fi (static IP, enterprise network…). |
| `loop()` | Call on every `loop()`. |
| `setValue(index, value)` | Sets variable `index` (0 = first). `NAN` = no reading. |
| `send()` | Sends and clears the values. Returns `false` on error. |
| `lastError()` / `lastHttpStatus()` | Diagnostics of the last send (error messages are in Portuguese). |
| `isConnected()` | Wi-Fi connected (and broker, in MQTT mode). |
| `setDecimals(n)` | Decimal places sent (default 4). |
| `setWifiTimeout(ms)` | Max wait of `begin(ssid, password)`. |
| `setDebug(Serial)` | Diagnostic messages on a serial port. |

Up to 20 variables per send (`#define SENSORSYNC_MAX_VALUES` before the `#include` to change it).

### Protocol

- **HTTP:** `POST /api/ingest`, header `Authorization: Bearer <token>`, body `{"values":[21.5,60.2,null]}`.
- **MQTT:** topic `sensorsync`, message `{"token":"<token>","values":[21.5,60.2,null]}`.

The array position is the variable index; `null` means "no reading". The server identifies project and
device from the token, so a board cannot write to another project. The MQTT client ID is
`sensorsync-<MAC>`, unique per board.

### TLS

When the server runs with the `tls` profile, the generated code already includes the server CA and calls
`useTLS(CA_CERT)`; HTTPS uses port 8443 and MQTT over TLS port 8883 (example `TLS_Basico`).

- The board validates the server certificate against that CA (no `setInsecure()`), so a third machine on
  the network can neither read the token nor impersonate the server.
- The server address in the sketch must be one of the names/IPs in the server certificate (the server
  adds its own IPs and `SENSORSYNC_PUBLIC_HOST` automatically).
- Boards have no reliable clock: certificate dates are checked against the SNTP time when available,
  otherwise against the firmware build date.
- ESP8266: if the broker accepts MFLN, TLS buffers drop to 512 B (about 20 KB less RAM).
- Sketches that never call `useTLS()` do not link the TLS code (no flash cost).

### Migrating from 0.x (`sensor_sync/` folder)

| 0.x | 1.x |
|---|---|
| `#include "sensor_sync/sensor_sync.hpp"` | `#include <SensorSync.h>` |
| `setup_wifi(ssid, password)` | `sensorSync.begin(ssid, password)` |
| `HTTPConnection` / `MQTTConnection` | one `SensorSync` class with `useHTTP()` / `useMQTT()` |
| `sendData(project, device, n, desc, data, url)` | `setValue(i, v)` + `send()`; project and device come from the token |

To keep already deployed boards running during the switch, the server can temporarily accept the old
format with `SENSORSYNC_ALLOW_LEGACY_INGEST=true`.

---

## Português

Envia leituras de sensores por **HTTP** ou **MQTT**, autenticando cada placa com um **token de dispositivo**.

### Instalação

- **IDE Arduino:** *Ferramentas › Gerenciar Bibliotecas*, busque **SensorSync** e instale. A dependência
  [PubSubClient](https://github.com/knolleary/pubsubclient) é instalada junto.
- **PlatformIO:** `lib_deps = https://github.com/mwsprotte/sensorsync_lib.git` (o manifesto `library.json` já está incluído).
- **Manual:** baixe este repositório como .zip e use *Sketch › Incluir Biblioteca › Adicionar biblioteca .ZIP*
  (instale também a PubSubClient).

Compilação testada com o núcleo ESP8266 3.1.x e o núcleo ESP32 3.0.x.

### Uso

```cpp
#include <SensorSync.h>

SensorSync sensorSync;
unsigned long ultimoEnvio = 0;

void setup() {
  Serial.begin(115200);
  sensorSync.setToken("ss_...");              // dashboard › aba Dispositivos
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
| `useTLS(caPem)` | Criptografa a conexão e só aceita um servidor assinado por esta CA (ver TLS abaixo). |
| `setReuseConnection(bool)` | Mantém a conexão HTTP(S) aberta entre envios (padrão `true`). |
| `setMqttCredentials(usuario, senha)` | Se o broker exigir login. |
| `begin(ssid, senha)` | Conecta o Wi-Fi (espera até 15 s; depois continua tentando em `loop()`). |
| `begin()` | Quando o seu código já conectou o Wi-Fi (IP fixo, rede corporativa…). |
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
dispositivo pelo token, então uma placa não consegue gravar em outro projeto. O client ID MQTT é
`sensorsync-<MAC>`, único por placa.

### TLS

Com o servidor no perfil `tls`, o código gerado já vem com a CA do servidor e chama `useTLS(CA_CERT)`;
HTTPS usa a porta 8443 e MQTT com TLS a 8883 (exemplo `TLS_Basico`).

- A placa confere o certificado do servidor contra essa CA (sem `setInsecure()`): outra máquina na rede
  não consegue ler o token nem se passar pelo servidor.
- O endereço do servidor no sketch precisa ser um dos nomes/IPs do certificado (o servidor inclui os
  próprios IPs e o `SENSORSYNC_PUBLIC_HOST` automaticamente).
- As placas não têm relógio confiável: as datas do certificado são conferidas com a hora do SNTP quando
  disponível; senão, com a data de compilação do firmware.
- ESP8266: se o broker aceitar MFLN, os buffers TLS caem para 512 B (cerca de 20 KB a menos de RAM).
- Sketches que não chamam `useTLS()` não incluem o código de TLS (sem custo de flash).

### Migrando da versão 0.x (pasta `sensor_sync/`)

| 0.x | 1.x |
|---|---|
| `#include "sensor_sync/sensor_sync.hpp"` | `#include <SensorSync.h>` |
| `setup_wifi(ssid, senha)` | `sensorSync.begin(ssid, senha)` |
| `HTTPConnection` / `MQTTConnection` | uma classe `SensorSync` com `useHTTP()` / `useMQTT()` |
| `sendData(projeto, dispositivo, n, desc, dados, url)` | `setValue(i, v)` + `send()`; projeto e dispositivo vêm do token |

Para não parar placas já instaladas durante a troca, o servidor pode aceitar temporariamente o formato
antigo com `SENSORSYNC_ALLOW_LEGACY_INGEST=true`.
