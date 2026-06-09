/**
 * @file    test_ble.h
 * @brief   Batterie de tests unitaires pour le driver BLE.
 */

#ifndef INC_TEST_BLE_H_
#define INC_TEST_BLE_H_

#include "driverBLE.h"
#include <stdbool.h>

/**
 * @brief  Lance tous les tests de la couche BLE.
 * @param  hble Pointeur vers le handle applicatif.
 * @return true si tous les tests passent, false si une erreur est détectée.
 */
bool TestBLE_RunAll(BLE_t *hble);

#endif /* INC_TEST_BLE_H_ */
