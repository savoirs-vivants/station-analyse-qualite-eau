/**
 ******************************************************************************
 * @file    driverLPPCayenne.h
 * @brief   Encodeur de payload au format Cayenne LPP pour LoRaWAN.
 * @author  [Ton Nom/Projet]
 *
 * @details
 * Ce module permet de construire facilement une trame binaire compatible
 * avec la norme Cayenne Low Power Payload. Il intègre des sécurités contre
 * le dépassement de buffer et l'overflow mathématique des capteurs.
 ******************************************************************************
 */

#ifndef INC_DRIVERLPPCAYENNE_H_
#define INC_DRIVERLPPCAYENNE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ==========================================================================
 * CONFIGURATION DU DRIVER
 * ========================================================================== */

/** @brief Taille maximale du buffer binaire. (Souvent 51 octets en LoRaWAN SF12) */
#define LPP_MAX_PAYLOAD_SIZE    51U

/* --- Types standards Cayenne LPP --- */
#define LPP_TYPE_DIGITAL_INPUT  0x00
#define LPP_TYPE_DIGITAL_OUTPUT 0x01
#define LPP_TYPE_ANALOG_INPUT   0x02
#define LPP_TYPE_ANALOG_OUTPUT  0x03
#define LPP_TYPE_ILLUMINANCE    0x65
#define LPP_TYPE_PRESENCE       0x66
#define LPP_TYPE_TEMPERATURE    0x67  /* 103 en décimal */
#define LPP_TYPE_HUMIDITY       0x68  /* 104 en décimal */

/* ==========================================================================
 * ÉNUMÉRATIONS ET STRUCTURES
 * ========================================================================== */

/**
 * @brief Codes de retour de l'API LPP.
 */
typedef enum {
    LPP_OK = 0,             /**< Ajout réussi dans le payload */
    LPP_ERR_PARAM,          /**< Erreur de paramètre (pointeur NULL) */
    LPP_ERR_SIZE            /**< Espace insuffisant dans le buffer (Overflow) */
} LPP_Result_t;

/**
 * @brief Structure de stockage d'une trame Cayenne LPP.
 */
typedef struct {
    uint8_t buffer[LPP_MAX_PAYLOAD_SIZE]; /**< Buffer binaire de la trame */
    uint8_t cursor;                       /**< Index du prochain octet libre */
} LPP_Payload_t;

/* ==========================================================================
 * PROTOTYPES DES FONCTIONS (API PUBLIQUE)
 * ========================================================================== */

/**
 * @brief  Initialise ou réinitialise un payload LPP (remet le curseur à zéro).
 * @param  payload Pointeur vers la structure LPP.
 * @retval LPP_OK si succès, LPP_ERR_PARAM si pointeur NULL.
 */
LPP_Result_t LPP_Init(LPP_Payload_t *payload);

/**
 * @brief  Ajoute une mesure de température (Résolution: 0.1°C).
 * @param  payload Pointeur vers la structure LPP.
 * @param  channel Numéro du canal de la donnée (ex: 1).
 * @param  celsius Valeur de la température en degrés Celsius.
 * @retval LPP_OK, LPP_ERR_PARAM, ou LPP_ERR_SIZE.
 */
LPP_Result_t LPP_AddTemperature(LPP_Payload_t *payload, uint8_t channel, float celsius);

/**
 * @brief  Ajoute une mesure analogique générique (Résolution: 0.01).
 * @param  payload Pointeur vers la structure LPP.
 * @param  channel Numéro du canal de la donnée (ex: 2).
 * @param  value   Valeur brute du capteur.
 * @retval LPP_OK, LPP_ERR_PARAM, ou LPP_ERR_SIZE.
 */
LPP_Result_t LPP_AddAnalogInput(LPP_Payload_t *payload, uint8_t channel, float value);

/**
 * @brief  Ajoute une mesure d'humidité relative (Résolution: 0.5%).
 * @param  payload       Pointeur vers la structure LPP.
 * @param  channel       Numéro du canal de la donnée (ex: 3).
 * @param  rh_percentage Humidité en pourcentage (0.0 à 100.0).
 * @retval LPP_OK, LPP_ERR_PARAM, ou LPP_ERR_SIZE.
 */
LPP_Result_t LPP_AddHumidity(LPP_Payload_t *payload, uint8_t channel, float rh_percentage);

/**
 * @brief  Formate le payload binaire en une commande AT Hexadécimale prête à envoyer.
 * @param  payload       Pointeur vers la structure LPP.
 * @param  at_cmd_buffer Buffer de destination pour la chaîne de caractères.
 * @param  max_len       Taille maximale allouée pour le buffer de destination.
 * @return La longueur de la chaîne générée (sans le \0), ou -1 en cas d'erreur.
 */
int LPP_GetATCommand(LPP_Payload_t *payload, char *at_cmd_buffer, size_t max_len);

/**
 * @brief  Convertit le payload binaire en une simple chaîne Hexadécimale brute.
 * @param  payload    Pointeur vers la structure LPP.
 * @param  hex_buffer Buffer de destination pour la chaîne de caractères.
 * @param  max_len    Taille maximale allouée pour le buffer de destination.
 * @return La longueur de la chaîne générée (sans le \0), ou -1 en cas d'erreur.
 */
int LPP_GetPayloadHex(LPP_Payload_t *payload, char *hex_buffer, size_t max_len);

#endif /* INC_DRIVERLPPCAYENNE_H_ */
