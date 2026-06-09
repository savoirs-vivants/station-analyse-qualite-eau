/**
 * @file    driverFRAM.h
 * @brief   Pilote asynchrone par DMA pour mémoire FRAM SPI.
 * @author  valen
 * @date    16 avr. 2026
 * * Ce driver gère les écritures et lectures non-bloquantes vers une mémoire FRAM
 * via le bus SPI et le DMA. Il intègre une machine d'état robuste avec gestion
 * de timeouts et prévention des dépassements de mémoire.
 */

#ifndef INC_DRIVERFRAM_H_
#define INC_DRIVERFRAM_H_

#include <stdbool.h>
#include "main.h"
#include "spi.h"
#include "gpio.h"

/* ==========================================================================
 * MACROS ET CONFIGURATION
 * ========================================================================== */

/** @brief Taille maximale du buffer DMA (Payload de 256 octets + 4 octets de header SPI) */
#define MAX_BUFFER_SIZE     260

/** @brief Capacité totale de la FRAM en octets (Ex: 2Mbits = 256 Ko = 262144 octets) */
#define FRAM_MAX_ADDRESS    262144

/** @brief Temps d'attente maximum autorisé pour une transaction DMA avant de déclarer une erreur (en millisecondes) */
#define FRAM_TIMEOUT_MS     5

/* ==========================================================================
 * ENUMÉRATIONS APPLICATIVES
 * ========================================================================== */

/**
 * @brief Code de retour immédiat lors de l'appel d'une fonction de l'API.
 */
typedef enum {
    FRAM_OK = 0,         /**< Opération acceptée et démarrée avec succès */
    FRAM_ERR_PARAM,      /**< Paramètre invalide (Pointeur NULL, taille nulle, adresse hors limite) */
    FRAM_ERR_BUSY,       /**< Le driver est déjà occupé par une autre transaction */
    FRAM_ERR_HW,         /**< Erreur matérielle immédiate remontée par le HAL */
    FRAM_ERR_TIMEOUT     /**< Opération matérielle bloquée trop longtemps */
} FRAM_Result_t;

/**
 * @brief État global du driver, visible depuis l'extérieur (ex: pour le DataLog).
 */
typedef enum {
    FRAM_STATUS_READY = 0, /**< Prêt pour une nouvelle commande (Machine d'état IDLE) */
    FRAM_STATUS_BUSY,      /**< En cours de lecture/écriture via DMA */
    FRAM_STATUS_ERROR      /**< Erreur critique rencontrée, nécessite une action de la couche supérieure */
} FRAM_Status_t;

/**
 * @brief Code d'erreur interne détaillé pour le débogage de la machine d'état.
 */
typedef enum {
    FRAM_INT_ERR_NONE = 0,        /**< Aucune erreur */
    FRAM_INT_ERR_TIMEOUT_WREN,    /**< Plantage/Timeout en attendant le Write Enable (WREN) */
    FRAM_INT_ERR_TIMEOUT_WRITE,   /**< Plantage/Timeout pendant l'envoi des données (WRITE) */
    FRAM_INT_ERR_TIMEOUT_READ,    /**< Plantage/Timeout pendant la lecture (READ) */
    FRAM_INT_ERR_DMA_HALT         /**< Erreur matérielle remontée par le HAL SPI/DMA */
} FRAM_ErrorCode_t;

/**
 * @brief États internes de la machine d'état (FSM) du driver.
 */
typedef enum {
    FRAM_STATE_IDLE,              /**< Au repos */
    FRAM_STATE_WRITE_EN,          /**< Envoi de la commande WREN */
    FRAM_STATE_WAIT_WRITE_EN,     /**< Attente de la fin de l'envoi WREN */
    FRAM_STATE_WRITE_DATA,        /**< Envoi de la commande WRITE, de l'adresse et des données */
    FRAM_STATE_WAIT_WRITE_DATA,   /**< Attente de la fin de l'écriture des données */
    FRAM_STATE_READ_EN,           /**< Envoi de la commande READ, de l'adresse et lecture des données */
    FRAM_STATE_WAIT_READ_DATA,    /**< Attente de la fin de la lecture continue */
	FRAM_STATE_ERROR			  /**< Etat d'erreur */
} FRAM_State_t;

/* ==========================================================================
 * STRUCTURE PRINCIPALE
 * ========================================================================== */

/**
 * @brief Structure de contexte du driver FRAM.
 */
