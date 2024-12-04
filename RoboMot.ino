#define TINY_GSM_MODEM_SIM808  // Use this line to select the SIM808 modem
#include <TinyGsmClient.h>
#include <ArduinoJson.h>       //versão: 5.13.4 - by Benoit Blanchon
#include <ArduinoHttpClient.h>
#include "DHT.h"

#define SerialMon Serial

// Defina os pinos RX e TX para a comunicação com o SIM808
#define SIM808_RX_PIN 17
#define SIM808_TX_PIN 16

// Defina as configurações da rede móvel (APN)
const char apn[]      = "timbrasil.br";
const char gprsUser[] = "tim";
const char gprsPass[] = "tim";

// Inicialize a comunicação serial com o SIM808
HardwareSerial SerialAT(1);  // Porta serial 1 para ESP32

const char FIREBASE_HOST[]  = "robomot-bdd06-default-rtdb.firebaseio.com";
const String FIREBASE_AUTH  = "dOk4sp1Oigp7kUGJ53bjYhPn42agOogewEhk8MEZ";
const String FIREBASE_PATH  = "/";
const int SSL_PORT          = 443;

TinyGsm modem(SerialAT);
TinyGsmClientSecure gsm_client_secure_modem(modem, 0);
HttpClient http_client = HttpClient(gsm_client_secure_modem, FIREBASE_HOST, SSL_PORT);
 
unsigned long previousMillis = 0;

#define DHTPIN 4        // Pino de dados conectado ao sensor DHT (D4)
#define DHTTYPE DHT11   // Tipo de sensor DHT

DHT dht(DHTPIN, DHTTYPE);

void setup() {

  SerialMon.begin(115200);
  SerialAT.begin(9600, SERIAL_8N1, SIM808_RX_PIN, SIM808_TX_PIN); // Velocidade de comunicação do módulo SIM808
  SerialMon.println("Initializing modem...");
  modem.restart();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);
  http_client.setHttpResponseTimeout(10 * 1000); //^0 secs timeout

  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isNetworkConnected()) { SerialMon.println("Network connected"); }

  SerialMon.print(F("Connecting to GPRS..."));
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isGprsConnected()) { SerialMon.println("GPRS connected"); }

  if (modem.enableGPS()) { SerialMon.println("GPS conectado também"); }
  
  //SerialAT.println("AT+CIPGSMLOC=2,1");

  // Inicialize o sensor DHT
  dht.begin();
}

void loop() {if (!modem.isGprsConnected()) {
    SerialMon.println("GPRS disconnected, reconnecting...");
    modem.gprsConnect(apn, gprsUser, gprsPass);
    delay(5000);
  }

  if (!http_client.connected()) {
    SerialMon.println("Establishing new HTTP connection...");
    http_client.connect(FIREBASE_HOST, SSL_PORT);
    delay(1000);
  }

  if (http_client.connected()) {
    orquestrador();
    delay(3 * 60 * 1000); // 3 minute delay
  } else {
    SerialMon.println("Failed to connect, retrying in 10 seconds...");
    http_client.stop();
    delay(10000);
  }
}

//retornar um array de temper e humid

String* obterTemperaturaHumdidade(){
  // Criar um array de strings para armazenar os parâmetros divididos
  String* param = new String[2];
  
  float temper = dht.readTemperature();
  float humid = dht.readHumidity();

  if(isnan(temper)){
    temper = 0.1;
    humid = 0.1;
  }
  
  param[0] = String(temper);
  param[1] = String(humid);

  SerialMon.print(" TEMPERATURA: ");
  SerialMon.print(temper);
  SerialMon.print(" humid: ");
  SerialMon.print(humid);

  return param;
}

