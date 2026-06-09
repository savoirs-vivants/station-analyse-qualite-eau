/**
 * @file    test_analog.c
 * @brief   Implémentation des tests de robustesse analogique.
 */

#include "test_analog.h"
#include <stdio.h>

static Analog_t *testAnalog;

/* --- Macros d'assertion --- */
#define EXPECT(cond, msg)                                           \
    do {                                                            \
        if (!(cond)) {                                              \
            printf("  [FAIL] %s (ligne %d)\r\n", msg, __LINE__);    \
            return false;                                           \
        } else {                                                    \
            printf("  [ OK ] %s\r\n", msg);                         \
        }                                                           \
    } while (0)

#define EXPECT_EQ(actual, expected, msg)                                    \
    do {                                                                    \
        if ((actual) != (expected)) {                                       \
            printf("  [FAIL] %s - Recu: %d, Attendu: %d (ligne %d)\r\n",    \
                   msg, (int)actual, (int)expected, __LINE__);              \
            return false;                                                   \
        } else {                                                            \
            printf("  [ OK ] %s\r\n", msg);                                 \
        }                                                                   \
    } while (0)

/* --- Helper d'attente --- */
static bool WaitAnalogDone(uint32_t timeout_ms) {
    uint32_t start = HAL_GetTick();
    while (Analog_GetStatus(testAnalog) == ANALOG_STATUS_BUSY) {
        Analog_Process(testAnalog);
        if ((HAL_GetTick() - start) > timeout_ms) return false;
    }
    return (Analog_GetStatus(testAnalog) == ANALOG_STATUS_READY);
}

/* ===================================================================
 * TESTS UNITAIRES
 * =================================================================== */

/** TEST 1 : Paramètres et Init */
static bool Test_Analog_Params(void) {
    printf("\r\n--- TEST 1 : Parametres Invalides ---\r\n");
    EXPECT_EQ(Analog_Init(NULL, NULL), ANALOG_ERR_PARAM, "Rejet pointeur NULL");
    EXPECT_EQ(Analog_Start(NULL), ANALOG_ERR_PARAM, "Rejet Start NULL");
    return true;
}

/** TEST 2 : Séquence de conversion complète */
static bool Test_Analog_Conversion(void) {
    printf("\r\n--- TEST 2 : Cycle de Conversion ---\r\n");

    EXPECT_EQ(Analog_Start(testAnalog), ANALOG_OK, "Lancement conversion");
    EXPECT_EQ(Analog_GetStatus(testAnalog), ANALOG_STATUS_BUSY, "Statut est BUSY");

    EXPECT(WaitAnalogDone(200), "Fin de conversion DMA");

    /* Vérification de la cohérence des données système */
    EXPECT(testAnalog->true_vdda_mv > 2000 && testAnalog->true_vdda_mv < 3600, "VDDA coherent (2V-3.6V)");
    //EXPECT(testAnalog->internal_temp > -20 && testAnalog->internal_temp < 80, "Temp interne MCU coherente");

    printf("  [ INFO ] VDDA : %lu mV | VBAT : %lu mV\r\n",
            testAnalog->true_vdda_mv, testAnalog->v_battery);

    return true;
}

/** TEST 3 : Protection Busy (Anti-collision) */
static bool Test_Analog_Busy(void) {
    printf("\r\n--- TEST 3 : Protection Busy ---\r\n");

    Analog_Start(testAnalog);
    EXPECT_EQ(Analog_Start(testAnalog), ANALOG_ERR_BUSY, "Refus d'un double Start");

    WaitAnalogDone(100);
    return true;
}

/** TEST 4 : Vérification du Timeout */
static bool Test_Analog_Timeout(void) {
    printf("\r\n--- TEST 4 : Verification Timeout ---\r\n");

    /* Simulation d'un blocage : on passe en WAIT sans lancer le DMA */
    testAnalog->state = ANALOG_WAIT_CONVERSION;
    testAnalog->event_adc_complete = false;
    testAnalog->start_time = HAL_GetTick() - 100; // Force un vieux timestamp

    Analog_Process(testAnalog); // Devrait détecter le timeout

    EXPECT_EQ(Analog_GetStatus(testAnalog), ANALOG_STATUS_ERROR, "Erreur detectee apres timeout");
    EXPECT_EQ(testAnalog->error_code, 2, "Code erreur 2 (Timeout)");

    /* Reset pour les prochains tests */
    testAnalog->status = ANALOG_STATUS_READY;
    testAnalog->state = ANALOG_IDLE;

    return true;
}

/* ===================================================================
 * RUNNER
 * =================================================================== */

bool TestAnalog_RunAll(Analog_t *hanalog) {
    printf("\r\n========================================\r\n");
    printf("     BATTERIE DE TESTS ANALOGIQUE\r\n");
    printf("========================================\r\n");

    testAnalog = hanalog;

    if (!Test_Analog_Params())     return false;
    if (!Test_Analog_Conversion()) return false;
    if (!Test_Analog_Busy())       return false;
    if (!Test_Analog_Timeout())    return false;

    printf("\r\n========================================\r\n");
    printf("     TOUS LES TESTS ANALOG PASSES\r\n");
    printf("========================================\r\n\r\n");
    return true;
}
