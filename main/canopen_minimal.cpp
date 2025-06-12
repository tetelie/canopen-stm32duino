#include "canopen_minimal.h"

// Exemple d'OD : index 0x2000 → variable 8 bits
uint8_t OD_2000 = 0x42;

void CO_minimal_init(STM32_CAN &can) {
  // Init des structures internes si besoin
}

void sendSDOResponse(uint32_t request_id, uint8_t *data, uint8_t len, STM32_CAN &can) {
  CAN_message_t resp;
  resp.id = request_id | 0x580;
  resp.len = len;
  memcpy(resp.buf, data, len);
  can.write(resp);
}


void CO_minimal_process(const CAN_message_t &msg, STM32_CAN &can) {
  // SDO request (0x600 + node ID)
  if ((msg.id & 0x7F0) == 0x600 && msg.len >= 8) {
    uint8_t cmd = msg.buf[0];
    uint16_t index = msg.buf[2] << 8 | msg.buf[1];
    uint8_t sub = msg.buf[3];

    if ((cmd & 0xF0) == 0x40) { // SDO read
      if (index == 0x2000 && sub == 0x00) {
        uint8_t resp[8] = {0x47, 0x00, 0x20, 0x00, OD_2000, 0, 0, 0};
        sendSDOResponse(msg.id, resp, 8, can);
      }
    }

    // Tu pourrais aussi ajouter ici l’écriture d’OD avec 0x2F, etc.
  }

  // Gère NMT (ex : 0x000) → en option
  if (msg.id == 0x000 && msg.len >= 2) {
    uint8_t cmd = msg.buf[0];
    uint8_t node = msg.buf[1];

    Serial.print("NMT cmd: ");
    Serial.print(cmd);
    Serial.print(" for node ");
    Serial.println(node);
  }
}

void CO_minimal_tick(STM32_CAN &can) {
  // Ajoute gestion heartbeat ou watchdog ici si besoin
}
