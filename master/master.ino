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
volatile int canopen_5000ms_tick = false;

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

void print_delay(int ms, int point=10, char * c="⏳")
{
  int part = (int) ms / 10;
  for(int i = 0; i < point; i++)
  {
    Serial.print(c);
    delay(part);
  }
  Serial.println("");
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

// === enregistrement du timer hardware
void setup_hardware_timer() {
  timer.setOverflow(1000, MICROSEC_FORMAT);  // 1ms
  timer.attachInterrupt([]() {
    canopen_1ms_tick = true;
    canopen_5000ms_tick++;
    //Serial.println("tick");
  });
  timer.resume();
}

void setup() {
  print_delay(3000); // attente nécessaire pour afficher les premiers messages
  Serial.begin(115200); // Moniteur série

  /*--------------------------------------------
      Variables
    --------------------------------------------*/
  uint32_t errInfo = 0;
  uint16_t bitrate = 500000;  // en kbps → 500 kbps
  uint8_t nodeId = 0x03;   // identifiant noeud désiré

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

  /*--------------------------------------------
      Création des objets CANopen
    --------------------------------------------*/
  /* allocation objets canopen */
  CO = CO_new(config_ptr, NULL);
  if (CO == NULL) {
    debug("Erreur d'allocation CO");
    while (1)
      ;
  }
  /* fin allocation objets canopen */


  /*--------------------------------------------
      Mise en configuration du module CAN
    --------------------------------------------*/
  CO->CANmodule->CANnormal = false;
  CO_CANsetConfigurationMode((void *)&Can1); // rattachement du bus CAN à canopen
  CO_CANmodule_disable(CO->CANmodule);       // désactivation du module CAN

  debug("après co_news");




  /*--------------------------------------------
      Initialisation du module CAN
    --------------------------------------------*/
  // Init module CANopen avec STM32_CAN
  CO_ReturnError_t err = CO_CANinit(CO, (void *)&Can1, 500 /*bitrate*/);
  if (err != CO_ERROR_NO) {
    debug("Erreur d'init CO_CANinit");
    while (1)
      ;
  }

  debug("après init");
  print_delay(1000);




  /*--------------------------------------------
      Initialisation de LSS
    --------------------------------------------*/
  CO_LSS_address_t lssAddress = { .identity = { .vendorID = OD_PERSIST_COMM.x1018_identity.vendor_ID,
                                                .productCode = OD_PERSIST_COMM.x1018_identity.productCode,
                                                .revisionNumber = OD_PERSIST_COMM.x1018_identity.revisionNumber,
                                                .serialNumber = OD_PERSIST_COMM.x1018_identity.serialNumber } };


  err = CO_LSSinit(CO, &lssAddress, &nodeId, &bitrate);
  if (err != CO_ERROR_NO) {
    debug("Error: LSS slave initialization failed");
    while (1)
      ;
  }

  debug("après LSSinit");
  print_delay(2000);

  /*--------------------------------------------
      Initialisation du protocole CANopen principal
    --------------------------------------------*/
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
  print_delay(2000);

  //sendNMTstartNode(CO->CANmodule, nodeId); test

  print_delay(2000);

  /*--------------------------------------------
      Initialisation des PDOs
    --------------------------------------------*/
  /* PDO start */
  err = CO_CANopenInitPDO(CO, CO->em, OD, nodeId, &errInfo);

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
  print_delay(2000);



  /*--------------------------------------------
      Initialisation du Timer Hardware (1ms)
    --------------------------------------------*/
  // initialisation du timer hardware
  setup_hardware_timer();


  /*--------------------------------------------
      Activation du mode CAN "normal"
    --------------------------------------------*/
  // Passage en mode normal CAN
  CO->CANmodule->CANnormal = true;
  CO_CANsetNormalMode(CO->CANmodule);


  //Can1.begin();
  //Can1.setBaudRate(500000);

  print_delay(2000);


  if (CO->nodeIdUnconfigured) {
    debug("Erreur : Node ID non initialisé !");
  } else {
    debug("CANopenNode - En fonctionnement !");
  }
  /*err = CO_CANrxBufferInit(CO->CANmodule, 33, 0x180, 0x7FF, false, NULL, myRxCallback);
  if (err != CO_ERROR_NO) {
      Serial.printf("Erreur initialisation buffer CAN RX : %d\n", err);
  }*/
  debug("CANopenNode prêt");

  print_delay(2000);

  //Can1.setCallback(CAN1_RX0_IRQHandler);

  print_delay(5000);
}

void CAN1_RX0_IRQHandler(void){
  CO_CANinterruptRx(CO->CANmodule);
}

void loop() {
  static uint32_t lastTimeMain = 0;
  uint32_t now = millis();
  uint32_t diffMain = now - lastTimeMain;

  /*--------------------------------------------
      Début Boucle Principal (5ms)
    --------------------------------------------*/
  // -- Boucle principale, exécution toutes les ~5ms --
  if (diffMain > 5) {
    lastTimeMain = now;

    uint32_t timerNext_us = 0;
    CO_NMT_reset_cmd_t reset = CO_process(CO, false, diffMain * 1000, &timerNext_us);

    // Traitement réception CAN brute (optionnel)
    if (Can1.read(msg)) {
      CO_CANinterruptRx(CO->CANmodule);  // OK ici message reçu
      messagePending = true;
      Serial.println("Réception CAN brute :");
    }

    if (reset != CO_RESET_NOT) {
      Serial.println("RESET demandé !");
      // Implémenter un redémarrage ou une réinit si besoin
    }
  }
  /*--------------------------------------------
      Fin Boucle Principal
    --------------------------------------------*/

  /*--------------------------------------------
      Début Boucle temps réel (1ms)
    --------------------------------------------*/
  // -- Traitement cyclique toutes les 1ms via le flag timer --
  if (canopen_1ms_tick) {
    canopen_1ms_tick = false;

    static uint32_t lastProcessTime = 0;
    uint32_t now = millis();
    uint32_t diff = now - lastProcessTime;
    lastProcessTime = now;

    uint32_t timerNext_us = 0;

    // Appels cycliques CANopenNode
    bool_t syncWas = CO_process_SYNC(CO, diff * 1000, &timerNext_us);
    CO_process_RPDO(CO, syncWas, diff * 1000, &timerNext_us);
    CO_process_TPDO(CO, syncWas, diff * 1000, &timerNext_us);

    // Ton code applicatif non-bloquant
    if (newMessage) {
      newMessage = false;
      Serial.println("Nouveau message reçu.");
    }
  }
  /*--------------------------------------------
      Fin Boucle temps réel
    --------------------------------------------*/


  /*--------------------------------------------
      Vérification tu timer hardware
    --------------------------------------------*/
  // -- Vérification du timer 5000ms --
  if (canopen_5000ms_tick == 5000) {
    canopen_5000ms_tick = 0;
    Serial.println("Hardware Timer Alive ! (5000ms)");
  }
}
