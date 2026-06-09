/**
 ******************************************************************************
 * @file    driverDataLog.h
 * @brief   Pilote de Datalogger circulaire asynchrone sur mémoire FRAM.
 * @author  [Ton Nom/Projet]
 * @date    [Date actuelle]
 *
 * @details
 * Ce module implémente un système de fichiers circulaire simple et robuste.
 * Il stocke des blocs de données de taille fixe (payload utilisateur) tout en
 * ajoutant de manière transparente un identifiant unique (record_id) sur 32 bits.
 * * Le système est conçu pour être 100% non-bloquant et s'appuie sur le driver
 * asynchrone FRAM (DMA). La persistance est assurée par un bloc de métadonnées
 * enregistré au début de la mémoire.
 *
 * @par Exemple d'utilisation (Écriture) :
 * @code
 * DataLog_Init(&myLog, &myFRAM);
 * DataLog_Write(&myLog, &mes_donnees); // Enregistre la structure utilisateur
 * @endcode
 *
 * @par Exemple d'utilisation (Lecture) :
 * @code
 * DataLog_SeekOldest(&myLog); // Positionne la tête de lecture
 * uint32_t id;
 * MesDonnees_t buf;
 * * while(DataLog_ReadNext(&myLog, &buf, &id) == DATALOG_READ_OK) {
 * // Traiter les données lues...
 * }
 * @endcode
 ******************************************************************************
 */

#ifndef INC_DRIVERDATALOG_H_
#define INC_DRIVERDATALOG_H_

#include <stdint.h>
#include <stdbool.h>
#include "driverFRAM.h"

/* ==========================================================================
 * CONFIGURATION ET DIMENSIONNEMENT
 * ========================================================================== */

/** @brief Taille en octets de la structure de données de l'utilisateur (Payload) */
#define DATALOG_USER_DATA_SIZE   12U

/** @brief Mot magique utilisé pour vérifier l'intégrité de la FRAM au boot */
#define DATALOG_MAGIC            0x5A7AU

/** @brief Taille totale d'un enregistrement en mémoire (ID 32 bits + Payload) */
#define DATALOG_FRAME_SIZE       (4U + DATALOG_USER_DATA_SIZE)

/* --- Cartographie Mémoire (Memory Map) --- */
#define DATALOG_META_ADDR        0x000000U               /**< Adresse du bloc de métadonnées */
#define DATALOG_DATA_START       0x000040U               /**< Début de la zone de stockage circulaire */
#define DATALOG_DATA_END         (256U * 1024U - 1U)     /**< Fin de la mémoire FRAM (ex: 2Mbits = 256 Ko) */
#define DATALOG_DATA_ZONE_SIZE   (DATALOG_DATA_END - DATALOG_DATA_START + 1U) /**< Capacité totale de la zone circulaire */
#define DATALOG_MAX_RECORDS      (DATALOG_DATA_ZONE_SIZE / DATALOG_FRAME_SIZE) /**< Nombre maximum d'enregistrements possibles */


/* ==========================================================================
 * ÉNUMÉRATIONS APPLICATIVES
 * ========================================================================== */

/**
 * @brief Code de retour immédiat pour les appels de fonctions d'écriture/configuration.
 */
typedef enum {
    DATALOG_OK = 0,             /**< L'opération a été acceptée avec succès */
    DATALOG_ERR_PARAM,          /**< Erreur de paramètre (ex: pointeur NULL) */
    DATALOG_ERR_BUSY,           /**< Le driver est déjà occupé par une transaction */
    DATALOG_ERR_FRAM            /**< Erreur ou refus de la couche matérielle basse (FRAM) */
} DataLog_Result_t;

/**
 * @brief État opérationnel global du Datalogger.
 */
typedef enum {
    DATALOG_STATUS_UNINIT = 0,  /**< Non initialisé ou pointeur invalide */
    DATALOG_STATUS_INIT_PENDING,/**< En cours de démarrage (chargement des métadonnées) */
    DATALOG_STATUS_READY,       /**< Prêt à accepter de nouvelles commandes (Idle) */
    DATALOG_STATUS_BUSY,        /**< Transaction de lecture ou d'écriture en cours (DMA) */
    DATALOG_STATUS_ERROR        /**< Erreur critique matérielle (FRAM HS ou Timeout) */
} DataLog_Status_t;

