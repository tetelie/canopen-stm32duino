#define CO_MULTIPLE_OD

#include "CANopen.h"

#include "OD.h"

#include "hal_conf_extra.h"
#include <STM32_CAN.h>

#define DEBUG 1



CO_NMT_control_t nmt_control = CO_NMT_STARTUP_TO_OPERATIONAL;


STM32_CAN Can1(CAN1, DEF);  // Broches PA11/PA12 pour CAN1
CO_t *CO = NULL;            // Objet CANopen

CAN_message_t latestMsg;
volatile bool newMessage = false;
bool messagePending = false;
CAN_message_t msg;

HardwareTimer timer(TIM4);  // TIM1 --> TIM4 https://github.com/stm32duino/Arduino_Core_STM32/wiki/HardwareTimer-library
volatile bool canopen_1ms_tick = false;


/*
  debug function
  description: print with or without jump 'message' if DEBUG Enabled
    - message : char *
    - ln : bool (default true) // jump line
  return type : void
*/
void debug(bool ln = true) {
  if (DEBUG) {
    if (ln) Serial.println();
  }
}

// Pour les types numériques avec base
template<typename T>
typename std::enable_if<std::is_integral<T>::value, void>::type
debug(T message, int format, bool ln = true) {
  if (ln) {
    Serial.println(message, format);
  } else {
    Serial.print(message, format);
  }
}

// Pour les types génériques
template<typename T>
void debug(T message, bool ln = true) {
  if (ln) {
    Serial.println(message);
  } else {
    Serial.print(message);
  }
}

// === Callback appelée à la réception d’un message CANopen configuré ===
void myRxCallback(void *object, void *msgPtr) {
  CAN_message_t *msg = (CAN_message_t *)msgPtr;
  latestMsg = *msg;
  newMessage = true;
}

// === enregistrement du timer hardware
void setup_hardware_timer() {
  timer.setOverflow(1000, MICROSEC_FORMAT);  // 1ms
  timer.attachInterrupt([]() {
    canopen_1ms_tick = true;
  });
  timer.resume();
}

