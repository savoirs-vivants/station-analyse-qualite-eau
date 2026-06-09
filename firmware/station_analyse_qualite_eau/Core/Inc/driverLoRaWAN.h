/**
 ******************************************************************************
 * @file    driverLoRaWAN.h
 * @brief   Pilote asynchrone pour module LoRaWAN commandé par AT.
 * @author  [Ton Nom/Projet]
 *
 * @details
 * Ce driver implémente une machine d'état (FSM) 100% non-bloquante basée sur
 * l'UART DMA. Il gère la configuration, la jonction au réseau (OTAA), l'envoi
 * de payloads (Hexadécimal), ainsi que la gestion stricte de l'énergie
 * (Sleep / WakeUp).
 ******************************************************************************
 */

#ifndef INC_DRIVER_LORAWAN_H_
#define INC_DRIVER_LORAWAN_H_

#include "main.h"
#include "usart.h"
#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * ÉNUMÉRATIONS APPLICATIVES
 * ========================================================================== */

/**
 * @brief Codes de retour immédiat de l'API.
 */
typedef enum {
    LORA_OK = 0,            /**< Commande acceptée avec succès */
    LORA_ERR_PARAM,         /**< Erreur de paramètre (ex: pointeur NULL) */
    LORA_ERR_BUSY,          /**< Refus : le module traite déjà une commande AT */
    LORA_ERR_NOT_JOINED     /**< Refus : tentative d'envoi sans être sur le réseau */
} LoRa_Result_t;

/**
 * @brief Statut public et opérationnel du driver.
 * @note  C'est la seule information d'état que l'application (main.c) doit lire.
 */
typedef enum {
    LORA_STATUS_UNINIT = 0, /**< Structure non initialisée */
    LORA_STATUS_READY,      /**< Module prêt à recevoir une commande réseau */
    LORA_STATUS_BUSY,       /**< Transaction AT et attente de réponse en cours */
    LORA_STATUS_SLEEP,      /**< Module en mode basse consommation (Low Power) */
    LORA_STATUS_ERROR       /**< Erreur critique (Timeout répété, refus réseau) */
} LoRa_Status_t;

/**
 * @brief États internes de la machine d'état (FSM).
 * @warning À usage strictement privé par LoRaWAN_Process().
 */
typedef enum {
    S_LORA_IDLE = 0,

    /* --- Séquence de Configuration --- */
    S_LORA_CONFIG_SEND,
    S_LORA_CONFIG_WAIT,

    /* --- Séquence de Jonction (OTAA) --- */
    S_LORA_JOIN_SEND,
    S_LORA_JOIN_WAIT,

    /* --- Séquence d'Envoi (TX) --- */
    S_LORA_TX_SEND,
    S_LORA_TX_WAIT,

    /* --- Gestion d'Énergie --- */
    S_LORA_GOTO_SLEEP,
    S_LORA_WAIT_SLEEP,
    S_LORA_SLEEP,
    S_LORA_WAKING_UP,
    S_LORA_WAKING_UP_WAIT,

    /* --- Gestion des Erreurs --- */
    S_LORA_ERROR
} LoRa_State_t;


/* ==========================================================================
 * STRUCTURES DE DONNÉES
 * ========================================================================== */

/**
 * @brief Contexte global d'exécution du driver LoRaWAN.
 */
typedef struct {
    /* --- Couche Matérielle --- */
    UART_HandleTypeDef *huart;      /**< Pointeur vers le périphérique UART (avec DMA activé) */

    /* --- Machine d'État --- */
    LoRa_State_t state;             /**< État actuel interne de la FSM */
    LoRa_Status_t status;           /**< Statut public du module */

    /* --- Buffers de Communication --- */
    char rx_buffer[128];            /**< Buffer de réception des réponses AT */
    char tx_buffer[128];            /**< Buffer de transmission sécurisé */
    volatile uint16_t rx_length;    /**< Nombre d'octets reçus par le DMA */
    volatile bool rx_complete;      /**< Flag levé par l'interruption DMA (UART IDLE) */

    /* --- Gestion Temporelle et Logique --- */
    uint32_t cmd_start_time;        /**< Timestamp du dernier envoi pour calcul du Timeout */
    uint8_t config_step;            /**< Index de la commande actuelle dans la séquence d'initialisation */
    uint8_t retry_count;            /**< Compteur de tentatives (Retry) en cas de réponse "ERROR" */
    bool is_joined;                 /**< Flag indiquant si le module est connecté au réseau LoRaWAN */
} LoRaWAN_t;