/**
 * @brief Code de retour spécifique à la récupération des enregistrements (Read).
 */
typedef enum {
    DATALOG_READ_OK = 0,        /**< Un enregistrement a été lu et copié dans le buffer utilisateur */
    DATALOG_READ_PENDING,       /**< Lecture asynchrone en cours, la donnée n'est pas encore disponible */
    DATALOG_READ_EOF,           /**< Fin de fichier atteinte (plus de nouveaux enregistrements) */
    DATALOG_READ_ERR_PARAM,     /**< Erreur de paramètre (pointeur NULL) */
    DATALOG_READ_ERR_BUSY,      /**< Refus : Le Datalogger effectue déjà une écriture */
    DATALOG_READ_ERR_HW         /**< Erreur matérielle pendant la lecture (ex: FRAM Timeout) */
} DataLog_ReadResult_t;


/* ==========================================================================
 * STRUCTURES DE DONNÉES
 * ========================================================================== */

/**
 * @brief Structure des métadonnées stockées de manière persistante en FRAM.
 * @note  Ne jamais manipuler ces données directement depuis l'application.
 */
typedef struct {
    uint16_t magic;             /**< Mot magique d'identification (DATALOG_MAGIC) */
    uint16_t reserved;          /**< Padding pour alignement mémoire */
    uint32_t write_ptr;         /**< Adresse matérielle de la prochaine écriture */
    uint32_t newest_id;         /**< Identifiant (ID) incrémental du dernier record écrit */
    uint32_t record_count;      /**< Nombre actuel d'enregistrements valides en mémoire */
    uint32_t has_wrapped;       /**< Vaut 1 si la mémoire a fait un tour complet (aligné sur 32 bits) */
} DataLog_Meta_t;

/**
 * @brief Contexte d'exécution global du Datalogger.
 */
typedef struct {
    /* Couche matérielle */
    FRAM_t *hfram;                  /**< Handle du driver matériel bas niveau (FRAM) */

    /* État courant */
    DataLog_Meta_t meta;            /**< Copie en RAM des métadonnées persistantes */
    DataLog_Status_t status;        /**< Statut opérationnel public du Datalogger */

    /* Machine d'état (FSM) */
    uint8_t  fsm_state;             /**< État interne du processus asynchrone */
    bool     busy;                  /**< Drapeau interne d'occupation */

    /* Paramètres de la tête de lecture */
    uint32_t read_ptr;              /**< Adresse physique courante de la tête de lecture */
    uint32_t read_target_id;        /**< ID du record ciblé par la lecture courante */
    bool     read_active;           /**< Indique si une session de lecture est en cours */
    bool     read_buffer_ready;     /**< Indique qu'un record a été rapatrié en RAM par la FRAM */

    /* Buffers internes DMA (Alignement mémoire strict) */
    uint8_t  write_frame[DATALOG_FRAME_SIZE]; /**< Buffer d'assemblage pour l'écriture (ID + Payload) */
    uint8_t  read_frame[DATALOG_FRAME_SIZE];  /**< Buffer de réception pour la lecture (ID + Payload) */

    /* Pointeur de transfert */
    const void *pending_src;        /**< Pointeur vers la source des données utilisateur à écrire */
} DataLog_t;


/* ==========================================================================
 * PROTOTYPES DES FONCTIONS (API PUBLIQUE)
 * ========================================================================== */

/**
 * @brief  Initialise le contexte du Datalogger et lance le chargement des métadonnées.
 * @param  hdl    Pointeur vers le handle du Datalogger.
 * @param  hfram  Pointeur vers le handle du driver FRAM déjà initialisé.
 * @retval DATALOG_OK en cas de succès de l'initialisation logicielle.
 */
DataLog_Result_t DataLog_Init(DataLog_t *hdl, FRAM_t *hfram);

