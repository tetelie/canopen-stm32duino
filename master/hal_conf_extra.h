#ifndef HAL_CONFIG_H
#define HAL_CONFIG_H            // Evite l'importation multiple

#define USE_HAL_DRIVER          // Active l'utilisation de la HAL
#define HAL_CAN_MODULE_ENABLED  // Active le module CAN


/*
  Inclut les HAL spécifiques de la famille de carte stm32f1xx (ici bluepill)
*/
#include "stm32f1xx_hal.h"      // Inclut le fichier principal de la HAL
#include "stm32f1xx_hal_can.h"  // Inclut le fichier spécifique au module CAN de la HAL

#endif
