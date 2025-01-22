#define TINY_GSM_MODEM_SIM808  // Use this line to select the SIM808 modem
#include <TinyGsmClient.h>
#include <ArduinoJson.h>       //versão: 5.13.4 - by Benoit Blanchon
#include <ArduinoHttpClient.h>

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



void setupSIM808(){
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
  
}


 void enviarRequisicao(){
  if (!modem.isGprsConnected()) {
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