void setup() {
  delay(3000); // attente nécessaire pour afficher les premiers messages
  Serial.begin(115200); // Moniteur série

  analogReadResolution(12);

  /* chargement du dictionnaire d'objets */
  CO_config_t* config_ptr = NULL;
  // --- Initialisation CANopenNode ---
  CO_config_t config={0};
  OD_INIT_CONFIG(config);

  config.CNT_LSS_SLV = 1;
  config.CNT_RPDO = 1;
  config_ptr = &config;
  /* fin dictionnaire objets */

  debug("début initialisation");
  /* allocation objets canopen */
  CO = CO_new(config_ptr, NULL);
  if (CO == NULL) {
    debug("Erreur d'allocation CO");
    while (1)
      ;
  }
  /* fin allocation objets canopen */

  CO->CANmodule->CANnormal = false;

  CO_CANsetConfigurationMode((void *)&Can1); // rattachement du bus CAN à canopen
  CO_CANmodule_disable(CO->CANmodule);       // désactivation du module CAN

  debug("après co_news");

  uint32_t errInfo = 0;
  
  uint16_t bitrate = 500000;  // en kbps → 500 kbps


  // Init module CANopen avec STM32_CAN
  CO_ReturnError_t err = CO_CANinit(CO, (void *)&Can1, 500 /*bitrate*/);
  if (err != CO_ERROR_NO) {
    debug("Erreur d'init CO_CANinit");
    while (1)
      ;
  }

  debug("après init");
  delay(1000);


  CO_LSS_address_t lssAddress = { .identity = { .vendorID = OD_PERSIST_COMM.x1018_identity.vendor_ID,
                                                .productCode = OD_PERSIST_COMM.x1018_identity.productCode,
                                                .revisionNumber = OD_PERSIST_COMM.x1018_identity.revisionNumber,
                                                .serialNumber = OD_PERSIST_COMM.x1018_identity.serialNumber } };

  uint8_t nodeId = 0x02;   // identifiant noeud désiré

  err = CO_LSSinit(CO, &lssAddress, &nodeId, &bitrate);
  if (err != CO_ERROR_NO) {
    debug("Error: LSS slave initialization failed");
    while (1)
      ;
  }

  debug("après LSSinit");
  delay(2000);

  // Init objets CANopen (PDO, SDO, NMT, etc.)itRa
  err = CO_CANopenInit(
    CO,           // pointeur CO principal
    NULL,         // NULL ici car OD_CNT_NMT == 1
    NULL,         // NULL ici car OD_CNT_EM == 1 et co->em sera utilisé
    OD,           // OD global (déclaré dans OD.c)
    NULL,         // NULL ici si tu n’utilises pas de bits de statut spécifiques
    nmt_control,  // mode de contrôle, ex. : CO_NMT_STARTUP_TO_OPERATIONAL | CO_NMT_MANUAL_OPERATIONAL
    1000,         // firstHBTime_ms (fréquence du heartbeat)
    1000,         // SDO server timeout (en ms)
    500,          // SDO client timeout (en ms)
    false,        // SDO client block transfer désactivé
    nodeId,         // nodeId
    &errInfo      // erreur détaillée en cas d’échec
  );
  if (err != CO_ERROR_NO) {
    debug("Erreur d'init CO_CANopenInit");
    while (1)
      ;
  }

  debug("après Open_init");
  delay(2000);

  //sendNMTstartNode(CO->CANmodule, nodeId); test

  delay(2000);

  /* PDO start */
  err = CO_CANopenInitPDO(CO, CO->em, OD, 0x02, &errInfo);

  if (err != CO_ERROR_NO) {
    if (err == CO_ERROR_OD_PARAMETERS) {
      debug("Erreur : OD invalide, entrée : ");
    } else {
      debug("Erreur CO_CANopenInit");
    }
    while (1)
      ;
  }
  /* PDO end */
  debug("après InitPDO");
  delay(2000);

  // Enregistrement du callback de recetpion
  //CO_CANrxBufferInit(CO->CANmodule, 0, 0x000, 0x000, false, NULL, myRxCallback);
  debug("après rxBufferInit");
  delay(1000);



  // Passage en mode normal CAN
  CO->CANmodule->CANnormal = true;
  CO_CANsetNormalMode(CO->CANmodule);


  //Can1.begin();
  //Can1.setBaudRate(500000);

  delay(2000);


  if (CO->nodeIdUnconfigured) {
    debug("Erreur : Node ID non initialisé !");
  } else {
    debug("CANopenNode - En fonctionnement !");
  }

  debug("CANopenNode prêt");

  // initialisation du timer hardware
  setup_hardware_timer();
}

void loop() {
  static uint32_t lastProcessTime = 0;

  int value = analogRead(PA0);
  Serial.println(value);

  if (canopen_1ms_tick) { // ce flag est mis à vrai à chaque iteration du timer hardware
    canopen_1ms_tick = false; // on met le flag directement à faux

    uint32_t now = millis();
    uint32_t diff = now - lastProcessTime;
    lastProcessTime = now;

    uint32_t timerNext_us = 0;
    CO_NMT_reset_cmd_t reset;


    OD_RAM.x2110_newObject[0] = value;


    // Appel de toute les fonctions process
    reset = CO_process(CO, false, diff * 1000, &timerNext_us);
    CO_process_RPDO(CO, false, diff * 1000, &timerNext_us);
    CO_process_TPDO(CO, false, diff * 1000, &timerNext_us);


    if (reset != CO_RESET_NOT) {
      // Implémenter un redémarrage logiciel ici si besoin
    }



    // Traitement réception CAN brute (optionnel)
    if (Can1.read(msg)) {
      messagePending = true;
    }
    CO_CANinterruptRx(CO->CANmodule);  // OK ici message reçu

    if (newMessage) {
      newMessage = false;
      debug("Reçu ID: 0x", false);
      debug(latestMsg.id, HEX, false);
      debug(" DLC: ", false);
      debug(latestMsg.len, false);
      debug(" Data: ", false);
      for (uint8_t i = 0; i < latestMsg.len; i++) {
        debug(latestMsg.buf[i], HEX, false);
        debug();
      }
      debug();
    }
  }
}
