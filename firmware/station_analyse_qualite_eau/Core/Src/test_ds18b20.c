/*
 * test_ds18b20.c
 *
 *  Created on: 4 mai 2026
 *      Author: valen
 */

#include <stdio.h>
#include "test_ds18b20.h"
#include "driverDS18B20.h"
#include "main.h" // Pour hlpuart2 et HAL_GetTick()
#include "stm32u0xx.h"
#include "usart.h"

/** Macro d'assertion simple. */
#define EXPECT(cond, msg)                                           \
    do {                                                            \
        if (!(cond)) {                                              \
            printf("  [FAIL] %s (ligne %d)\r\n", msg, __LINE__);    \
            return false;                                           \
        } else {                                                    \
            printf("  [ OK ] %s\r\n", msg);                         \
        }                                                           \
    } while (0)

/* -------------------------------------------------------------------
 * Outils de test
 * ------------------------------------------------------------------- */

/**
 * @brief Attend que le capteur ait fini son travail tout en faisant tourner la FSM.
 * @return true si le capteur est prêt, false si timeout (plantage).
 */
static bool WaitIdle(DS18B20_t *hds)
{
    uint32_t guard = HAL_GetTick() + 2000U; // 2 secondes max (une conversion prend ~750ms)

    // On boucle tant que le capteur n'est pas READY (et qu'il n'est pas en ERROR)
    while (DS18B20_GetStatus(hds) == DS_STATUS_BUSY) {
        DS18B20_Process(hds);

        if (HAL_GetTick() > guard) {
            printf("  [TIMEOUT] La FSM est restee bloquee !\r\n");
            return false;
        }
    }
    return true;
}

/* -------------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------------- */

/** TEST 1 : Init */
static bool Test_InitBasic(DS18B20_t *hds)
{
    printf("\r\n--- TEST 1 : Initialisation ---\r\n");

    DS18B20_Result_t res = DS18B20_Init(hds, &hlpuart2);
    EXPECT(res == DS_OK, "Retour Init = OK");
    EXPECT(DS18B20_GetStatus(hds) == DS_STATUS_READY, "Le driver est pret");

    return true;
}

/** TEST 2 : Lecture complète de la température */
static bool Test_ReadTemperature(DS18B20_t *hds)
{
    printf("\r\n--- TEST 2 : Start + Read ---\r\n");

    DS18B20_Result_t res = DS18B20_Start(hds);
    EXPECT(res == DS_OK, "Demarrage de la mesure");

    // On laisse la machine d'etat faire son travail (Reset, Skip, Convert, Read...)
    EXPECT(WaitIdle(hds) == true, "Attente fin de mesure sans timeout");

    // Vérification qu'on n'a pas fini en état d'erreur (ex: fil débranché)
    EXPECT(DS18B20_GetStatus(hds) != DS_STATUS_ERROR, "Pas d'erreur hardware");

    // Récupération de la donnée
    int16_t raw_temp = DS18B20_GetTemperature(hds);

    // Astuce d'affichage Virgule Fixe
    uint16_t abs_raw = (raw_temp < 0) ? -raw_temp : raw_temp;
    printf("  -> Valeur lue : %c%d.%02d C\r\n",
           (raw_temp < 0) ? '-' : ' ', abs_raw / 16, ((abs_raw % 16) * 100) / 16);

    // On s'attend à une température de bureau réaliste (entre 10°C et 40°C)
    // En Q11.4, cela correspond à (10 * 16) = 160 et (40 * 16) = 640
    EXPECT((raw_temp > 160) && (raw_temp < 640), "La temperature est physiquement coherente");

    return true;
}

/* -------------------------------------------------------------------
 * Runner principal
 * ------------------------------------------------------------------- */

bool TestDS18B20_RunAll(DS18B20_t *hds)
{
    printf("\r\n========================================\r\n");
    printf("     BATTERIE DE TESTS DS18B20\r\n");
    printf("========================================\r\n");

    if (!Test_InitBasic(hds))       { return false; }
    if (!Test_ReadTemperature(hds)) { return false; }

    printf("\r\n========================================\r\n");
    printf("     TOUS LES TESTS PASSES\r\n");
    printf("========================================\r\n\r\n");
    return true;
}
