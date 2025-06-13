/*
 * CAN module object for generic microcontroller.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver.c
 * @ingroup     CO_driver
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License atat
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 #include <stdio.h>

#define log_printf(macropar_message, ...) printf(macropar_message, ##__VA_ARGS__)


#include "CANopen.h"


#include "hal_conf_extra.h"
#include <STM32_CAN.h>

#include "CO_app_STM32.h"

extern CAN_message_t msg;
extern bool messagePending;


// le block permettant de faire des déclarations du target de manière propre sans avoir d'erreur du à l'inclusion de la bibliothéque stm32_can dans mon target.h
extern "C" {
  uint16_t CO_CANrxMsg_readIdent(void *msg) {
    return ((CAN_message_t *)msg)->id & 0x7FF;
  }

  uint8_t *CO_CANrxMsg_readData(void *msg) {
    return ((CAN_message_t *)msg)->buf;
  }

  uint8_t CO_CANrxMsg_readDLC(void *msg) {
    return ((CAN_message_t *)msg)->len;
  }
}


/* Local CAN module object */
static CO_CANmodule_t* CANModule_local = NULL; /* Local instance of global CAN module */

/* CAN masks for identifiers */
#define CANID_MASK 0x07FF /*!< CAN standard ID mask */
#define FLAG_RTR   0x8000 /*!< RTR flag, part of identifier */



/******************************************************************************/

void CO_CANsetConfigurationMode(void* CANptr) {
    /* Put CAN module in configuration mode */
    stm32_can_t* internalCAN = (stm32_can_t*)CANptr;
    if (CANptr != NULL) {
#ifdef CO_STM32_FDCAN_Driver
        HAL_FDCAN_Stop(&internalCAN->handle);
#else
        HAL_CAN_Stop(&internalCAN->handle);
#endif
    }
}

/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule) {

  CANmodule->CANnormal = true;
}


static CO_CANrx_t *rxArrayGlobal = NULL;
static uint16_t rxSizeGlobal = 0;
static void *CANptrGlobal = NULL;

CO_ReturnError_t CO_CANmodule_init(
  CO_CANmodule_t *CANmodule,
  void *CANptr,
  CO_CANrx_t rxArray[],
  uint16_t rxSize,
  CO_CANtx_t txArray[],
  uint16_t txSize,
  uint16_t CANbitRate) {
  log_printf("CO_CANmodule_init\n");
  if (!CANmodule || !rxArray || !txArray) return CO_ERROR_ILLEGAL_ARGUMENT;
  log_printf("CO_CANmodule_init 2\n");
  log_printf("Voic le petit baudrate: %d\n", CANbitRate);


  CANmodule->CANptr = CANptr;
  CANmodule->rxArray = rxArray;
  CANmodule->rxSize = rxSize;
  CANmodule->txArray = txArray;
  CANmodule->txSize = txSize;
  CANmodule->CANnormal = false;
  CANmodule->firstCANtxMessage = true;
  CANmodule->useCANrxFilters = false;
  CANmodule->bufferInhibitFlag = false;
  CANmodule->CANtxCount = 0;
  CANmodule->errOld = 0;

  /* Reset all variables */
  for (uint16_t i = 0U; i < rxSize; i++) {
    rxArray[i].ident = 0U;
    rxArray[i].mask = 0xFFFFU;
    rxArray[i].object = NULL;
    rxArray[i].CANrx_callback = NULL;
  }
  for (uint16_t i = 0; i < txSize; i++) {
    txArray[i].bufferFull = false;
  }



  STM32_CAN *can = static_cast<STM32_CAN *>(CANptr);
  can->begin();             // ces appels DOIVENT afficher tes prints
  log_printf("Voic le petit baudrate: %d\n", CANbitRate);

  can->setBaudRate(CANbitRate*1000);

  log_printf("CO_CANmodule_init 3\n");

  return CO_ERROR_NO;
}

/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t* CANmodule) {
    stm32_can_t* internalCAN = (stm32_can_t*)CANmodule->CANptr;
    if (CANmodule != NULL && CANmodule->CANptr != NULL) {
#ifdef CO_STM32_FDCAN_Driver
        HAL_FDCAN_Stop(&internalCAN->handle);
#else
        HAL_CAN_Stop(&internalCAN->handle);
#endif
    }
}

