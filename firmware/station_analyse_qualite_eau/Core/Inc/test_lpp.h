/**
 * @file    test_lpp.h
 * @brief   Batterie de tests unitaires pour l'encodeur LoRaWAN Cayenne LPP.
 */

#ifndef INC_TEST_LPP_H_
#define INC_TEST_LPP_H_

#include "driverLPPCayenne.h"

/**
 * @brief  Lance tous les tests de la couche Cayenne LPP.
 * @return true si tous les tests passent, false si une erreur est détectée.
 */
bool TestLPP_RunAll(void);

#endif /* INC_TEST_LPP_H_ */
