/*
 * Device and application specific definitions for CANopenNode.
 * Adapted to match STM32_CAN Arduino-compatible driver
 */

#ifndef CO_DRIVER_TARGET_H
#define CO_DRIVER_TARGET_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <Arduino.h>


#ifdef __cplusplus
extern "C" {
#endif

// Determining the CANOpen Driver

#if defined(FDCAN) || defined(FDCAN1) || defined(FDCAN2) || defined(FDCAN3)
#define CO_STM32_FDCAN_Driver 1
#elif defined(CAN) || defined(CAN1) || defined(CAN2) || defined(CAN3)
#define CO_STM32_CAN_Driver 1
#else
#error This STM32 Do not support CAN or FDCAN
#endif

#undef CO_CONFIG_STORAGE_ENABLE // We don't need Storage option, implement based on your use case and remove this line from here

//#ifdef CO_DRIVER_CUSTOM
//#include "CO_driver_custom.h"
//#endif

/* Stack configuration override default values. */
#define CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE CO_CONFIG_FLAG_CALLBACK_PRE
#define CO_CONFIG_GLOBAL_FLAG_TIMERNEXT CO_CONFIG_FLAG_TIMERNEXT

#undef CO_CONFIG_NMT
#define CO_CONFIG_NMT (CO_CONFIG_NMT_CALLBACK_CHANGE | CO_CONFIG_NMT_MASTER | CO_CONFIG_GLOBAL_FLAG_TIMERNEXT)

#undef CO_CONFIG_HB_CONS
#define CO_CONFIG_HB_CONS (CO_CONFIG_HB_CONS_ENABLE | CO_CONFIG_HB_CONS_QUERY_FUNCT | CO_CONFIG_GLOBAL_FLAG_TIMERNEXT)

#undef CO_CONFIG_EM
#define CO_CONFIG_EM (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_CONSUMER | CO_CONFIG_GLOBAL_FLAG_TIMERNEXT)

#undef CO_CONFIG_FIFO
#define CO_CONFIG_FIFO (CO_CONFIG_FIFO_ENABLE)

#undef CO_CONFIG_SDO_CLI
#define CO_CONFIG_SDO_CLI (CO_CONFIG_SDO_CLI_ENABLE | CO_CONFIG_SDO_CLI_SEGMENTED | CO_CONFIG_SDO_CLI_LOCAL | CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE | CO_CONFIG_GLOBAL_FLAG_TIMERNEXT)

#undef CO_CONFIG_PDO
#define CO_CONFIG_PDO (CO_CONFIG_RPDO_ENABLE | CO_CONFIG_TPDO_ENABLE | CO_CONFIG_RPDO_TIMERS_ENABLE | CO_CONFIG_TPDO_TIMERS_ENABLE | CO_CONFIG_FLAG_CALLBACK_PRE | CO_CONFIG_GLOBAL_FLAG_TIMERNEXT)

#undef CO_CONFIG_LEDS
#define CO_CONFIG_LEDS (CO_CONFIG_LEDS_ENABLE | CO_CONFIG_FLAG_TIMERNEXT)

#undef CO_CONFIG_STORAGE
#define CO_CONFIG_STORAGE 0x00

#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x) x
#define CO_SWAP_32(x) x
#define CO_SWAP_64(x) x

typedef uint_fast8_t bool_t;
typedef float float32_t;
typedef double float64_t;

//typedef struct CAN_message_t CAN_message_t;  // juste une déclaration


/* Received message object */
typedef struct {
    uint16_t ident;
    uint16_t mask;
    void *object;
    void (*CANrx_callback)(void *object, void *message);
} CO_CANrx_t;

/* Transmit message object */
typedef struct {
    uint32_t ident;
    uint8_t DLC;
    uint8_t data[8];
    volatile bool_t bufferFull;
    volatile bool_t syncFlag;
} CO_CANtx_t;

/* CAN module object */
typedef struct {
    void *CANptr;
    CO_CANrx_t *rxArray;
    uint16_t rxSize;
    CO_CANtx_t *txArray;
    uint16_t txSize;
    uint16_t CANerrorStatus;
    volatile bool_t CANnormal;
    volatile bool_t useCANrxFilters;
    volatile bool_t bufferInhibitFlag;
    volatile bool_t firstCANtxMessage;
    volatile uint16_t CANtxCount;
    uint32_t errOld;
} CO_CANmodule_t;

/* Data storage object for one entry */
typedef struct {
    void *addr;
    size_t len;
    uint8_t subIndexOD;
    uint8_t attr;
    void *addrNV;
} CO_storage_entry_t;

/* Critical section locking */
#define CO_LOCK_CAN_SEND(CAN_MODULE) __disable_irq()
#define CO_UNLOCK_CAN_SEND(CAN_MODULE) __enable_irq()
#define CO_LOCK_EMCY(CAN_MODULE)      CO_LOCK_CAN_SEND(CAN_MODULE)
#define CO_UNLOCK_EMCY(CAN_MODULE)    CO_UNLOCK_CAN_SEND(CAN_MODULE)
#define CO_LOCK_OD(CAN_MODULE)        CO_LOCK_CAN_SEND(CAN_MODULE)
#define CO_UNLOCK_OD(CAN_MODULE)      CO_UNLOCK_CAN_SEND(CAN_MODULE)

#define CO_MemoryBarrier() __DMB()
#define CO_FLAG_READ(rxNew) ((rxNew) != NULL)
#define CO_FLAG_SET(rxNew)  { CO_MemoryBarrier(); rxNew = (void*)1L; }
#define CO_FLAG_CLEAR(rxNew) { CO_MemoryBarrier(); rxNew = NULL; }

uint16_t CO_CANrxMsg_readIdent(void *msg);
/*CO_CANrxMsg_readIdent est une fonction C++ déclarée avec un linkage C qui 
extrait et retourne l'identifiant sur 11 bits d'un message CAN (type CAN_message_t) 
passé en paramètre via un pointeur void *. Elle utilise un masque binaire (0x7FF) 
pour isoler les 11 bits de l'identifiant.*/


uint8_t *CO_CANrxMsg_readData(void *msg);
/*La fonction CO_CANrxMsg_readData est une fonction C compatible avec le 
linkage C (extern "C") qui prend un pointeur générique void *msg en paramètre. 
Elle retourne un pointeur vers un tableau d'octets (uint8_t *) correspondant 
au champ buf de la structure CAN_message_t pointée par msg.*/

uint8_t  CO_CANrxMsg_readDLC(void *msg); 
/*La fonction CO_CANrxMsg_readDLC est une fonction C compatible avec l'ABI C 
(extern "C") qui prend un pointeur void *msg représentant un message CAN et 
retourne un entier non signé de 8 bits (uint8_t) correspondant à la longueur 
du message (len) extraite de la structure CAN_message_t.*/


/* Prototypes */
void CO_CANinterruptRx(CO_CANmodule_t *CANmodule);

#ifdef __cplusplus
}
#endif

#endif /* CO_DRIVER_TARGET_H */
