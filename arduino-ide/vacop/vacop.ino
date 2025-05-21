#define CO_MULTIPLE_OD
#include "CANopen.h"

#include "OD.h"

#include "hal_conf_extra.h"
#include <STM32_CAN.h>

#define DEBUG 1



CO_NMT_control_t nmt_control;    // il semblerait y avoir un problème à verifier lors des tests


STM32_CAN Can1(CAN1, DEF); // Broches PA11/PA12 pour CAN1
CO_t *CO = NULL;           // Objet CANopen

CAN_message_t latestMsg;
volatile bool newMessage = false;
bool messagePending = false;
CAN_message_t msg;


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
template <typename T>
typename std::enable_if<std::is_integral<T>::value, void>::type
debug(T message, int format, bool ln = true) {
    if(DEBUG){
        if (ln) {
            Serial.println(message, format);
        } else {
            Serial.print(message, format);
        }
    }
}

// Pour les types génériques
template <typename T>
void debug(T message, bool ln = true) {
    if(DEBUG){
        if (ln) {
            Serial.println(message);
        } else {
            Serial.print(message);
        }
    }
}





// === Callback appelée à la réception d’un message CANopen configuré ===
void myRxCallback(void *object, void *msgPtr) {
    CAN_message_t *msg = (CAN_message_t *)msgPtr;
    latestMsg = *msg;
    newMessage = true;
}

void setup() {
    delay(5000);
    Serial.begin(115200);

    // --- Initialisation CANopenNode ---
    CO_config_t config;
    OD_INIT_CONFIG(config);

    debug("début initialisation");
    CO = CO_new(&config, NULL);
    if (CO == NULL) {
        debug("Erreur d'allocation CO");
        while (1);
    }

    CO_CANmodule_disable(CO->CANmodule);

    debug("après co_news");

    uint32_t errInfo = 0;

    // Init module CANopen avec STM32_CAN
    CO_ReturnError_t err = CO_CANinit(CO, (void*)&Can1, 500000 /*bitrate*/);
    if (err != CO_ERROR_NO) {
        debug("Erreur d'init CO_CANinit");
        while (1);
    }

    debug("après init");
    delay(5000);

    // Init objets CANopen (PDO, SDO, NMT, etc.)itRa
    err = CO_CANopenInit(
        CO,                    // pointeur CO principal
        NULL,                   // NULL ici car OD_CNT_NMT == 1
        NULL,                    // NULL ici car OD_CNT_EM == 1 et co->em sera utilisé
        OD,                    // OD global (déclaré dans OD.c)
        NULL,         // NULL ici si tu n’utilises pas de bits de statut spécifiques
        nmt_control,        // mode de contrôle, ex. : CO_NMT_STARTUP_TO_OPERATIONAL | CO_NMT_MANUAL_OPERATIONAL
        1000,                  // firstHBTime_ms (fréquence du heartbeat)
        1000,                  // SDO server timeout (en ms)
        500,                   // SDO client timeout (en ms)
        false,                 // SDO client block transfer désactivé
        0x01,                  // nodeId
        &errInfo               // erreur détaillée en cas d’échec
    );
    if (err != CO_ERROR_NO) {
        debug("Erreur d'init CO_CANopenInit");
        while (1);
    }

    debug("après Open_init");
    delay(5000);



    err = CO_CANopenInitPDO(CO, CO->em, OD, 0x01, &errInfo);

    debug("après InitPDO");
    delay(5000);


    // Exemple d’enregistrement d’un callback sur l’ID 0x180 (TPDO1 d’un autre noeud)
    CO_CANrxBufferInit(CO->CANmodule, 0, 0x180, 0x7FF, false, NULL, myRxCallback);

    debug("après rxBufferInit");
    delay(5000);



    debug("CANopenNode prêt");
}

void loop() {
    //debug("paul est beau");
    // Tick CANopenNode
    uint32_t now = millis();
    CO_process(CO,true, now, NULL);


    if (Can1.read(msg)) {
        messagePending = true; // ← là on injecte dans CANopen
    }
    CO_CANinterruptRx(CO->CANmodule);

    // Affiche les messages reçus par le callback
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

    delay(1);
}
