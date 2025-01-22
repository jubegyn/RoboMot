#include <Arduino.h>
#include "temperatura.h"

#include "DHT.h"
#define DHTPIN 4       // Pino de dados conectado ao sensor DHT (D4)
#define DHTTYPE DHT11  // Tipo de sensor DHT

DHT dht(DHTPIN, DHTTYPE);

float tempInicial = 0.0;
float humidInicial = 0.0;

void setupDHT11() {
  Serial.println("Initializing DHT22 ...");
  dht.begin();
  tempInicial = dht.readTemperature();
  humidInicial = dht.readHumidity();
  Serial.print(" TEMPERATURA INICIAL: ");
  Serial.print(tempInicial);
  Serial.print(" HUMIDADE INICIAL: ");
  Serial.print(humidInicial);
  Serial.println("");
}

void verificandoTemperatura() {
  float tempAtual = dht.readTemperature();
  float humidAtual = dht.readHumidity();

  if(abs(tempInicial - tempAtual) >= 1.0){
    Serial.print(" TEMPERATURA mudou ");
  }else if(abs(humidInicial - humidAtual) >= 10.0){
    Serial.print(" HUMIDADE mudou ");
  }

  if(abs(tempInicial - tempAtual) >= 1.0 || abs(humidInicial - humidAtual) >= 10.0){
    Serial.print(" TEMPERATURA: ");
    Serial.print(tempAtual);
    Serial.print(" humid: ");
    Serial.print(humidAtual);
    Serial.println("");
  }
}
