/**
 * @file    test_lpp.c
 * @brief   Implémentation des tests de robustesse et d'encodage LPP.
 */

#include "test_lpp.h"
#include <stdio.h>
#include <string.h>

/* --- Macros d'assertion standards --- */
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
                   msg, (int)(actual), (int)(expected), __LINE__);          \
            return false;                                                   \
        } else {                                                            \
            printf("  [ OK ] %s\r\n", msg);                                 \
        }                                                                   \
    } while (0)


/* ===================================================================
 * TESTS UNITAIRES
 * =================================================================== */

/** TEST 1 : Sécurité des pointeurs NULL */
static bool Test_LPP_Pointers(void) {
    printf("\r\n--- TEST 1 : Pointeurs Invalides ---\r\n");

    EXPECT_EQ(LPP_Init(NULL), LPP_ERR_PARAM, "Rejet LPP_Init(NULL)");
    EXPECT_EQ(LPP_AddTemperature(NULL, 1, 20.0f), LPP_ERR_PARAM, "Rejet LPP_AddTemperature(NULL)");
    EXPECT_EQ(LPP_GetPayloadHex(NULL, NULL, 50), -1, "Rejet LPP_GetPayloadHex(NULL)");

    return true;
}

/** TEST 2 : Encodage de base mathématique (Précision) */
static bool Test_LPP_BasicEncoding(void) {
    printf("\r\n--- TEST 2 : Encodage Mathematique ---\r\n");

    LPP_Payload_t lpp;
    LPP_Init(&lpp);

    /* Température : 25.5°C -> 255 -> 0x00FF */
    EXPECT_EQ(LPP_AddTemperature(&lpp, 1, 25.5f), LPP_OK, "Ajout Temp OK");
    EXPECT_EQ(lpp.cursor, 4, "Taille curseur = 4");
    EXPECT_EQ(lpp.buffer[0], 0x01, "Canal = 1");
    EXPECT_EQ(lpp.buffer[1], 0x67, "Type = Temp (0x67)");
    EXPECT_EQ(lpp.buffer[2], 0x00, "MSB = 0x00");
    EXPECT_EQ(lpp.buffer[3], 0xFF, "LSB = 0xFF");

    /* Humidité : 50.5% -> 50.5 * 2 = 101 -> 0x65 */
    EXPECT_EQ(LPP_AddHumidity(&lpp, 2, 50.5f), LPP_OK, "Ajout Hum OK");
    EXPECT_EQ(lpp.cursor, 7, "Taille curseur = 7 (4+3)");
    EXPECT_EQ(lpp.buffer[6], 0x65, "Valeur Hum = 0x65");

    return true;
}

/** TEST 3 : Protection anti-débordement mathématique (Clamping) */
static bool Test_LPP_Clamping(void) {
    printf("\r\n--- TEST 3 : Saturation Mathematique (Anti-Overflow) ---\r\n");

    LPP_Payload_t lpp;
    LPP_Init(&lpp);

    /* Température extrême : 5000°C -> Saturé à 3276.7°C -> 32767 -> 0x7FFF */
    LPP_AddTemperature(&lpp, 1, 5000.0f);
    EXPECT_EQ(lpp.buffer[2], 0x7F, "Clamping Temp MSB = 0x7F");
    EXPECT_EQ(lpp.buffer[3], 0xFF, "Clamping Temp LSB = 0xFF");

    /* Analog Input négatif extrême : -500.0 -> Saturé à -327.68 -> -32768 -> 0x8000 */
    LPP_AddAnalogInput(&lpp, 2, -500.0f);
    EXPECT_EQ(lpp.buffer[6], 0x80, "Clamping Analog MSB = 0x80");
    EXPECT_EQ(lpp.buffer[7], 0x00, "Clamping Analog LSB = 0x00");

    /* Humidité absurde : 250% -> Saturé à 100% -> 200 -> 0xC8 */
    LPP_AddHumidity(&lpp, 3, 250.0f);
    EXPECT_EQ(lpp.buffer[10], 0xC8, "Clamping Hum = 0xC8");

    return true;
}

/** TEST 4 : Protection anti-débordement mémoire (Buffer Overflow) */
static bool Test_LPP_BufferLimits(void) {
    printf("\r\n--- TEST 4 : Limites du Buffer (%d octets) ---\r\n", LPP_MAX_PAYLOAD_SIZE);

    LPP_Payload_t lpp;
    LPP_Init(&lpp);

    /* Remplissage de la trame (ex: LPP_MAX_PAYLOAD_SIZE = 51) */
    /* On ajoute 12 températures de 4 octets = 48 octets */
    for(int i = 0; i < 12; i++) {
        EXPECT_EQ(LPP_AddTemperature(&lpp, i, 20.0f), LPP_OK, "Remplissage progressif");
    }

    EXPECT_EQ(lpp.cursor, 48, "Curseur a 48/51 octets");

    /* Tentative d'ajout d'une 13ème temp (nécessite 4 octets, or il n'en reste que 3) */
    EXPECT_EQ(LPP_AddTemperature(&lpp, 13, 20.0f), LPP_ERR_SIZE, "Rejet : Espace insuffisant pour Temp");

    /* Mais il reste de la place pour une humidité (nécessite 3 octets) ! */
    EXPECT_EQ(LPP_AddHumidity(&lpp, 13, 50.0f), LPP_OK, "Acceptation : Espace suffisant pour Hum");
    EXPECT_EQ(lpp.cursor, 51, "Curseur plein (51/51)");

    return true;
}

/** TEST 5 : Génération des chaînes de caractères (Hexa et AT Command) */
static bool Test_LPP_StringGeneration(void) {
    printf("\r\n--- TEST 5 : Generation Chaines Caracteres ---\r\n");

    LPP_Payload_t lpp;
    LPP_Init(&lpp);
    LPP_AddTemperature(&lpp, 1, 25.5f); /* Produit: 01 67 00 FF */

    char buffer[50];

    /* Test du Hexa simple */
    int len = LPP_GetPayloadHex(&lpp, buffer, sizeof(buffer));
    EXPECT_EQ(len, 8, "Longueur Hexa = 8");
    EXPECT(strcmp(buffer, "016700FF") == 0, "Chaine Hexa correcte");

    /* Test de la commande AT */
    len = LPP_GetATCommand(&lpp, buffer, sizeof(buffer));
    EXPECT_EQ(len, 22, "Longueur AT Command = 22"); /* AT+MSGHEX="016700FF"\r\n = 22 chars */

    /* On vérifie le préfixe et le suffixe manuellement */
    EXPECT(strncmp(buffer, "AT+MSGHEX=\"016700FF\"\r\n", 22) == 0, "Chaine AT Command correcte");

    /* Sécurité sur les buffers trop petits */
    EXPECT_EQ(LPP_GetPayloadHex(&lpp, buffer, 5), -1, "Rejet si buffer destination trop petit");

    return true;
}

/* ===================================================================
 * RUNNER
 * =================================================================== */

bool TestLPP_RunAll(void) {
    printf("\r\n========================================\r\n");
    printf("     BATTERIE DE TESTS CAYENNE LPP\r\n");
    printf("========================================\r\n");

    if (!Test_LPP_Pointers())         return false;
    if (!Test_LPP_BasicEncoding())    return false;
    if (!Test_LPP_Clamping())         return false;
    if (!Test_LPP_BufferLimits())     return false;
    if (!Test_LPP_StringGeneration()) return false;

    printf("\r\n========================================\r\n");
    printf("     TOUS LES TESTS LPP PASSES\r\n");
    printf("========================================\r\n\r\n");
    return true;
}
