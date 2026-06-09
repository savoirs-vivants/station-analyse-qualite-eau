/*
 * test_fram.c
 * Implémentation des tests du driver FRAM
 */

#include "test_fram.h"
#include <stdio.h>
#include <string.h>

/* Pointeur vers le handle de l'application */
static FRAM_t *testFRAM;

/* --- Macros d'assertion (identiques à celles du DataLog) --- */

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


/* --- Fonction utilitaire d'attente --- */

/**
 * @brief Attend que la FRAM repasse en mode READY avec un Timeout.
 */
static bool WaitFramIdle(void)
{
    uint32_t guard = HAL_GetTick() + 1000U; /* 1 seconde max */

    while (FRAM_GetStatus(testFRAM) == FRAM_STATUS_BUSY)
    {
        FRAM_Process(testFRAM);

        if (HAL_GetTick() > guard)
        {
            printf("  [TIMEOUT] WaitFramIdle bloque !\r\n");
            return false;
        }
    }
    return true;
}


/* ===================================================================
 * BATTERIE DE TESTS
 * =================================================================== */

/** * TEST 0 : Lecture du Device ID (Mode Bloquant)
 * Vérifie que la puce est vivante et que le bus SPI de base fonctionne.
 */
static bool Test_ReadDeviceID(void)
{
    printf("\r\n--- TEST 0 : Lecture Device ID (SPI Bloquant) ---\r\n");

    /* La commande RDID est 0x9F pour 99% des FRAM (Cypress, Fujitsu, Rohm...) */
    uint8_t cmd_rdid = 0x9F;
    uint8_t id_buffer[4] = {0}; /* La plupart des FRAM renvoient 4 à 9 octets */

    /* 1. On abaisse le CS */
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, RESET);

    /* 2. On envoie la commande (Bloquant, Timeout 100ms) */
    HAL_StatusTypeDef tx_res = HAL_SPI_Transmit(testFRAM->hspi, &cmd_rdid, 1, 100);

    /* 3. On lit la réponse (Bloquant, Timeout 100ms) */
    HAL_StatusTypeDef rx_res = HAL_SPI_Receive(testFRAM->hspi, id_buffer, sizeof(id_buffer), 100);

    /* 4. On remonte le CS */
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, SET);

    /* --- Analyse des résultats --- */
    EXPECT(tx_res == HAL_OK && rx_res == HAL_OK, "Communication SPI bloquante OK");

    printf("  -> ID brut recu : %02X %02X %02X %02X\r\n",
           id_buffer[0], id_buffer[1], id_buffer[2], id_buffer[3]);

    /* Si on lit que des 0x00 ou que des 0xFF, le bus (ou la puce) est mort */
    if (id_buffer[0] == 0x00 || id_buffer[0] == 0xFF) {
        printf("  [FAIL] La ligne MISO semble muette (0x00 ou 0xFF constants).\r\n");
        return false;
    }

    printf("  [ OK ] La puce a repondu avec un ID coherent !\r\n");
    return true;
}

/** * TEST 0.1 : Diagnostic Registre de Statut
 * Vérifie que la puce accepte de se déverrouiller pour une écriture.
 */
static bool Test_DiagnoseStatus(void)
{
    printf("\r\n--- TEST 0.1 : Diagnostic Registre de Statut ---\r\n");
    uint8_t cmd_rdsr = 0x05; // Commande Read Status Register
    uint8_t cmd_wren = 0x06; // Commande Write Enable
    uint8_t sr_avant = 0;
    uint8_t sr_apres = 0;

    /* 1. Lire SR avant */
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, RESET);
    HAL_SPI_Transmit(testFRAM->hspi, &cmd_rdsr, 1, 100);
    HAL_SPI_Receive(testFRAM->hspi, &sr_avant, 1, 100);
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, SET);
    HAL_Delay(1);

    /* 2. Envoyer WREN */
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, RESET);
    HAL_SPI_Transmit(testFRAM->hspi, &cmd_wren, 1, 100);
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, SET);
    HAL_Delay(1);

    /* 3. Lire SR apres WREN */
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, RESET);
    HAL_SPI_Transmit(testFRAM->hspi, &cmd_rdsr, 1, 100);
    HAL_SPI_Receive(testFRAM->hspi, &sr_apres, 1, 100);
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, SET);

    printf("  -> SR Avant WREN : 0x%02X\r\n", sr_avant);
    printf("  -> SR Apres WREN : 0x%02X\r\n", sr_apres);

    /* Analyse des bits : WEL est le bit 1 (0x02), BP0/BP1 sont les bits 2 et 3 (0x0C) */
    if ((sr_apres & 0x02) == 0) {
        printf("  [FAIL] Le bit WEL refuse de passer a 1. La puce est verrouillee !\r\n");
        return false;
    }
    if ((sr_apres & 0x0C) != 0) {
        printf("  [FAIL] La memoire est partiellement protegee en lecture seule (BP0/BP1) !\r\n");
        return false;
    }

    printf("  [ OK ] Le registre de statut est parfait (Pret a ecrire).\r\n");
    return true;
}

