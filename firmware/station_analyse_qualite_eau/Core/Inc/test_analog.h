/**
 * @file    test_analog.h
 * @brief   Batterie de tests unitaires pour le driver Analogique.
 */

#ifndef INC_TEST_ANALOG_H_
#define INC_TEST_ANALOG_H_

#include "driverAnalog.h"

/**
 * @brief  Lance tous les tests de la couche analogique.
 * @param  hanalog Pointeur vers le handle analogique de l'application.
 * @return true si succès, false si une erreur critique est détectée.
 */
bool TestAnalog_RunAll(Analog_t *hanalog);

#endif /* INC_TEST_ANALOG_H_ */