/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, uint16_t mask, bool_t rtr, void *object,
                                    void (*CANrx_callback)(void *object, void *message)) {

  //log_printf("CO_CANrxBufferInit\n");
  CO_ReturnError_t ret = CO_ERROR_NO;

  log_printf("Buffer enregistré : index = %d, ID = 0x%03X, mask = 0x%03X, cb = %p\n",
              index, ident, mask, CANmodule->rxArray[index].CANrx_callback);


  if ((CANmodule != NULL) && (CANrx_callback != NULL) && (index < CANmodule->rxSize)) {

    //log_printf("je suis rentrer dans la condition buffer init\n");
    /* buffer, which will be configured */
    CO_CANrx_t *buffer = &CANmodule->rxArray[index];

    /* Configure object variables */
    buffer->object = object;

    buffer->CANrx_callback = CANrx_callback;
    log_printf("Callback assigné : index=%d, cb=%p\n", index, CANrx_callback);


    /* CAN identifier and CAN mask, bit aligned with CAN module. Different on different microcontrollers. */
    /*
      /* Configure global identifier, including RTR bit
         *
         * This is later used for RX operation match case
         */
    buffer->ident = (ident & CANID_MASK) | (rtr ? FLAG_RTR : 0x00);
    buffer->mask = (mask & CANID_MASK) | FLAG_RTR;

    /* Set CAN hardware module filter and mask. */
    if (CANmodule->useCANrxFilters) {
      __NOP();
    }
  } else {
    ret = CO_ERROR_ILLEGAL_ARGUMENT;
  }

  return ret;
}

/******************************************************************************/
CO_CANtx_t *CO_CANtxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, bool_t rtr, uint8_t noOfBytes, bool_t syncFlag) {
  CO_CANtx_t *buffer = NULL;

  if ((CANmodule != NULL) && (index < CANmodule->txSize)) {
    /* get specific buffer */
    buffer = &CANmodule->txArray[index];

    /* CAN identifier, DLC and rtr, bit aligned with CAN module transmit buffer */
    buffer->ident = ((uint32_t)ident & CANID_MASK) | ((uint32_t)(rtr ? FLAG_RTR : 0x00));
    buffer->DLC = noOfBytes;
    buffer->bufferFull = false;
    buffer->syncFlag = syncFlag;
  }

  return buffer;
}

/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer) {
  if (!CANmodule || !buffer) return CO_ERROR_ILLEGAL_ARGUMENT;

  CO_LOCK_CAN_SEND(CANmodule);

  if (buffer->bufferFull && !CANmodule->firstCANtxMessage) {
    CANmodule->CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
    CO_UNLOCK_CAN_SEND(CANmodule);
    return CO_ERROR_TX_OVERFLOW;
  }

  CAN_message_t msg;
  msg.id = buffer->ident;// >> 2;
  msg.len = buffer->DLC;
  memcpy(msg.buf, buffer->data, msg.len);

  STM32_CAN *can = (STM32_CAN *)CANmodule->CANptr;

  bool success = can->write(msg);

  if(success){
    CANmodule->bufferInhibitFlag = buffer->syncFlag;
  } else {
    buffer->bufferFull = true;
    CANmodule->CANtxCount++;
  }

  CO_UNLOCK_CAN_SEND(CANmodule);

  return success ? CO_ERROR_NO : CO_ERROR_TX_OVERFLOW;
}

/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule) /*pas modifiée*/
{
  uint32_t tpdoDeleted = 0U;

  CO_LOCK_CAN_SEND(CANmodule);

  if (CANmodule->bufferInhibitFlag) {
    // Hypothèse : si inhibitFlag est actif, un TPDO était prêt
    // mais on ne peut pas vraiment l'annuler → on signale quand même
    CANmodule->bufferInhibitFlag = false;
    tpdoDeleted = 1U;
  }

  if (CANmodule->CANtxCount != 0U) {
    uint16_t i;
    CO_CANtx_t *buffer = &CANmodule->txArray[0];
    for (i = CANmodule->txSize; i > 0U; i--) {
      if (buffer->bufferFull && buffer->syncFlag) {
        buffer->bufferFull = false;
        CANmodule->CANtxCount--;
        tpdoDeleted = 2U;
      }
      buffer++;
    }
  }

  CO_UNLOCK_CAN_SEND(CANmodule);

  if (tpdoDeleted != 0U) {
    CANmodule->CANerrorStatus |= CO_CAN_ERRTX_PDO_LATE;
  }
}
/******************************************************************************/
/* Get error counters from the module. If necessary, function may use
 * different way to determine errors. */