/** * TEST 0.2 : Déverrouillage d'urgence de la FRAM
 * Remet le registre de statut à 0 pour autoriser les écritures.
 */
static bool Test_UnlockFRAM(void)
{
    printf("\r\n--- TEST 0.2 : Deverrouillage de la FRAM (WRSR) ---\r\n");

    uint8_t cmd_wren = 0x06; // Commande WREN (Indispensable avant de toucher au SR)
    uint8_t cmd_wrsr[2] = {0x01, 0x00}; // Commande WRSR (0x01) + Valeur 0x00 pour tout effacer
    uint8_t cmd_rdsr = 0x05; // Commande RDSR pour vérifier
    uint8_t sr_verif = 0;

    /* 1. On donne l'autorisation d'écrire (WREN) */
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, RESET);
    HAL_SPI_Transmit(testFRAM->hspi, &cmd_wren, 1, 100);
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, SET);
    HAL_Delay(1); // Laisse le temps à la puce d'activer le latch

    /* 2. On ecrase le registre de statut avec 0x00 */
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, RESET);
    HAL_SPI_Transmit(testFRAM->hspi, cmd_wrsr, 2, 100);
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, SET);
    HAL_Delay(5); // Laisse le temps d'enregistrer

    /* 3. On lit pour vérifier */
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, RESET);
    HAL_SPI_Transmit(testFRAM->hspi, &cmd_rdsr, 1, 100);
    HAL_SPI_Receive(testFRAM->hspi, &sr_verif, 1, 100);
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, SET);

    printf("  -> Nouveau SR : 0x%02X\r\n", sr_verif);

    if ((sr_verif & 0x0C) != 0) {
        printf("  [FAIL] Impossible d'effacer la protection. (Vérifie ta broche WP !)\r\n");
        return false;
    }

    printf("  [ OK ] FRAM totallement deverrouillee ! Les ecritures sont permises.\r\n");
    return true;
}

/** * TEST 0.5 : Write/Read 100% Bloquant (Sans DMA)
 * Permet de prouver que la puce accepte physiquement les ecritures.
 */
static bool Test_BlockingWriteRead(void)
{
    printf("\r\n--- TEST 0.5 : Write/Read (SPI Bloquant) ---\r\n");

    uint8_t tx_test[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint8_t rx_test[8] = {0};
    uint8_t cmd_wren = 0x06;
    uint8_t cmd_write[4] = {0x02, 0x01, 0x00, 0x00}; // Adresse 0x010000
    uint8_t cmd_read[4]  = {0x03, 0x01, 0x00, 0x00};

    /* 1. WREN */
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, RESET);
    HAL_SPI_Transmit(testFRAM->hspi, &cmd_wren, 1, 100);
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, SET);
    HAL_Delay(1); // Marge de sécurité tCEH

    /* 2. WRITE */
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, RESET);
    HAL_SPI_Transmit(testFRAM->hspi, cmd_write, 4, 100);
    HAL_SPI_Transmit(testFRAM->hspi, tx_test, 8, 100);
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, SET);
    HAL_Delay(1);

    /* 3. READ */
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, RESET);
    HAL_SPI_Transmit(testFRAM->hspi, cmd_read, 4, 100);
    HAL_SPI_Receive(testFRAM->hspi, rx_test, 8, 100);
    HAL_GPIO_WritePin(testFRAM->cs_gpio_port, testFRAM->cs_gpio_pin, SET);

    int diff = memcmp(tx_test, rx_test, 8);
    if (diff != 0) {
        printf("  -> TX : %02X %02X %02X %02X\r\n", tx_test[0], tx_test[1], tx_test[2], tx_test[3]);
        printf("  -> RX : %02X %02X %02X %02X\r\n", rx_test[0], rx_test[1], rx_test[2], rx_test[3]);
        printf("  [FAIL] La memoire refuse l'ecriture ! (Broche WP ? Registre bloque ?)\r\n");
        return false;
    }

    printf("  [ OK ] Ecriture/Lecture Bloquante parfaite !\r\n");
    return true;
}

/** * TEST 1 : Robustesse des paramètres (Mode Synchrone)
 * Vérifie que le driver rejette bien les requêtes invalides sans planter.
 */
