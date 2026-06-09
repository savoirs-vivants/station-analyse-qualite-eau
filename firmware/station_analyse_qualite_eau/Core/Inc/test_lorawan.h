/**
 * @file    test_lorawan.h
 * @brief   Batterie de tests unitaires pour le driver LoRaWAN.
 */

#ifndef INC_TEST_LORAWAN_H_
#define INC_TEST_LORAWAN_H_

#include "driverLoRaWAN.h"
#include <stdbool.h>

/**
 * @brief  Lance tous les tests de la couche LoRaWAN.
 * @param  hlora Pointeur vers le handle applicatif.
 * @return true si tous les tests passent, false si une erreur est détectée.
 */
bool TestLoRaWAN_RunAll(LoRaWAN_t *hlora);

#endif /* INC_TEST_LORAWAN_H_ */
