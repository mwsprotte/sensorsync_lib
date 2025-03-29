#ifndef SENSOR_SYNC_H
#define SENSOR_SYNC_H

/**
 * @class HTTPConnection
 * @brief Classe para enviar dados via HTTP.
 *
 * Esta classe permite o envio de dados de sensores para a API SensorSync
 * utilizando uma requisição POST com dados no formato JSON.
 */
class HTTPConnection
{
private:
  WiFiClient espClient;
  HTTPClient httpClient;

public:
  /**
   * @brief Envia os dados dos sensores para o servidor HTTP.
   *
   * Este método monta uma string JSON contendo as informações do projeto,
   * do dispositivo, do índice dos dados, a descrição do sensor e os valores dos dados.
   * Em seguida, envia a requisição POST para o servidor HTTP.
   *
   * @param project Identificador do projeto.
   * @param device Identificador do dispositivo (sensor).
   * @param dataNumber Número de dados a serem enviados.
   * @param desc Array de descrições dos sensores.
   * @param data Array de dados dos sensores.
   * @param URL URL do servidor para onde os dados serão enviados.
   */
  void sendData(int project, int device, int dataNumber, String desc[], float data[], const char *URL);
};

/**
 * @class MQTTConnection
 * @brief Classe para comunicação via MQTT.
 *
 * Esta classe facilita a conexão com um broker MQTT e o envio de dados de sensores
 * para tópicos específicos. Ela utiliza a biblioteca PubSubClient para a comunicação.
 */
class MQTTConnection
{
private:
  WiFiClient espClient;
  PubSubClient client;

public:
  /**
   * @brief Construtor da classe MQTTConnection.
   *
   * Inicializa o cliente MQTT com um buffer de 64KB.
   */
  MQTTConnection();

  /**
   * @brief Reconnecta ao broker MQTT em caso de perda de conexão.
   *
   * Este método tenta reconectar ao broker MQTT até que a conexão seja restabelecida.
   */
  void reconnect();

  /**
   * @brief Envia os dados dos sensores para um tópico MQTT.
   *
   * Este método monta uma string JSON com os dados dos sensores e publica essa string
   * em um tópico MQTT especificado.
   *
   * @param project Identificador do projeto.
   * @param device Identificador do dispositivo (sensor).
   * @param desc Array de descrições dos sensores.
   * @param data Array de dados dos sensores.
   * @param topicAPI Tópico MQTT para onde os dados serão publicados.
   */
  void sendData(int project, int device, String desc[], float data[], const char *topicAPI);

  /**
   * @brief Configura o servidor MQTT.
   *
   * Este método configura o servidor MQTT e a porta do broker.
   *
   * @param server Endereço do servidor MQTT.
   * @param port Porta do broker MQTT.
   */
  void setServer(const char *server, uint16_t port);

  /**
   * @brief Acessa o cliente MQTT (opcional).
   *
   * Este método retorna o cliente MQTT para que o usuário possa acessar
   * diretamente as funcionalidades do cliente.
   *
   * @return O cliente PubSubClient.
   */
  PubSubClient &getClient(); // Método para acessar o client (opcional)
};

// Função para configurar a conexão Wi-Fi
void setup_wifi(const char *wifi_ssid, const char *wifi_password);

#endif // SENSOR_SYNC_H
