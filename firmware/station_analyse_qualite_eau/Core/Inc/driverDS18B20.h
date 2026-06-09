/**
 * @file    driverDS18B20.h
 * @author  valen
 * @brief   Driver asynchrone non-bloquant pour capteur de température DS18B20 via UART (1-Wire Hack).
 */

#ifndef INC_DRIVERDS18B20_H_
#define INC_DRIVERDS18B20_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================== */
/* INCLUSIONS                                                           */
/* ==================================================================== */
#include <stdint.h>
#include <stdbool.h>
#include "usart.h" // Indispensable pour ton projet (contient UART_HandleTypeDef)

/* ==================================================================== */
/* CONFIGURATION UTILISATEUR ET MACROS                                  */
/* ==================================================================== */

/** @brief L'horloge qui alimente ton LPUART2 */
#define LPUART_CLOCK_FREQ   16000000ULL

/** @brief Calculs des registres faits par le compilateur (avec arrondi au plus proche) */
#define LPUART_BRR_115200   ((256ULL * LPUART_CLOCK_FREQ + (115200 / 2)) / 115200)
#define LPUART_BRR_9600     ((256ULL * LPUART_CLOCK_FREQ + (9600 / 2)) / 9600)

#define DS18B20_TIMEOUT_MS  10
#define DS18B20_MAX_RETRIES 3
#define DS18B20_RETRY_BREAK 2000

#define DS18B20_CONVERSION_TIME 760

/** @brief Seuil pour considérer qu'un octet UART correspond à un bit '1' en 1-Wire */
#define DS1Wire_BIT_1_THRESHOLD 0xFC

/** @brief L'interrupteur maître pour les logs de debug (1 = ON, 0 = OFF) */
#define DEBUG_DS18B20 1

#if DEBUG_DS18B20 == 1
    #define DS_LOG(msg, ...) printf("[DS18B20] " msg "\r\n", ##__VA_ARGS__)
#else
    #define DS_LOG(msg, ...) // Rien, le vide absolu
#endif


/* ==================================================================== */
/* ÉNUMÉRATIONS (TYPES PUBLICS ET PRIVÉS)                               */
/* ==================================================================== */

/** @cond INTERNAL */
/** @brief Les différents états de la machine à états (FSM) */
typedef enum {
    DS_STATE_IDLE,
    DS_STATE_START_RESET,
    DS_STATE_WAIT_RESET_RX,
    DS_STATE_SEND_SKIP_ROM,
    DS_STATE_WAIT_SKIP_ROM_RX,
    DS_STATE_SEND_CONVERT,
    DS_STATE_WAIT_SEND_CONVERT_TX,
    DS_STATE_WAIT_CONVERSION,
    DS_STATE_SEND_READ_SCRATCHPAD,
    DS_STATE_WAIT_READ_SCRATCHPAD_TX,
    DS_STATE_READ_DATA,
    DS_STATE_WAIT_READ_DATA,
    DS_STATE_DECODE_TEMP,
    DS_STATE_WAIT_RETRY,
    DS_STATE_ERROR
} DS18B20_State_t;
/** @endcond */

/** @brief Codes d'erreurs internes remontés en cas de problème */
typedef enum {
    DS_ERROR_NONE = 0,
    DS_ERROR_UART_TIMEOUT,      /**< L'UART ou le DMA n'a pas répondu à temps */
    DS_ERROR_NO_PRESENCE,       /**< Aucun capteur n'a répondu au Reset */
    DS_ERROR_UART_CONFIG,       /**< Impossible de changer le Baudrate */
    DS_ERROR_DMA_INIT,          /**< Le HAL a refusé de lancer le DMA (Busy ou Erreur) */
    DS_ERROR_CRC                /**< Erreur de contrôle de redondance (parasites) */
} DS18B20_Error_t;

/** @brief État global du capteur (pour l'application principale) */
typedef enum {
    DS_STATUS_READY,   /**< Le capteur ne fait rien, la donnée est disponible */
    DS_STATUS_BUSY,    /**< Le capteur est en plein travail (FSM en cours) */
    DS_STATUS_ERROR    /**< Le capteur a planté définitivement */
} DS18B20_Status_t;

