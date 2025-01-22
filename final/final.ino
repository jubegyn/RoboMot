
#include <Wire.h>
#include "temperatura.h"
#include "acelerometro.h"




void setup(void) {
  Serial.begin(115200);
  while (!Serial)
    delay(10); // will pause Zero, Leonardo, etc until serial console opens

  setupDHT11();
  setupMPU6050();
  delay(100);
}



void loop() {

  verificandoTemperatura();
  //if alarme == true
  verificandoMovimento();

  delay(1000);
}
