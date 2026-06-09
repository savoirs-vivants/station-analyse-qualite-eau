/**
 * @file    test_ble.c
 * @brief   Implémentation des tests de robustesse BLE.
 */

#include "test_ble.h"
#include <stdio.h>
#include <string.h>
#include "main.h"

static BLE_t *testBLE;

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
 * Helper : Fait tourner la FSM jusqu'à un statut donné (avec timeout)
 * ------------------------------------------------------------------- */
static bool WaitBLEStatus(BLE_Status_t expected_status, uint32_t timeout_ms)
{
    uint32_t guard = HAL_GetTick() + timeout_ms;

    while (1)
    {
        BLE_Process(testBLE);
        BLE_Status_t current_status = BLE_GetStatus(testBLE);

        if (current_status == expected_status) return true;

        if (current_status == BLE_STATUS_ERROR) {
            printf("  [!] Le driver BLE a declare une erreur interne.\r\n");
            return false;
        }

        if (HAL_GetTick() > guard) {
            printf("  [TIMEOUT] Delai de %lu ms depasse !\r\n", (unsigned long)timeout_ms);
            return false;
        }
    }
}

/* ===================================================================
 * TESTS UNITAIRES
 * =================================================================== */

/** TEST 0 : Test Materiel Brut (Bloquant, sans FSM) */
static bool Test_BLE_Hardware_Raw(void)
{
    printf("\r\n--- TEST 0 : Ping Materiel Brut (Sans FSM) ---\r\n");

    /* 1. Allumage forcé du module (au cas où ce ne soit pas fait) */
    //HAL_GPIO_WritePin(EN_LS_BLE_GPIO_Port, EN_LS_BLE_Pin, SET);

    /* On laisse 1 seconde au processeur du BLE pour démarrer */
    HAL_Delay(1000);

    /* Nettoyage de l'UART */
    HAL_UART_AbortReceive(testBLE->huart);
    __HAL_UART_CLEAR_OREFLAG(testBLE->huart);

    /* 2. Envoi de la commande d'entrée ($$$ SANS retour chariot) */
    char rx_buffer[32] = {0};
    char *cmd_enter = "$$$";
    printf("  ... Envoi de '%s' en mode bloquant ...\r\n", cmd_enter);
    HAL_UART_Transmit(testBLE->huart, (uint8_t*)cmd_enter, strlen(cmd_enter), 100);

    /* 3. Attente bloquante de la réponse (1 seconde max) */
    HAL_UART_Receive(testBLE->huart, (uint8_t*)rx_buffer, 10, 1000);
    rx_buffer[sizeof(rx_buffer)-1] = '\0'; /* Sécurité chaîne */

    printf("  -> Reponse de l'UART : '%s'\r\n", rx_buffer);

    /* 4. Vérification */
    EXPECT(strstr(rx_buffer, "CMD") != NULL, "Le module comprend le $$$ et repond CMD");

    /* 5. Sortie propre du mode commande pour ne pas perturber la suite des tests */
    char *cmd_exit = "---\r";
    HAL_UART_Transmit(testBLE->huart, (uint8_t*)cmd_exit, strlen(cmd_exit), 100);
    HAL_Delay(200); /* Laisse le temps de sortir du mode CMD */

    return true;
}

/** TEST 1 : Initialisation et Protections */
static bool Test_BLE_InitAndParam(void)
{
    printf("\r\n--- TEST 1 : Init et Protections ---\r\n");

    /* Test pointeurs NULL */
    EXPECT_EQ(BLE_Configure(NULL), BLE_ERR_PARAM, "Rejet Configure(NULL)");
    EXPECT_EQ(BLE_Start(NULL), BLE_ERR_PARAM, "Rejet Start(NULL)");
    EXPECT_EQ(BLE_SendChunk(NULL, "Test"), BLE_ERR_PARAM, "Rejet SendChunk(NULL)");

    /* Init normale */
    BLE_Init(testBLE, testBLE->huart);
    EXPECT_EQ(BLE_GetStatus(testBLE), BLE_STATUS_READY, "Driver READY apres init");

    return true;
}

