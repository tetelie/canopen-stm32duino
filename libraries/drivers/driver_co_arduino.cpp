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
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * FR :
 * Ce fichier est une implémentation spécifique pour le STM32,
 * utilisée par la pile CANopenNode (implémentation open-source de CANopen).
 * Il fournit les fonctions d'initialisation, de lecture/écriture, et de gestion
 * d'erreur pour le contrôleur CAN.
 */


#include "301/CO_driver.h"
#include "CO_app_STM32.h"
#include "CANopen.h"
#include "hal_conf_extra.h"
#include <STM32_CAN.h>
#include <stdio.h>

#define log_printf(macropar_message, ...) printf(macropar_message, ##__VA_ARGS__)

extern CAN_message_t msg;       // Message CAN reçu [STM32_CAN]
extern bool messagePending;     // Indicateur : un message CAN a été reçu


/*Le block permettant de faire des déclarations du target de manière propre sans avoir 
  d'erreur du à l'inclusion de la bibliothéque stm32_can dans le target.h */
extern "C" { 
  uint16_t CO_CANrxMsg_readIdent(void *msg) {
    return ((CAN_message_t *)msg)->id & 0x7FF; // identifiant sur 11 bits
  }

  uint8_t *CO_CANrxMsg_readData(void *msg) {
    return ((CAN_message_t *)msg)->buf;
  }

  uint8_t CO_CANrxMsg_readDLC(void *msg) {
    return ((CAN_message_t *)msg)->len;
  }
}


// Module CAN global local (singleton)
static CO_CANmodule_t* CANModule_local = NULL; // Instance du module CAN configuré

// Masques pour identifier les messages CAN
#define CANID_MASK 0x07FF // Masque sur les 11 bits d'un ID standard CAN
#define FLAG_RTR   0x8000 // Flag pour indiquer un message RTR



/******************************************************************************/
// Met le module CAN en mode configuration (avant init)
void CO_CANsetConfigurationMode(void* CANptr) {
  //Put CAN module in configuration mode
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
// Active le mode normal du module CAN (transmission/réception activée)
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule) {
  // Put CAN module in normal mode 
  if (CANmodule->CANptr != NULL) {
#ifdef CO_STM32_FDCAN_Driver
        if (HAL_FDCAN_Start(&((stm32_can_t*)CANmodule->CANptr)->handle) == HAL_OK)
#else
        if (HAL_CAN_Start(&((stm32_can_t*)CANmodule->CANptr)->handle) == HAL_OK)
#endif
        {
            CANmodule->CANnormal = true;
        }
    }
}

/******************************************************************************/
/*Initialise le module CAN avec les buffers de réception/transmission (RX et RT), 
 * et configure le matériel CAN STM32. 
 */
//static CO_CANrx_t *rxArrayGlobal = NULL;
//static uint16_t rxSizeGlobal = 0;
//static void *CANptrGlobal = NULL;

CO_ReturnError_t CO_CANmodule_init( CO_CANmodule_t *CANmodule,
                                    void *CANptr,
                                    CO_CANrx_t rxArray[],
                                    uint16_t rxSize,
                                    CO_CANtx_t txArray[],
                                    uint16_t txSize,
                                    uint16_t CANbitRate
                                  ) {
  log_printf("CO_CANmodule_init - 1\n");

  if (CANmodule == NULL || rxArray == NULL || txArray == NULL) {
    return CO_ERROR_ILLEGAL_ARGUMENT;
  }

  log_printf("CO_CANmodule_init - 2\n");
  log_printf("Baudrate : %d\n", CANbitRate);

  // Hold CANModule variable
  CANmodule->CANptr = CANptr;

  // Keep a local copy of CANModule
  CANModule_local = CANmodule;

  // Enregistrement des buffers et paramètres dans le module
  CANmodule->rxArray = rxArray;
  CANmodule->rxSize = rxSize;
  CANmodule->txArray = txArray;
  CANmodule->txSize = txSize;
  CANmodule->CANerrorStatus = 0;
  CANmodule->CANnormal = false;
  CANmodule->useCANrxFilters = false; 
  CANmodule->bufferInhibitFlag = false;
  CANmodule->firstCANtxMessage = true;
  CANmodule->CANtxCount = 0U;
  CANmodule->errOld = 0U;

  // Reset des buffers RX
  for (uint16_t i = 0U; i < rxSize; i++) {
    rxArray[i].ident = 0U;
    rxArray[i].mask = 0xFFFFU;
    rxArray[i].object = NULL;
    rxArray[i].CANrx_callback = NULL;
  }

  // Reset des flags TX
  for (uint16_t i = 0U; i < txSize; i++) {
    txArray[i].bufferFull = false;
  }

  // Initialisation de l'objet STM32_CAN (HAL custom)
  STM32_CAN *can = static_cast<STM32_CAN *>(CANptr);
  can->begin(); // Initialisation matérielle CAN
  can->setBaudRate(CANbitRate*1000); // Configuration du débit en bit/s
  log_printf("Baudrate : %d\n", CANbitRate);

  log_printf("CO_CANmodule_init - 3\n");

  // Surement incomplet
  // Voir ligne 125 : <https://github.com/CANopenNode/CanOpenSTM32/blob/master/CANopenNode_STM32/CO_driver_STM32.c#L125V>
  return CO_ERROR_NO;
}

