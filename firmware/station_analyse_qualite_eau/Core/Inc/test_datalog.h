#ifndef INC_TEST_DATALOG_H_
#define INC_TEST_DATALOG_H_

#include <stdbool.h>
#include "driverFRAM.h"

/**
 * @brief Lance la batterie de tests DataLog.
 * @param hfram Handle FRAM DEJA INITIALISE par l'application.
 *              Les callbacks HAL SPI doivent router vers ce handle.
 * @return true si tous les tests passent.
 */
bool TestDataLog_RunAll(FRAM_t *hfram);

#endif
