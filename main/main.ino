#include <Arduino.h>
#include "STM32_CAN.h"
#include "canopen_minimal.h"


STM32_CAN can1(PA11, PA12);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("CANopen minimal démarrage...");

  can1.begin();
  can1.setBaudRate(500000);

  CO_minimal_init(can1);
}

void loop() {
  CAN_message_t msg;
  while (can1.read(msg)) {
    CO_minimal_process(msg, can1);  // Traite les messages CAN
  }

  CO_minimal_tick(can1);  // Tick pour gérer timeouts/synchro
  delay(1);
}