/******************************************************************************/
// Désactive le module CAN (utile pour réinitialisation ou mise hors tension)
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
// Initialise un buffer de réception CAN (ID + masque + callback à appeler)
CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *CANmodule, 
                                    uint16_t index, 
                                    uint16_t ident, 
                                    uint16_t mask, 
                                    bool_t rtr, 
                                    void *object,
                                    void (*CANrx_callback)(void *object, void *message)
                                  ){
  CO_ReturnError_t ret = CO_ERROR_NO;

  if ((CANmodule != NULL) && (object != NULL) && (CANrx_callback != NULL) && (index < CANmodule->rxSize)) {
    // Récupère le buffer à configurer
    CO_CANrx_t *buffer = &CANmodule->rxArray[index];

    // Configure l'objet et la fonction de callback
    buffer->object = object;
    buffer->CANrx_callback = CANrx_callback;

    // Configurer l'identifiant global, y compris le bit RTR.
    // Ceci sera utilisé ultérieurement pour la correspondance des opérations RX.

    // Applique le masque et identifiant filtré (et bit RTR si nécessaire)
    buffer->ident = (ident & CANID_MASK) | (rtr ? FLAG_RTR : 0x00);
    buffer->mask  = (mask  & CANID_MASK) | FLAG_RTR;

    /* Set CAN hardware module filter and mask. */
    if (CANmodule->useCANrxFilters) {
      STM32_CAN *canDriver = (STM32_CAN *)CANmodule->CANptr;

      if (canDriver != NULL) {
        bool ok = canDriver->setFilterSingleMask(
          index,                    // Numéro de banque (peut être égal à l’index du buffer)
          ident,                    // ID à filtrer
          mask,                     // masque à appliquer
          STD,                      // Type d'identifiant : STD (à adapter si tu gères EXT plus tard)
          STM32_CAN::STORE_FIFO0,   // Action : mettre dans FIFO0
          true                      // Filtre activé
        );

        if (!ok) {
          ret = CO_ERROR_ILLEGAL_ARGUMENT;  // ou un autre code d'erreur adapté
        }
      }
    } else {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
      }
  }
  return ret;
}

/******************************************************************************/
// Initialise un buffer de transmission CAN.
CO_CANtx_t *CO_CANtxBufferInit( CO_CANmodule_t *CANmodule, 
                                uint16_t index, 
                                uint16_t ident, 
                                bool_t rtr, 
                                uint8_t noOfBytes, 
                                bool_t syncFlag
                              ) {
  CO_CANtx_t *buffer = NULL;

  if ((CANmodule != NULL) && (index < CANmodule->txSize)) {
    // Récupére le buffer à configurer
    buffer = &CANmodule->txArray[index];

    // Configuration des champs CAN : indentifiant, longueur, etc.
    buffer->ident = ((uint32_t)ident & CANID_MASK) | ((uint32_t)(rtr ? FLAG_RTR : 0x00));
    buffer->DLC = noOfBytes;
    buffer->bufferFull = false;
    buffer->syncFlag = syncFlag;
  }

  return buffer;
}