static uint16_t rxErrors = 0, txErrors = 0, overflow = 0;

void CO_CANmodule_process(CO_CANmodule_t* CANmodule) {
    uint32_t err = 0;


    stm32_can_t* internalCAN = (stm32_can_t*)(CANmodule->CANptr);

    // CANOpen just care about Bus_off, Warning, Passive and Overflow
    // I didn't find overflow error register in STM32, if you find it please let me know

#ifdef CO_STM32_FDCAN_Driver

    err = ((FDCAN_HandleTypeDef*) &internalCAN->handle)->Instance->PSR
          & (FDCAN_PSR_BO | FDCAN_PSR_EW | FDCAN_PSR_EP);

    if (CANmodule->errOld != err) {

        uint16_t status = CANmodule->CANerrorStatus;

        CANmodule->errOld = err;

        if (err & FDCAN_PSR_BO) {
            status |= CO_CAN_ERRTX_BUS_OFF;
            // In this driver we expect that the controller is automatically handling the protocol exceptions.

        } else {
            /* recalculate CANerrorStatus, first clear some flags */
            status &= 0xFFFF
                      ^ (CO_CAN_ERRTX_BUS_OFF | CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE | CO_CAN_ERRTX_WARNING
                         | CO_CAN_ERRTX_PASSIVE);

            if (err & FDCAN_PSR_EW) {
                status |= CO_CAN_ERRRX_WARNING | CO_CAN_ERRTX_WARNING;
            }

            if (err & FDCAN_PSR_EP) {
                status |= CO_CAN_ERRRX_PASSIVE | CO_CAN_ERRTX_PASSIVE;
            }
        }

        CANmodule->CANerrorStatus = status;
    }
#else

    err = ((CAN_HandleTypeDef*) &internalCAN->handle)->Instance->ESR
          & (CAN_ESR_BOFF | CAN_ESR_EPVF | CAN_ESR_EWGF);

    //    uint32_t esrVal = ((CAN_HandleTypeDef*)((CANopenNodeSTM32*)CANmodule->CANptr)->CANHandle)->Instance->ESR; Debug purpose
    if (CANmodule->errOld != err) {

        uint16_t status = CANmodule->CANerrorStatus;

        CANmodule->errOld = err;

        if (err & CAN_ESR_BOFF) {
            status |= CO_CAN_ERRTX_BUS_OFF;
            // In this driver, we assume that auto bus recovery is activated ! so this error will eventually handled automatically.

        } else {
            /* recalculate CANerrorStatus, first clear some flags */
            status &= 0xFFFF
                      ^ (CO_CAN_ERRTX_BUS_OFF | CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE | CO_CAN_ERRTX_WARNING
                         | CO_CAN_ERRTX_PASSIVE);

            if (err & CAN_ESR_EWGF) {
                status |= CO_CAN_ERRRX_WARNING | CO_CAN_ERRTX_WARNING;
            }

            if (err & CAN_ESR_EPVF) {
                status |= CO_CAN_ERRRX_PASSIVE | CO_CAN_ERRTX_PASSIVE;
            }
        }

        CANmodule->CANerrorStatus = status;
    }

#endif
}