/** TEST 2 : Configuration AT du Module */
static bool Test_BLE_Configuration(void)
{
    printf("\r\n--- TEST 2 : Sequence Configuration AT ---\r\n");

    EXPECT_EQ(BLE_Configure(testBLE), BLE_OK, "Demande de configuration acceptee");
    EXPECT_EQ(BLE_GetStatus(testBLE), BLE_STATUS_CONFIGURING, "Le driver est passe en CONFIGURING");

    /* Le module prend un peu de temps à s'allumer et répondre aux commandes AT */
    EXPECT(WaitBLEStatus(BLE_STATUS_READY, 5000), "Configuration terminee avec succes");

    return true;
}

/** TEST 3 : Simulation d'une connexion Téléphone (Le plus important !) */
static bool Test_BLE_PhoneSimulation(void)
{
    printf("\r\n--- TEST 3 : Simulation Connexion Telephone ---\r\n");

    /* 1. On lance l'écoute */
    EXPECT_EQ(BLE_Start(testBLE), BLE_OK, "Demande de demarrage (ecoute) acceptee");

    /* On force quelques tours de Process pour passer le BLE_BOOT_TIME (500ms) */
    uint32_t start = HAL_GetTick();
    while(HAL_GetTick() - start < 600) { BLE_Process(testBLE); }

    /* 2. SIMULATION : Le téléphone envoie '?' pour demander les données */
    printf("  ... Simulation : Le telephone envoie '?' ...\r\n");
    testBLE->rx_buffer[0] = '?';
    testBLE->rx_buffer[1] = '\0';
    testBLE->rx_flag = true; /* On fait croire à l'UART qu'il a reçu quelque chose */

    /* La machine d'état devrait traiter le '?' et passer en STREAMING */
    BLE_Process(testBLE);
    EXPECT_EQ(BLE_GetStatus(testBLE), BLE_STATUS_STREAMING, "Passage en STREAMING reussi");

    /* 3. Envoi d'un Chunk de données */
    EXPECT_EQ(BLE_SendChunk(testBLE, "1,123456,250,3300,1500,00\r\n"), BLE_OK, "Envoi Chunk 1 OK");

    /* 4. Clôture du flux par l'application */
    EXPECT_EQ(BLE_EndStream(testBLE), BLE_OK, "Demande de fin de flux (EndStream)");

    /* 5. SIMULATION : Le téléphone confirme avec 'Q' (Quit) */
    printf("  ... Simulation : Le telephone envoie 'Q' ...\r\n");
    testBLE->rx_buffer[0] = 'Q';
    testBLE->rx_buffer[1] = '\0';
    testBLE->rx_flag = true;

    BLE_Process(testBLE);
    BLE_Process(testBLE);
    EXPECT_EQ(BLE_GetStatus(testBLE), BLE_STATUS_READY, "Retour en READY apres acquittement 'Q'");

    return true;
}

/* ===================================================================
 * RUNNER PRINCIPAL
 * =================================================================== */

bool TestBLE_RunAll(BLE_t *hble)
{
    printf("\r\n========================================\r\n");
    printf("     BATTERIE DE TESTS BLE\r\n");
    printf("========================================\r\n");

    if (hble == NULL) {
        printf("  [FAIL] Pointeur BLE_t NULL\r\n");
        return false;
    }

    testBLE = hble;

    if (!Test_BLE_Hardware_Raw())      return false;
    if (!Test_BLE_InitAndParam())      return false;
    if (!Test_BLE_Configuration())     return false;
    if (!Test_BLE_PhoneSimulation())   return false;

    printf("\r\n========================================\r\n");
    printf("     TOUS LES TESTS BLE PASSES\r\n");
    printf("========================================\r\n\r\n");
    return true;
}