typedef struct {
    /* Périphériques matériels */
    SPI_HandleTypeDef *hspi;      /**< Handle du périphérique SPI utilisé */
    GPIO_TypeDef* cs_gpio_port;   /**< Port GPIO du Chip Select (CS) */
    uint16_t cs_gpio_pin;         /**< Broche GPIO du Chip Select (CS) */

    /* Contexte de la transaction en cours */
    uint32_t address;             /**< Adresse de destination/source en FRAM */
    uint8_t *tx_data;             /**< Pointeur vers les données utilisateur à écrire */
    uint16_t tx_data_size;        /**< Taille des données à écrire */
    uint8_t *rx_data;             /**< Pointeur vers le buffer utilisateur pour lire */
    uint16_t rx_data_size;        /**< Taille des données à lire */

    /* Buffers DMA internes (incluant la place pour l'entête SPI de 4 octets) */
    uint8_t tx_buffer[MAX_BUFFER_SIZE]; /**< Buffer d'émission DMA */
    uint8_t rx_buffer[MAX_BUFFER_SIZE]; /**< Buffer de réception DMA */

    /* Gestion d'état et d'exécution */
    volatile FRAM_State_t state;  /**< État actuel de la machine d'état interne */
    volatile bool cmd_flag;       /**< Drapeau levé par les callbacks DMA pour signifier la fin d'un transfert */

    /* Traçabilité et sécurité */
    volatile FRAM_Status_t status;/**< État global du driver pour la couche supérieure */
    FRAM_ErrorCode_t error_code;  /**< Trace de la dernière erreur asynchrone survenue */
    uint32_t start_time;          /**< Timestamp de début de transaction (pour gestion du timeout) */
} FRAM_t;

/* ==========================================================================
 * PROTOTYPES DES FONCTIONS (API)
 * ========================================================================== */

/**
 * @brief Initialise la structure du driver et sécurise le bus SPI.
 * @param hfram     Pointeur vers l'instance du driver FRAM.
 * @param hspi      Pointeur vers le handle SPI (ex: &hspi3).
 * @param gpio_port Port GPIO du Chip Select.
 * @param gpio_pin  Broche GPIO du Chip Select.
 * @retval FRAM_OK si succès, sinon un code d'erreur synchrone (FRAM_ERR_PARAM).
 */
FRAM_Result_t FRAM_Init(FRAM_t *hfram, SPI_HandleTypeDef *hspi, GPIO_TypeDef *gpio_port, uint16_t gpio_pin);

/**
 * @brief Lance une opération asynchrone de lecture depuis la FRAM.
 * @param hfram     Pointeur vers l'instance du driver FRAM.
 * @param address   Adresse de départ en mémoire (doit être < FRAM_MAX_ADDRESS).
 * @param rx_data   Pointeur vers le buffer où stocker les données lues.
 * @param data_size Nombre d'octets à lire.
 * @retval FRAM_OK si l'opération est acceptée, sinon un code d'erreur (BUSY, PARAM).
 */
FRAM_Result_t FRAM_Read(FRAM_t *hfram, uint32_t address, uint8_t *rx_data, uint16_t data_size);

/**
 * @brief Lance une opération asynchrone d'écriture vers la FRAM.
 * @param hfram     Pointeur vers l'instance du driver FRAM.
 * @param address   Adresse de départ en mémoire (doit être < FRAM_MAX_ADDRESS).
 * @param data      Pointeur vers les données à écrire.
 * @param data_size Nombre d'octets à écrire.
 * @retval FRAM_OK si l'opération est acceptée, sinon un code d'erreur (BUSY, PARAM).
 */
FRAM_Result_t FRAM_Write(FRAM_t *hfram, uint32_t address, uint8_t *data, uint16_t data_size);

/**
 * @brief Machine d'état du driver. Doit être appelée régulièrement dans la boucle principale.
 * @param hfram Pointeur vers l'instance du driver FRAM.
 */
void FRAM_Process(FRAM_t *hfram);

/**
 * @brief Renvoie le statut opérationnel actuel du driver.
 * @param hfram Pointeur vers l'instance du driver FRAM.
 * @return FRAM_STATUS_READY, FRAM_STATUS_BUSY, ou FRAM_STATUS_ERROR.
 */
FRAM_Status_t FRAM_GetStatus(FRAM_t *hfram);

/* ==========================================================================
 * CALLBACKS D'INTERRUPTIONS
 * ========================================================================== */

/**
 * @brief Callback à appeler dans HAL_SPI_RxCpltCallback ou HAL_SPI_TxRxCpltCallback.
 * @param hfram Pointeur vers l'instance du driver FRAM.
 */
void FRAM_Rx_Callback(FRAM_t *hfram);

/**
 * @brief Callback à appeler dans HAL_SPI_TxCpltCallback.
 * @param hfram Pointeur vers l'instance du driver FRAM.
 */
void FRAM_Tx_Callback(FRAM_t *hfram);

#endif /* INC_DRIVERFRAM_H_ */
