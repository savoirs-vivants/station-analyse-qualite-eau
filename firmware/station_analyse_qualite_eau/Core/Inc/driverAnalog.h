/**
 ******************************************************************************
 * @file    driverAnalog.h
 * @brief   Pilote d'acquisition analogique (ADC) asynchrone avec monitoring.
 * @author  [Ton Nom/Projet]
 * @date    [Date actuelle]
 *
 * @details
 * Ce module gère l'acquisition de capteurs analogiques externes et internes
 * (Température MCU, VREFINT, VBAT) via le DMA du STM32 pour un fonctionnement
 * 100% non-bloquant.
 * * Il intègre une compensation dynamique de la tension d'alimentation (VDDA)
 * basée sur la lecture de la référence interne (VREFINT), garantissant des
 * mesures précises même lors de la décharge d'une batterie.
 *
 * @note L'ordre des conversions dans CubeMX doit correspondre aux index :
 * - Index 0, 1 : Capteurs externes (ex: C1, C2)
 * - Index 2    : VREFINT Channel
 * - Index 3    : Temperature Sensor Channel
 * - Index 4    : Vbat Channel
 ******************************************************************************
 */

#ifndef INC_DRIVERANALOG_H_
#define INC_DRIVERANALOG_H_

#include "main.h"
#include "stm32u0xx_hal.h"
#include <stdbool.h>

/* ==========================================================================
 * CONFIGURATION DU DRIVER
 * ========================================================================== */

/** @brief Nombre total de canaux convertis par la séquence ADC (Ranks) */
#define NBR_ANALOG_SENSOR           4U

/** @brief Coefficient multiplicateur pour compenser un pont diviseur externe */
#define ATTENUATOR_MULTIPLICATOR    2.0f

/** @brief Valeur maximale d'un ADC 12 bits (Utilisé pour éviter le conflit avec la macro HAL) */
#define ADC_MAX_RESOLUTION_12B      4095.0f


/* ==========================================================================
 * ÉNUMÉRATIONS APPLICATIVES
 * ========================================================================== */

/**
 * @brief Code de retour immédiat pour les appels de l'API.
 */
typedef enum {
    ANALOG_OK = 0,          /**< Opération acceptée avec succès */
    ANALOG_ERR_PARAM,       /**< Erreur de paramètre (ex: pointeur NULL) */
    ANALOG_ERR_BUSY,        /**< Refus : Une conversion DMA est déjà en cours */
    ANALOG_ERR_HAL          /**< Erreur retournée par la couche d'abstraction matérielle (HAL) */
} Analog_Result_t;

/**
 * @brief Statut opérationnel global du driver analogique.
 */
typedef enum {
    ANALOG_STATUS_READY = 0,/**< Driver prêt à lancer une nouvelle séquence de conversion */
    ANALOG_STATUS_BUSY,     /**< Séquence de conversion DMA et traitement mathématique en cours */
    ANALOG_STATUS_ERROR     /**< Erreur critique (ex: Timeout DMA, échec de calibration) */
} Analog_Status_t;

/**
 * @brief États internes de la machine d'état (FSM).
 * @note  À usage interne par Analog_Process().
 */
typedef enum {
    ANALOG_IDLE = 0,          /**< Repos, attente d'une commande Start */
    ANALOG_START_CONVERSION,  /**< Lancement matériel de la séquence DMA */
    ANALOG_WAIT_CONVERSION,   /**< Attente du flag de fin d'interruption DMA avec Timeout */
    ANALOG_PROCESS_DATA       /**< Calculs de compensation et conversion bits vers valeurs physiques */
} Analog_State_t;


/* ==========================================================================
 * STRUCTURES DE DONNÉES
 * ========================================================================== */

/**
 * @brief Contexte d'exécution global du driver analogique.
 */
typedef struct {
    /* --- Couche Matérielle et État --- */
    ADC_HandleTypeDef *hadc;            /**< Handle du périphérique ADC utilisé */
    volatile Analog_State_t state;      /**< État actuel de la machine d'état interne */
    Analog_Status_t status;             /**< Statut public du driver (READY/BUSY/ERROR) */

    /* --- Buffers de Données Brutes --- */
    uint16_t ADC_values[NBR_ANALOG_SENSOR]; /**< Buffer de réception DMA pour les données brutes (bits) */

    /* --- Données Physiques Calculées --- */
    float    VOLT_values[2];            /**< Tensions calculées (en Volts) pour les capteurs externes (C1, C2) */

    /* --- Monitoring Système (Calculé via macros HAL) --- */
    uint32_t true_vdda_mv;              /**< Tension d'alimentation réelle du MCU (en millivolts) estimée via VREFINT */
    int32_t  internal_temp;             /**< Température interne du silicium (en degrés Celsius) */
    uint32_t v_battery;                 /**< Tension de la batterie de l'IOT (en mV) */

    /* --- Contrôle et Sécurité --- */
    uint32_t start_time;                /**< Timestamp (ms) mémorisé au lancement pour la gestion du Timeout */
    uint8_t  error_code;                /**< Code d'erreur interne (0=Aucune, 1=Erreur Start DMA, 2=Timeout) */
    volatile bool event_adc_complete;   /**< Flag levé par l'interruption DMA lorsque le buffer est plein */
} Analog_t;


/* ==========================================================================
 * PROTOTYPES DES FONCTIONS (API PUBLIQUE)
 * ========================================================================== */

/**
 * @brief  Initialise le driver analogique et lance la calibration matérielle.
 * @note   Doit être appelée une seule fois au démarrage de l'application.
 * @param  hanalog Pointeur vers l'instance du driver Analogique.
 * @param  hadc    Pointeur vers le handle HAL de l'ADC (déjà initialisé par CubeMX).
 * @retval ANALOG_OK en cas de succès, ANALOG_ERR_HAL si la calibration échoue.
 */
Analog_Result_t Analog_Init(Analog_t *hanalog, ADC_HandleTypeDef *hadc);

/**
 * @brief  Demande le démarrage d'une nouvelle séquence de conversions.
 * @param  hanalog Pointeur vers l'instance du driver Analogique.
 * @retval ANALOG_OK si la demande est acceptée, ANALOG_ERR_BUSY si l'ADC est déjà occupé.
 */
Analog_Result_t Analog_Start(Analog_t *hanalog);

/**
 * @brief  Machine d'état (FSM) gérant la séquence DMA, les timeouts et les calculs physiques.
 * @note   Doit être appelée régulièrement dans la boucle principale (main/while(1)).
 * @param  hanalog Pointeur vers l'instance du driver Analogique.
 */
void Analog_Process(Analog_t *hanalog);

/**
 * @brief  Retourne le statut opérationnel actuel du driver.
 * @param  hanalog Pointeur vers l'instance du driver Analogique.
 * @retval ANALOG_STATUS_READY, ANALOG_STATUS_BUSY ou ANALOG_STATUS_ERROR.
 */
Analog_Status_t Analog_GetStatus(Analog_t *hanalog);

/**
 * @brief  Fonction de rappel (Callback) à insérer dans l'interruption HAL_ADC_ConvCpltCallback.
 * @note   Lève le drapeau de fin de conversion pour débloquer la machine d'état.
 * @param  hanalog Pointeur vers l'instance du driver Analogique.
 */
void Analog_ConvCpltCallback(Analog_t *hanalog);

#endif /* INC_DRIVERANALOG_H_ */