/**
 * @brief  Machine d'état (FSM) du Datalogger.
 * @note   Doit être appelée le plus souvent possible dans la boucle principale (main/while(1)).
 * @param  hdl Pointeur vers le handle du Datalogger.
 */
void DataLog_Process(DataLog_t *hdl);

/**
 * @brief  Retourne le statut opérationnel actuel du Datalogger.
 * @param  hdl Pointeur vers le handle du Datalogger.
 * @return DATALOG_STATUS_READY, BUSY, ERROR, ou INIT_PENDING.
 */
DataLog_Status_t DataLog_GetStatus(DataLog_t *hdl);

/**
 * @brief  Demande l'écriture asynchrone d'un nouvel enregistrement en mémoire.
 * @param  hdl       Pointeur vers le handle du Datalogger.
 * @param  user_data Pointeur vers la structure de données utilisateur à sauvegarder.
 * @retval DATALOG_OK si la transaction est acceptée, sinon ERR_BUSY ou ERR_PARAM.
 */
DataLog_Result_t DataLog_Write(DataLog_t *hdl, const void *user_data);

/**
 * @brief  Positionne la tête de lecture sur le plus vieil enregistrement non écrasé.
 * @param  hdl Pointeur vers le handle du Datalogger.
 * @retval DATALOG_OK si accepté, sinon ERR_BUSY si le Datalogger travaille déjà.
 */
DataLog_Result_t DataLog_SeekOldest(DataLog_t *hdl);

/**
 * @brief  Positionne la tête de lecture sur un enregistrement spécifique via son ID.
 * @note   Si l'ID demandé a déjà été écrasé (trop vieux), la tête se positionnera
 * automatiquement sur l'ID disponible le plus ancien.
 * @param  hdl       Pointeur vers le handle du Datalogger.
 * @param  record_id L'identifiant 32 bits de l'enregistrement ciblé.
 * @retval DATALOG_OK si accepté, sinon ERR_BUSY ou ERR_PARAM.
 */
DataLog_Result_t DataLog_SeekById(DataLog_t *hdl, uint32_t record_id);

/**
 * @brief  Tente de lire l'enregistrement pointé par la tête de lecture et avance le curseur.
 * @param  hdl           Pointeur vers le handle du Datalogger.
 * @param  user_buffer   Buffer où les données utiles seront copiées (doit faire DATALOG_USER_DATA_SIZE).
 * @param  out_record_id Pointeur (optionnel, peut être NULL) pour récupérer l'ID de la donnée lue.
 * @return DATALOG_READ_OK si réussi, PENDING s'il faut attendre, EOF si plus de données à lire.
 */
DataLog_ReadResult_t DataLog_ReadNext(DataLog_t *hdl, void *user_buffer, uint32_t *out_record_id);


DataLog_Result_t DataLog_Format(DataLog_t *hdl);

/* ==========================================================================
 * ACCESSEURS (GETTERS)
 * ========================================================================== */

/**
 * @brief  Renvoie l'ID du tout dernier enregistrement sauvegardé en mémoire.
 * @param  hdl Pointeur vers le handle du Datalogger.
 * @return L'ID du record le plus récent (0 si la mémoire est vide ou pointeur NULL).
 */
uint32_t DataLog_GetNewestId(DataLog_t *hdl);

/**
 * @brief  Renvoie l'ID du plus vieil enregistrement encore disponible en mémoire.
 * @param  hdl Pointeur vers le handle du Datalogger.
 * @return L'ID du plus vieux record (0 si la mémoire est vide ou pointeur NULL).
 */
uint32_t DataLog_GetOldestId(DataLog_t *hdl);

/**
 * @brief  Renvoie le nombre total d'enregistrements valides actuellement en mémoire.
 * @param  hdl Pointeur vers le handle du Datalogger.
 * @return Le nombre d'enregistrements (0 si mémoire vide, jusqu'à DATALOG_MAX_RECORDS).
 */
uint32_t DataLog_GetRecordCount(DataLog_t *hdl);

#endif /* INC_DRIVERDATALOG_H_ */