/** @brief Retours des fonctions Init et Start */
typedef enum {
    DS_OK = 0,             /**< Commande acceptée avec succès */
    DS_ERR_PARAM,          /**< Paramètre invalide (ex: pointeur NULL) */
    DS_ERR_BUSY,           /**< Le capteur est déjà en train de faire une mesure */
    DS_ERR_HW_LOCKED,      /**< Le capteur est bloqué dans l'état ERROR, il faut le reset */
    DS_ERR_CONFIG          /**< Problème de configuration UART/DMA */
} DS18B20_Result_t;


/* ==================================================================== */
/* STRUCTURE DE CONTEXTE                                                */
/* ==================================================================== */

/**
 * @brief Structure contenant tout le contexte d'une instance du capteur DS18B20.
 * @warning Ordre des variables préservé pour compatibilité mémoire (ABI).
 */
typedef struct {
    DS18B20_State_t state;        /**< L'état actuel de la FSM */
    UART_HandleTypeDef* huart;    /**< Le pointeur vers le LPUART matériel */

    uint8_t rx_buffer[72];        /**< Buffer DMA pour la réception des 72 octets/bits */

    volatile bool tx_done;        /**< Drapeau : Transmission DMA terminée */
    volatile bool rx_done;        /**< Drapeau : Réception DMA terminée */
    uint32_t timer_start;         /**< Horodatage pour les timeouts non bloquants */

    int16_t temperature;          /**< La donnée utile finale (Q11.4). Diviser par 16.0 pour les °C */
    bool temperature_conversion_done; /**< Indique qu'une nouvelle lecture a abouti */

    bool already_converted;       /**< Flag interne : la température a été convertie */
    DS18B20_Error_t error_code;   /**< Code d'erreur actuel */
    DS18B20_Error_t last_error;   /**< Dernier code d'erreur avant retry */
    uint8_t retry_count;          /**< Compteur de tentatives actuelles */
} DS18B20_t;


/* ==================================================================== */
/* PROTOTYPES DES FONCTIONS PUBLIQUES                                   */
/* ==================================================================== */

/**
 * @brief Initialise le contexte du driver DS18B20.
 * @param[in,out] dev Pointeur vers la structure du capteur.
 * @param[in] huart Pointeur vers le périphérique UART associé.
 * @return DS_OK si succès, DS_ERR_PARAM si pointeur invalide.
 */
DS18B20_Result_t DS18B20_Init(DS18B20_t *dev, UART_HandleTypeDef *huart);

/**
 * @brief Lance une nouvelle conversion de température de manière asynchrone.
 * @param[in,out] dev Pointeur vers la structure du capteur.
 * @return DS_OK si démarré, DS_ERR_BUSY si déjà en cours.
 * @note Il faut appeler DS18B20_Process régulièrement dans la boucle pour faire avancer la conversion.
 */
DS18B20_Result_t DS18B20_Start(DS18B20_t *dev);

/**
 * @brief Retourne l'état macroscopique actuel du driver.
 * @param[in] dev Pointeur vers l'instance du capteur.
 * @return L'état (READY, BUSY, ou ERROR).
 */
DS18B20_Status_t DS18B20_GetStatus(DS18B20_t *dev);

/**
 * @brief Récupère la dernière température calculée valide.
 * @param[in] dev Pointeur vers l'instance du capteur.
 * @return La température brute.
 */
int16_t DS18B20_GetTemperature(DS18B20_t *dev);

/**
 * @brief Fait avancer la machine à états. Doit être appelée le plus souvent possible.
 * @param[in,out] dev Pointeur vers l'instance du capteur.
 */
void DS18B20_Process(DS18B20_t *dev);


/* ==================================================================== */
/* CALLBACKS MATÉRIELS                                                  */
/* ==================================================================== */

/**
 * @brief À appeler dans HAL_UART_RxCpltCallback.
 * @param[in,out] hds Pointeur vers l'instance du capteur.
 */
void DS18B20_RxCpltCallback(DS18B20_t *hds);

/**
 * @brief À appeler dans HAL_UART_TxCpltCallback.
 * @param[in,out] hds Pointeur vers l'instance du capteur.
 */
void DS18B20_TxCpltCallback(DS18B20_t *hds);

#ifdef __cplusplus
}
#endif

#endif /* INC_DRIVERDS18B20_H_ */
