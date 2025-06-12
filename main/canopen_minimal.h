/*Librairie de test - A SUPPRIMER*/

#ifndef CANOPEN_MINIMAL_H
#define CANOPEN_MINIMAL_H

#include "STM32_CAN.h"

void CO_minimal_init(STM32_CAN &can);
void CO_minimal_process(const CAN_message_t &msg, STM32_CAN &can);
void CO_minimal_tick(STM32_CAN &can);
void sendSDOResponse(uint32_t request_id, uint8_t *data, uint8_t len, STM32_CAN &can);

#endif