/* ==========================================================================
 * PROTOTYPES DES FONCTIONS (API PUBLIQUE)
 * ========================================================================== */

/**
 * @brief  Initialise le driver en associant l'UART.
 * @param  lora  Pointeur vers l'instance du driver.
 * @param  huart Pointeur vers le handle UART HAL.
 * @retval LORA_OK en cas de succès, LORA_ERR_PARAM si pointeur invalide.
 */
LoRa_Result_t LoRaWAN_Init(LoRaWAN_t *lora, UART_HandleTypeDef *huart);

/**
 * @brief  Lance la séquence complète de configuration AT (Mode, Class, DevEUI, AppKey...).
 * @param  lora Pointeur vers l'instance du driver.
 * @retval LORA_OK si accepté, LORA_ERR_BUSY si le module est occupé.
 */
LoRa_Result_t LoRaWAN_Configure(LoRaWAN_t *lora);

/**
 * @brief  Demande au module de rejoindre le réseau (Join OTAA).
 * @param  lora Pointeur vers l'instance du driver.
 * @retval LORA_OK si accepté, LORA_ERR_BUSY si le module est occupé.
 */
LoRa_Result_t LoRaWAN_Join(LoRaWAN_t *lora);

/**
 * @brief  Envoie une charge utile (Payload) sur le réseau LoRaWAN.
 * @param  lora        Pointeur vers l'instance du driver.
 * @param  hex_payload Chaîne de caractères hexadécimale (ex: "016700FF").
 * @retval LORA_OK, LORA_ERR_BUSY, ou LORA_ERR_NOT_JOINED.
 */
LoRa_Result_t LoRaWAN_SendHex(LoRaWAN_t *lora, const char *hex_payload);

/**
 * @brief  Place le module LoRaWAN en mode basse consommation (AT+LOWPOWER).
 * @param  lora Pointeur vers l'instance du driver.
 * @retval LORA_OK si accepté, LORA_ERR_BUSY si le module est occupé.
 */
LoRa_Result_t LoRaWAN_Sleep(LoRaWAN_t *lora);

/**
 * @brief  Réveille matériellement le module LoRaWAN via une impulsion UART.
 * @param  lora Pointeur vers l'instance du driver.
 * @retval LORA_OK si accepté, LORA_ERR_BUSY si le module est occupé.
 */
LoRa_Result_t LoRaWAN_WakeUp(LoRaWAN_t *lora);

/**
 * @brief  Machine d'état (FSM) traitant les requêtes et les réponses UART de façon asynchrone.
 * @note   Doit être appelée régulièrement dans la boucle principale (main / while(1)).
 * @param  lora Pointeur vers l'instance du driver.
 */
void LoRaWAN_Process(LoRaWAN_t *lora);

/**
 * @brief  Récupère le statut opérationnel public du driver.
 * @param  lora Pointeur vers l'instance du driver.
 * @retval LORA_STATUS_READY, LORA_STATUS_BUSY, LORA_STATUS_SLEEP ou LORA_STATUS_ERROR.
 */
LoRa_Status_t LoRaWAN_GetStatus(LoRaWAN_t *lora);

/**
 * @brief  Acquitte une erreur fatale et replace le driver en état READY.
 * @param  lora Pointeur vers l'instance du driver.
 */
void LoRaWAN_ResetError(LoRaWAN_t *lora);

/**
 * @brief  Callback à appeler depuis l'interruption HAL_UARTEx_RxEventCallback.
 * @note   Avertit la FSM qu'une réponse AT a été reçue.
 * @param  lora Pointeur vers l'instance du driver.
 * @param  Size Taille des données reçues.
 */
void LoRaWAN_RxEventCallback(LoRaWAN_t *lora, uint16_t Size);

#endif /* INC_DRIVER_LORAWAN_H_ */
