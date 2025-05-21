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

typedef struct CAN_message_t CAN_message_t;  // juste une déclaration


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
uint8_t *CO_CANrxMsg_readData(void *msg);
uint8_t  CO_CANrxMsg_readDLC(void *msg);


/* Prototypes */
void CO_CANinterruptRx(CO_CANmodule_t *CANmodule);

#ifdef __cplusplus
}
#endif

#endif /* CO_DRIVER_TARGET_H */