/******************************************************************************/
// Tente d'envoyer un message CAN en utilisant le buffer spécifié.
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer) {
  if (!CANmodule || !buffer) return CO_ERROR_ILLEGAL_ARGUMENT;

  CO_LOCK_CAN_SEND(CANmodule);

  // Vérifie si le buffer est déjà plein et ce n'est pas le premier envoi
  if (buffer->bufferFull && !CANmodule->firstCANtxMessage) {
    CANmodule->CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
    CO_UNLOCK_CAN_SEND(CANmodule);
    return CO_ERROR_TX_OVERFLOW;
  }

  CAN_message_t msg;
  msg.id = buffer->ident;
  msg.len = buffer->DLC;
  memcpy(msg.buf, buffer->data, msg.len);

  STM32_CAN *can = (STM32_CAN *)CANmodule->CANptr;
  bool success = can->write(msg); // Envoi matériel

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
// Annule les messages PDO synchrones en attente si nécessaire.
// == Fonction non modifiée ==
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule) {
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
/* Récupère les compteurs d'erreurs du module. Selon le matériel utilisé,
 * la méthode d'accès aux erreurs peut varier. Les erreurs surveillées sont 
 * celles du protocole CANopen : Bus Off, Avertissements, Passivité,        
 * mais pas les erreurs de débordement (overflow) spécifiques au STM32.
 */

static uint16_t rxErrors = 0, txErrors = 0, overflow = 0;

void CO_CANmodule_process(CO_CANmodule_t* CANmodule) {
    uint32_t err = 0;

    stm32_can_t* internalCAN = (stm32_can_t*)(CANmodule->CANptr);

    // CANOpen ne s'intéresse qu'aux erreurs Bus_off, Warning, Passive et Overflow. 
    // Je n'ai pas trouvé de registre d'erreur de dépassement dans STM32. 
    // Si vous le trouvez, merci de me le signaler.

#ifdef CO_STM32_FDCAN_Driver

    // Lecture de l'état d'erreur du contrôleur FDCAN (PSR : Protocol Status Register)
    err = ((FDCAN_HandleTypeDef*) &internalCAN->handle)->Instance->PSR & (FDCAN_PSR_BO | FDCAN_PSR_EW | FDCAN_PSR_EP);

    // Détection de changement d'état d'erreur
    if (CANmodule->errOld != err) {
        uint16_t status = CANmodule->CANerrorStatus;
        CANmodule->errOld = err;

        if (err & FDCAN_PSR_BO) { // Erreur : Bus off
            status |= CO_CAN_ERRTX_BUS_OFF;
            // In this driver we expect that the controller is automatically handling the protocol exceptions.

        } else { //Réinitialisation des flages d'erreur
            /* recalculate CANerrorStatus, first clear some flags */
            status &= 0xFFFF ^ (CO_CAN_ERRTX_BUS_OFF | CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE | CO_CAN_ERRTX_WARNING | CO_CAN_ERRTX_PASSIVE);
            
            if (err & FDCAN_PSR_EW) { // Erreur : Warning
                status |= CO_CAN_ERRRX_WARNING | CO_CAN_ERRTX_WARNING;
            }

            if (err & FDCAN_PSR_EP) { // Erreur : Passive
                status |= CO_CAN_ERRRX_PASSIVE | CO_CAN_ERRTX_PASSIVE;
            }
        }

        CANmodule->CANerrorStatus = status;
    }
#else
    // Lecture de l'état d'erreur du contrôleur CAN classique (ESR : Error Status Register)
    err = ((CAN_HandleTypeDef*) &internalCAN->handle)->Instance->ESR
          & (CAN_ESR_BOFF | CAN_ESR_EPVF | CAN_ESR_EWGF);

    //    uint32_t esrVal = ((CAN_HandleTypeDef*)((CANopenNodeSTM32*)CANmodule->CANptr)->CANHandle)->Instance->ESR; Debug purpose
    if (CANmodule->errOld != err) {
        uint16_t status = CANmodule->CANerrorStatus;
        CANmodule->errOld = err;

        if (err & CAN_ESR_BOFF) { // Erreur : Bus off
            status |= CO_CAN_ERRTX_BUS_OFF;
            // Dans ce pilote, nous supposons que la récupération automatique du bus est activée. Donc cette erreur sera finalement gérée automatiquement.

        } else {
            // Recalcule CANerrorStatus, efface d'abord les flags
            status &= 0xFFFF ^ (CO_CAN_ERRTX_BUS_OFF | CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE | CO_CAN_ERRTX_WARNING | CO_CAN_ERRTX_PASSIVE);

            if (err & CAN_ESR_EWGF) { // Erreur : Warning
                status |= CO_CAN_ERRRX_WARNING | CO_CAN_ERRTX_WARNING;
            }

            if (err & CAN_ESR_EPVF) { // Erreur : Passive
                status |= CO_CAN_ERRRX_PASSIVE | CO_CAN_ERRTX_PASSIVE;
            }
        }

        CANmodule->CANerrorStatus = status;
    }

#endif
}

/******************************************************************************/
/* Gère les interruptions de réception de messages CAN.                      
 * Vérifie s'il y a un message en attente (flag global).                    
 * Si oui, cherche dans le tableau rxArray un buffer qui correspond à l'ID. 
 * Si un match est trouvé, appelle la fonction de callback associée. 
 */

void CO_CANinterruptRx(CO_CANmodule_t *CANmodule) {
  if (!messagePending) return; // Pas de message à traiter

  uint32_t ident = msg.id & 0x7FF; // Récupére l'indentifiant CAN sans bits étendus
  CO_CANrx_t *buffer = NULL;
  bool msgMatched = false;

  // Parcourt les buffers de réception pour trouver une correspondance d'identifiant
  for (uint16_t i = 0; i < CANmodule->rxSize; i++) {
    buffer = &CANmodule->rxArray[i];
    if (((ident ^ buffer->ident) & buffer->mask) == 0U) {
      msgMatched = true;
      break;
    }
  }

  // Si une correspondance est trouvée et une fonction de callback est définie, l'appeler
  if (msgMatched && buffer && buffer->CANrx_callback) {
    buffer->CANrx_callback(buffer->object, &msg);
  }

  messagePending = false; // Réinitialise le flage de réception
}

// ---------------- SDO client helpers ----------------
// Fonction helper pour lire une variable via SDO sur un noeud CANopen.

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

// Fonction helper pour écrire une variable via SDO sur un noeud CANopen.

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