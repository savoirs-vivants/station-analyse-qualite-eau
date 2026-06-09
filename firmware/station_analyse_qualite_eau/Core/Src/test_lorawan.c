/**
 * @file    test_lorawan.c
 * @brief   Implémentation des tests de robustesse LoRaWAN.
 */

#include "test_lorawan.h"
#include <stdio.h>
#include "main.h"

static LoRaWAN_t *testLoRa;

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
        uint32_t _a = (uint32_t)(actual);                                   \
        uint32_t _e = (uint32_t)(expected);                                 \
        if (_a != _e) {                                                     \
            printf("  [FAIL] %s - Attendu: %lu, Recu: %lu (ligne %d)\r\n",  \
                   msg, (unsigned long)_e, (unsigned long)_a, __LINE__);    \
            return false;                                                   \
        } else {                                                            \
            printf("  [ OK ] %s\r\n", msg);                                 \
        }                                                                   \
    } while (0)


/* -------------------------------------------------------------------
 * Helper BÉTON : Attente active d'un statut spécifique (Non bloquante)
 * ------------------------------------------------------------------- */
static bool WaitLoRaStatus(LoRa_Status_t expected_status, uint32_t timeout_ms)
{
    uint32_t guard = HAL_GetTick() + timeout_ms;

    while (1)
    {
        LoRaWAN_Process(testLoRa);
        LoRa_Status_t current_status = LoRaWAN_GetStatus(testLoRa);

        /* Succès : Le module a atteint l'état souhaité */
        if (current_status == expected_status) {
            return true;
        }

        /* Échec : Le module est tombé en erreur matérielle ou réseau */
        if (current_status == LORA_STATUS_ERROR) {
            printf("  [!] Le driver a declare une erreur interne (Timeout/Plantage).\r\n");
            return false;
        }

        /* Timeout test */
        if (HAL_GetTick() > guard) {
            printf("  [TIMEOUT] Delai de %lu ms depasse !\r\n", (unsigned long)timeout_ms);
            return false;
        }
    }
}

/* ===================================================================
 * TESTS UNITAIRES
 * =================================================================== */

/** TEST 1 : Initialisation de base */
static bool Test_InitBasic(void)
{
    printf("\r\n--- TEST 1 : Init Basic ---\r\n");

    /* L'Init devrait préparer le terrain sans envoyer de commandes */
    LoRaWAN_Init(testLoRa, testLoRa->huart);

    EXPECT_EQ(LoRaWAN_GetStatus(testLoRa), LORA_STATUS_READY, "Driver READY apres init");
    return true;
}

/** TEST 2 : Configuration pure (Mode, Class, EUI, Keys) */
static bool Test_Configuration(void)
{
    printf("\r\n--- TEST 2 : Sequence Configuration AT ---\r\n");

    EXPECT_EQ(LoRaWAN_Configure(testLoRa), LORA_OK, "Demande de configuration acceptee");
    EXPECT_EQ(LoRaWAN_GetStatus(testLoRa), LORA_STATUS_BUSY, "Le driver est passe en BUSY");

    printf("  ... Envoi des cles et parametres au module ...\r\n");

    /* Attente de la fin de configuration (max 10s) */
    EXPECT(WaitLoRaStatus(LORA_STATUS_READY, 10000), "Configuration terminee avec succes");

    return true;
}

/** TEST 3 : Jonction OTAA (Test long, necessite une Gateway) */
static bool Test_JoinNetwork(void)
{
    printf("\r\n--- TEST 3 : Jonction Reseau OTAA ---\r\n");

    EXPECT_EQ(LoRaWAN_Join(testLoRa), LORA_OK, "Commande Join acceptee");

    printf("  ... Attente de l'acceptation reseau (peut prendre 15s a 30s) ...\r\n");

    /* Timeout tres long pour laisser le temps a la requete OTAA de faire l'aller-retour */
    EXPECT(WaitLoRaStatus(LORA_STATUS_READY, 35000), "Join reussi (Network Joined)");

    EXPECT(testLoRa->is_joined, "Le flag is_joined est bien a true");
    return true;
}

/** TEST 4 : Envoi de donnees (TX) */
static bool Test_SendPayload(void)
{
    printf("\r\n--- TEST 4 : Envoi Payload (Hexa) ---\r\n");

    /* On envoie un payload arbitraire ("DEADBEEF" = 4 octets) */
    EXPECT_EQ(LoRaWAN_SendHex(testLoRa, "DEADBEEF"), LORA_OK, "Commande Send acceptee");

    printf("  ... Attente fin de transmission radio ...\r\n");

    /* Le TX peut prendre du temps (Duty Cycle + Fenetres RX1/RX2) */
    EXPECT(WaitLoRaStatus(LORA_STATUS_READY, 20000), "Envoi termine avec succes (Done)");
    return true;
}

/** TEST 5 : Gestion de l'Energie (Sleep / WakeUp) */
static bool Test_LowPower(void)
{
    printf("\r\n--- TEST 5 : Low Power (Sleep & WakeUp) ---\r\n");

    /* 1. Mise en sommeil */
    EXPECT_EQ(LoRaWAN_Sleep(testLoRa), LORA_OK, "Demande de mise en sommeil");

    /* Le sommeil repond tres vite (500ms max) */
    EXPECT(WaitLoRaStatus(LORA_STATUS_SLEEP, 1000), "Module confirme son mode SLEEP");

    printf("  ... Le module dort profondement pendant 2 secondes ...\r\n");
    HAL_Delay(2000);

    /* 2. Reveil (Pichenette hardware) */
    EXPECT_EQ(LoRaWAN_WakeUp(testLoRa), LORA_OK, "Demande de reveil (Pichenette envoyee)");

    /* On attend que la machine d'etat ait fait sa tempo de 20ms et nettoyé l'UART */
    EXPECT(WaitLoRaStatus(LORA_STATUS_READY, 100), "Module reveille et pret (READY)");

    return true;
}

/* ===================================================================
 * RUNNER PRINCIPAL
 * =================================================================== */

bool TestLoRaWAN_RunAll(LoRaWAN_t *hlora)
{
    printf("\r\n========================================\r\n");
    printf("     BATTERIE DE TESTS LORAWAN\r\n");
    printf("========================================\r\n");

    if (hlora == NULL) {
        printf("  [FAIL] Pointeur LoRaWAN_t NULL\r\n");
        return false;
    }

    testLoRa = hlora;

    if (!Test_InitBasic())       return false;
    if (!Test_Configuration())   return false;

    /* Note : Si le Join echoue (pas de gateway a portee),
     * il vaut mieux arreter car le Send echouera forcement. */
    if (!Test_JoinNetwork()) {
        printf("\r\n[!] Echec du Join. Verifie tes cles OTAA et ta couverture LoRa.\r\n");
        return false;
    }

    if (!Test_SendPayload())     return false;
    if (!Test_LowPower())        return false;

    printf("\r\n========================================\r\n");
    printf("     TOUS LES TESTS LORAWAN PASSES\r\n");
    printf("========================================\r\n\r\n");
    return true;
}