static bool Test_BadParameters(void)
{
    printf("\r\n--- TEST 1 : Parametres Invalides ---\r\n");

    uint8_t dummy_buf[4] = {0};

    /* 1. Test pointeurs NULL */
    EXPECT_EQ(FRAM_Write(NULL, 0x00, dummy_buf, 4), FRAM_ERR_PARAM, "Rejet hfram NULL");
    EXPECT_EQ(FRAM_Write(testFRAM, 0x00, NULL, 4), FRAM_ERR_PARAM, "Rejet buffer NULL");

    /* 2. Test taille nulle */
    EXPECT_EQ(FRAM_Read(testFRAM, 0x00, dummy_buf, 0), FRAM_ERR_PARAM, "Rejet taille 0");

    /* 3. Test dépassement de capacité mémoire */
    uint32_t out_of_bounds_addr = FRAM_MAX_ADDRESS - 2;
    EXPECT_EQ(FRAM_Write(testFRAM, out_of_bounds_addr, dummy_buf, 4), FRAM_ERR_PARAM, "Rejet depassement adresse");

    return true;
}

/** * TEST 2 : Intégrité d'Écriture et de Lecture (Mode Asynchrone)
 * Écrit un motif connu, attend, le relit et compare.
 */
static bool Test_WriteReadIntegrity(void)
{
    printf("\r\n--- TEST 2 : Integrite Write/Read ---\r\n");

    uint32_t test_address = 0x0100; // Une adresse arbitraire
    uint8_t tx_data[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
    uint8_t rx_data[8] = {0};

    /* --- Écriture --- */
    EXPECT_EQ(FRAM_Write(testFRAM, test_address, tx_data, sizeof(tx_data)), FRAM_OK, "Write accepte");
    EXPECT(WaitFramIdle(), "Attente Write terminee");
    EXPECT_EQ(FRAM_GetStatus(testFRAM), FRAM_STATUS_READY, "Statut de nouveau READY");

    /* --- Lecture --- */
    EXPECT_EQ(FRAM_Read(testFRAM, test_address, rx_data, sizeof(rx_data)), FRAM_OK, "Read accepte");
    EXPECT(WaitFramIdle(), "Attente Read terminee");
    EXPECT_EQ(FRAM_GetStatus(testFRAM), FRAM_STATUS_READY, "Statut de nouveau READY");

    /* --- Vérification --- */
    int diff = memcmp(tx_data, rx_data, sizeof(tx_data));

    if (diff != 0) {
        printf("  -> TX : %02X %02X %02X %02X ...\r\n", tx_data[0], tx_data[1], tx_data[2], tx_data[3]);
        printf("  -> RX : %02X %02X %02X %02X ...\r\n", rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
    }

    EXPECT_EQ(diff, 0, "Donnees lues identiques aux donnees ecrites");

    return true;
}

/** * TEST 3 : Protection d'occupation (Busy State)
 * Lance une écriture et tente immédiatement une autre action.
 */
static bool Test_BusyProtection(void)
{
    printf("\r\n--- TEST 3 : Protection Etat BUSY ---\r\n");

    uint32_t test_address = 0x0200;
    uint8_t dummy_data[16] = {0x55};
    uint8_t rx_data[16] = {0};

    /* On lance une écriture longue */
    EXPECT_EQ(FRAM_Write(testFRAM, test_address, dummy_data, sizeof(dummy_data)), FRAM_OK, "Write 1 accepte");

    /* On tente de lire immédiatement par-dessus (le DMA travaille encore) */
    EXPECT_EQ(FRAM_Read(testFRAM, test_address, rx_data, sizeof(rx_data)), FRAM_ERR_BUSY, "Read rejete car driver occupe");

    /* On laisse la première transaction se terminer proprement */
    WaitFramIdle();

    return true;
}

/* ===================================================================
 * RUNNER PRINCIPAL
 * =================================================================== */

bool TestFRAM_RunAll(FRAM_t *hfram)
{
    printf("\r\n========================================\r\n");
    printf("     BATTERIE DE TESTS FRAM BRUTE\r\n");
    printf("========================================\r\n");

    if (hfram == NULL) {
        printf("[FAIL] Pointeur FRAM_t fourni est NULL !\r\n");
        return false;
    }

    testFRAM = hfram;

    if(!Test_ReadDeviceID())		{ return false; }
    if(!Test_UnlockFRAM())			{ return false; }
    if(!Test_DiagnoseStatus())		{ return false; }
    if(!Test_BlockingWriteRead())	{ return false; }
    if (!Test_BadParameters())      { return false; }
    if (!Test_WriteReadIntegrity()) { return false; }
    if (!Test_BusyProtection())     { return false; }

    printf("\r\n========================================\r\n");
    printf("     TOUS LES TESTS FRAM PASSES\r\n");
    printf("========================================\r\n\r\n");

    return true;
}