void orquestrador(){
  String* tempHumid = obterTemperaturaHumdidade();
  String dataHoraAtual = getDateTime();
  String dadosGps = obterLocalizacaoGps();

  SerialMon.print("=========== dadosGps: " + dadosGps + "\n");
  
  String* statusBateria = obterStatusBateria();
  String nivelBateria = statusBateria[0];
  String voltagemBateria = statusBateria[1];
  
  int qualidadeSinalGprs = obterQualidadeSinalGprs();

  String classificacaoSinal = obterClassificacaoSinal(qualidadeSinalGprs);


  // Crie um objeto JSON para armazenar os dados
  StaticJsonBuffer<300> jsonBuffer; // Especifique o tamanho do buffer conforme necessário
  JsonObject& root = jsonBuffer.createObject();

  // Adicione os dados ao objeto JSON
  root["ultimaAtualizacao"] = dataHoraAtual;
  root["temperatura"] = tempHumid[0];
  root["humidade"] = tempHumid[1];
  root["nivelBateria"] = nivelBateria;
  root["voltagemBateria"] = voltagemBateria;
  root["qualidadeSinalGprs"] = qualidadeSinalGprs;
  root["classificacaoSinal"] = classificacaoSinal;
  //root["localizacao"] = dadosGps;
  

// Imprima a string JSON 'dadosGps' antes de analisar
SerialMon.println("String dadosGps:");
SerialMon.println(dadosGps);

// Converta a string 'dadosGps' para um buffer estático
char buffer[dadosGps.length() + 1];
dadosGps.toCharArray(buffer, sizeof(buffer));

// Parse o buffer JSON para um objeto JSON 'localizacao'
StaticJsonBuffer<200> jsonBuffer3;
JsonObject& localizacao = jsonBuffer3.parseObject(buffer);

if (!localizacao.success()) {
  SerialMon.println("Falha ao analisar dadosGps como JSON");
} else {
  root["localizacao"] = localizacao;
}


  // Converta o objeto JSON em uma string
  String dados;
  root.printTo(dados);
  SerialMon.print(dados);
  

  // Montar os dados em formato JSON
  //String dados = "{\"humidade\":" + String(humidity) + ",\"temperatura\":" + String(temperature) + ",\"ultimaAtualizacao\":\"" + currentDateTime + "\"}";

  // Insira os dados no Firebase
  SetFirebase("POST", "/moto", dados, &http_client);



  // Liberar a memória alocada para o array
  delete[] statusBateria;
}

String obterLocalizacaoGps() {

  float gps_latitude  = 0;
  float gps_longitude = 0;
  float gps_speed     = 0;
  float gps_altitude  = 0;
  int   gps_vsat      = 0;
  int   gps_usat      = 0;
  float gps_accuracy  = 0;
  int   gps_year      = 0;
  int   gps_month     = 0;
  int   gps_day       = 0;
  int   gps_hour      = 0;
  int   gps_minute    = 0;
  int   gps_second    = 0;
  
  // Crie um objeto JSON para armazenar os dados
  StaticJsonBuffer<200> jsonBuffer1; // Especifique o tamanho do buffer conforme necessário
  JsonObject& locationObj = jsonBuffer1.createObject();
  String dados1;

  // tentar por 5 vezes
  for (int8_t i = 5; i; i--) {
    SerialMon.println("Requesting current GPS/GNSS/GLONASS location");
    if (modem.getGPS(&gps_latitude, &gps_longitude, &gps_speed, &gps_altitude,
                     &gps_vsat, &gps_usat, &gps_accuracy, &gps_year, &gps_month,
                     &gps_day, &gps_hour, &gps_minute, &gps_second)) {

      // Adicione os dados ao objeto JSON
      locationObj["Latitude"] = String(gps_latitude, 8);
      locationObj["Longitude"] = String(gps_longitude, 8);
      locationObj["Speed"] = gps_speed;
      locationObj["Altitude"] = gps_altitude;
      locationObj["VisibleSatellites"] = gps_vsat;
      locationObj["UsedSatellites"] = gps_usat;
      locationObj["Accuracy"] = gps_accuracy;
      locationObj["Year"] = gps_year;
      locationObj["Month"] = gps_month;
      locationObj["Day"] = gps_day;
      locationObj["Hour"] = gps_hour;
      locationObj["Minute"] = gps_minute;
      locationObj["Second"] = gps_second;
      
      break;
    } else {
      SerialMon.println("Couldn't get GPS/GNSS/GLONASS location, retrying in 15s.");
      delay(15000L);
    }
  }

  // SerialMon.println("Retrieving GPS/GNSS/GLONASS location again as a string");
  // String gps_raw = modem.getGPSraw();

  //SerialMon.println(gps_raw);



  // Converta o objeto JSON em uma string
  locationObj.printTo(dados1);
  SerialMon.print(dados1);

  return dados1;
}


