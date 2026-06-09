/*
 * test_fram.h
 * Batterie de tests pour le driver FRAM SPI/DMA
 */

#ifndef INC_TEST_FRAM_H_
#define INC_TEST_FRAM_H_

#include <stdbool.h>
#include "driverFRAM.h"

/**
 * @brief  Lance toute la batterie de tests sur la FRAM.
 * @param  hfram Pointeur vers le handle FRAM initialisé.
 * @return true si tous les tests passent, false sinon.
 */
bool TestFRAM_RunAll(FRAM_t *hfram);

#endif /* INC_TEST_FRAM_H_ */