/******************************************************************************/
void CO_CANinterruptRx(CO_CANmodule_t *CANmodule) {
    log_printf("MESSAGE REÇU !\n");

    if (!messagePending) {
        log_printf("Pas de message en attente.\n");
        return;
    }

    log_printf("Début traitement message...\n");

    uint32_t ident = msg.id & 0x7FF;
    CO_CANrx_t *buffer = NULL;
    bool msgMatched = false;

    log_printf("CAN ID reçu : 0x%03X\n", ident);
    log_printf("Recherche d'un buffer correspondant dans rxArray (%d entrées)...\n", CANmodule->rxSize);

    for (uint16_t i = 0; i < CANmodule->rxSize; i++) {
        buffer = &CANmodule->rxArray[i];
        log_printf(" - Index %d : ident = 0x%03X, mask = 0x%03X\n", i, buffer->ident, buffer->mask);

        if (((ident ^ buffer->ident) & buffer->mask) == 0U) {
            log_printf("   -> Correspondance trouvée à l'index %d\n", i);
            msgMatched = true;
            break;
        }
    }

    if (!msgMatched) {
        log_printf("Aucun buffer correspondant trouvé pour l'ID 0x%03X\n", ident);
    }

    if (msgMatched) {
        if (buffer == NULL) {
            log_printf("ERREUR: Buffer est NULL malgré msgMatched = true !\n");
        } else {
            log_printf("Buffer trouvé : %p\n", (void*)buffer);
            log_printf(" - Adresse du callback : %p\n", (void*)buffer->CANrx_callback);
            log_printf(" - Objet : %p\n", buffer->object);

            if (buffer->CANrx_callback == NULL) {
                log_printf("ERREUR: Fonction de rappel CANrx_callback est NULL.\n");
            } else {
                // Copier le message pour éviter d'éventuelles corruptions
                CAN_message_t localMsg = msg;

                log_printf("Callback address = %p\n", buffer->CANrx_callback);

                // Essayons de lire le code à cette adresse pour valider l'exécution
                uint32_t *fn_ptr = (uint32_t*) buffer->CANrx_callback;
                log_printf("Première instruction à l’adresse : 0x%08X\n", *fn_ptr);

                log_printf("Appel de la fonction de rappel...\n");

                // Bloc try-catch n'existe pas en C, mais on entoure l'appel de logs
                buffer->CANrx_callback(buffer->object, &localMsg);

                log_printf("Fonction de rappel exécutée sans crash.\n");
            }
        }
    }

    messagePending = false;
    log_printf("Fin traitement message.\n\n");
}



// ---------------- SDO client helpers ----------------


/*

CO_SDO_abortCode_t read_SDO(CO_SDOclient_t *SDO_C, uint8_t nodeId, uint16_t index, uint8_t subIndex, uint8_t *buf, size_t bufSize, size_t *readSize) {
  CO_SDO_return_t ret;
  CO_SDO_abortCode_t abort = CO_SDO_AB_NONE;

  ret = CO_SDOclient_setup(SDO_C, 0x600 + nodeId, 0x580 + nodeId, nodeId);
  if (ret != CO_SDO_RT_ok_communicationEnd) return CO_SDO_AB_GENERAL;

  ret = CO_SDOclientUploadInitiate(SDO_C, index, subIndex, 1000, false);
  if (ret != CO_SDO_RT_ok_communicationEnd) return CO_SDO_AB_GENERAL;

  do {
    uint32_t time_us = 1000;
    ret = CO_SDOclientUpload(SDO_C, time_us, false, &abort, NULL, NULL, NULL);
  } while (ret > 0);

  *readSize = CO_SDOclientUploadBufRead(SDO_C, buf, bufSize);
  return abort;
}

CO_SDO_abortCode_t write_SDO(CO_SDOclient_t *SDO_C, uint8_t nodeId, uint16_t index, uint8_t subIndex, uint8_t *data, size_t dataSize) {
  CO_SDO_return_t ret;
  CO_SDO_abortCode_t abort = CO_SDO_AB_NONE;

  ret = CO_SDOclient_setup(SDO_C, 0x600 + nodeId, 0x580 + nodeId, nodeId);
  if (ret != CO_SDO_RT_ok_communicationEnd) return CO_SDO_AB_GENERAL;

  ret = CO_SDOclientDownloadInitiate(SDO_C, index, subIndex, dataSize, 1000, false);
  if (ret != CO_SDO_RT_ok_communicationEnd) return CO_SDO_AB_GENERAL;

  CO_SDOclientDownloadBufWrite(SDO_C, data, dataSize);

  do {
    uint32_t time_us = 1000;
    ret = CO_SDOclientDownload(SDO_C, time_us, false, false, &abort, NULL, NULL);
  } while (ret > 0);

  return abort;
}

*/