String getDateTime() {
  SerialAT.println("AT+CCLK?");
  delay(500);

  String response = "";
  while (SerialAT.available()) {
    char c = SerialAT.read();
    response += c;
  }

  // Procurar pela sequência de caracteres "+CCLK: " na resposta
  int cclkIndex = response.indexOf("+CCLK: ");
  if (cclkIndex != -1) {
    // Extrair a data e hora da resposta
    String dateTimeStr = response.substring(cclkIndex + 8, cclkIndex + 25); // Começa após "+CCLK: " e exclui as aspas duplas

    // Reorganize a string para o formato desejado: "18/04/24 23:32:03"
    String formattedDateTime = dateTimeStr.substring(6, 8) + "/" + // Dia
                                dateTimeStr.substring(3, 5) + "/" + // Mês
                                dateTimeStr.substring(0, 2) + " " + // Ano
                                dateTimeStr.substring(9);           // Hora e minutos

    SerialMon.println("Data e hora atual: " + formattedDateTime);
    return formattedDateTime;
  } else {
    SerialMon.println("Resposta inválida do módulo SIM808.");
    return ""; // Retornar uma string vazia se a resposta for inválida
  }
}

String obterClassificacaoSinal(int signalQuality) {
  if (signalQuality == 99) {
    return "Sem sinal";
  } else if (signalQuality >= 0 && signalQuality <= 9) {
    return "Péssimo";
  } else if (signalQuality >= 10 && signalQuality <= 14) {
    return "Ruim";
  } else if (signalQuality >= 15 && signalQuality <= 19) {
    return "Aceitável";
  } else if (signalQuality >= 20 && signalQuality <= 30) {
    return "Bom";
  } else if (signalQuality >= 31 && signalQuality <= 99) {
    return "Ótimo";
  } else {
    return "Desconhecido";
  }
}

int obterQualidadeSinalGprs(){
  SerialMon.println("Verificando a qualidade do sinal...");
  int signalQuality = modem.getSignalQuality();

  // SerialMon.print("Qualidade do sinal: ");
  // SerialMon.print(signalQuality);
  // SerialMon.print(" - ");
  // SerialMon.println(classification);

  return int(signalQuality);
}


String* obterStatusBateria() {
  SerialAT.println("AT+CBC");
  delay(500);

  String response = "";
  while (SerialAT.available()) {
    char c = SerialAT.read();
    response += c;
  }

  // Remover caracteres especiais extras da resposta
  response.trim(); // Remove espaços em branco no início e no final
  response.replace("\r\n", ""); // Remove todas as ocorrências de "\r\n"
  response.replace("OK", ""); // Remove todas as ocorrências de "OK"

  // Criar um array de strings para armazenar os parâmetros divididos
  String* param = new String[2];

  // Copie a string para um array de caracteres
  char str[response.length() + 1];
  response.toCharArray(str, response.length() + 1);

  // Divida a string em tokens usando strtok
  char* token = strtok(str, ",");

  // Itere sobre os tokens e armazene-os no array param
  int i = 0;
  while (token != NULL && i < 3) {
    // Armazene apenas o segundo e o terceiro parâmetros
    if (i > 0) {
      param[i - 1] = String(token);
    }
    token = strtok(NULL, ",");
    i++;
  }

  return param;
}

void GetFirebase(const char* method, const String & path ,  HttpClient* http) {
  String response;
  int statusCode = 0;
  http->connectionKeepAlive(); // Currently, this is needed for HTTPS
 
  String url;
  if (path[0] != '/') {
    url = "/";
  }
  url += path + ".json";
  url += "?auth=" + FIREBASE_AUTH;

  http->get(url);
 
  //statusCode = http->responseStatusCode();
  //SerialMon.print("Status code: ");
  //SerialMon.println(statusCode);
  response = http->responseBody();
 
  SerialMon.print("Response: ");
  SerialMon.println(response);

  if (!http->connected()) {
    SerialMon.println();
    http->stop(); // Shutdown
    SerialMon.println("HTTP POST disconnected");
  }
}

void SetFirebase(const char* method, const String & path , const String & data, HttpClient* http) {
  String response;
  int statusCode = 0;
  int retries = 3;

  while (retries > 0) {
    if (!http->connected()) {
      SerialMon.println("Reconnecting HTTP...");
      http->connect(FIREBASE_HOST, SSL_PORT);
      delay(1000);
    }

    if (http->connected()) {
      String url = path[0] != '/' ? "/" : "";
      url += path + ".json?auth=" + FIREBASE_AUTH;

      http->post(url, "application/json", data);
      statusCode = http->responseStatusCode();
      
      SerialMon.print("Status code: ");
      SerialMon.println(statusCode);
      
      if (statusCode > 0) {
        response = http->responseBody();
        SerialMon.print("Response: ");
        SerialMon.println(response);
        break;
      }
    }
    
    retries--;
    if (retries > 0) {
      SerialMon.println("Retrying...");
      delay(5000);
    }
  }

  http->stop();
}
