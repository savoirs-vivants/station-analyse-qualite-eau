#include "driverDataLog.h"
#include <string.h>

/* États FSM internes (non exposés) */
enum {
    S_IDLE = 0,
    S_BOOT_READ_META, S_BOOT_WAIT_META,
    S_WRITE_RECORD,   S_WRITE_WAIT,
    S_WRITE_META,     S_WRITE_META_WAIT,
    S_READ_RECORD,    S_READ_WAIT
};

/* --- Helpers --- */
static inline bool FramReady(FRAM_t *hfram) { return (FRAM_GetStatus(hfram) == FRAM_STATUS_READY); }

static uint32_t AdvanceAddr(uint32_t addr)
{
    uint32_t next = addr + DATALOG_FRAME_SIZE;
    if ((next + DATALOG_FRAME_SIZE - 1U) > DATALOG_DATA_END) next = DATALOG_DATA_START;
    return next;
}

static void ResetMeta(DataLog_Meta_t *m)
{
    m->magic        = DATALOG_MAGIC;
    m->reserved     = 0;
    m->write_ptr    = DATALOG_DATA_START;
    m->newest_id    = 0;
    m->record_count = 0;
    m->has_wrapped  = 0;
}

static uint32_t GetOldestAddr(const DataLog_Meta_t *m)
{
    if (m->has_wrapped) { return m->write_ptr; }
    return DATALOG_DATA_START;
}

static uint32_t GetOldestId(const DataLog_Meta_t *m)
{
    if (m->record_count == 0) { return 0; }
    if (m->has_wrapped)       { return m->newest_id - DATALOG_MAX_RECORDS + 1U; }
    return m->newest_id - m->record_count + 1U;
}

/* Calcule l'adresse d'un record_id donné (prérequis : id dans [oldest, newest]) */
static uint32_t AddrForId(const DataLog_Meta_t *m, uint32_t target_id) // TODO : comprendre la fonction
{
    uint32_t offset_records = m->newest_id - target_id + 1U;
    uint32_t offset_bytes   = offset_records * DATALOG_FRAME_SIZE;
    uint32_t zone_size      = DATALOG_DATA_ZONE_SIZE;
    uint32_t write_offset   = m->write_ptr - DATALOG_DATA_START;

    uint32_t new_offset = (write_offset + zone_size - (offset_bytes % zone_size))
                          % zone_size;
    return DATALOG_DATA_START + new_offset;
}

/* --- API --- */
DataLog_Result_t DataLog_Init(DataLog_t *hdl, FRAM_t *hfram)
{
    if (hdl == NULL || hfram == NULL) return DATALOG_ERR_PARAM;

    memset(hdl, 0, sizeof(DataLog_t));
    hdl->hfram = hfram;
    hdl->fsm_state = S_BOOT_READ_META;
    hdl->status = DATALOG_STATUS_INIT_PENDING;

    return DATALOG_OK;
}

/**
 * Vérifie l'état de la FRAM.
 * Renvoie 'true' s'il faut bloquer la FSM du DataLog (BUSY ou ERROR).
 * Renvoie 'false' si la FRAM est libre et prête.
 */
static inline bool CheckFramBusyOrError(DataLog_t *hdl)
{
    FRAM_Status_t fram_status = FRAM_GetStatus(hdl->hfram);

    if (fram_status == FRAM_STATUS_ERROR) {
        hdl->status    = DATALOG_STATUS_ERROR;
        hdl->fsm_state = S_IDLE;
        hdl->busy      = false;
        return true; /* On bloque la FSM */
    }
    else if (fram_status == FRAM_STATUS_BUSY) {
        return true;
    }

    return false;
}

void DataLog_Process(DataLog_t *hdl)
{
    if (hdl->fsm_state == S_IDLE) { return; }

    switch (hdl->fsm_state)
    {
        /* --- BOOT : lecture métadonnées --- */
        case S_BOOT_READ_META:
            if (CheckFramBusyOrError(hdl)) { break; }

            if (FRAM_Read(hdl->hfram, DATALOG_META_ADDR, (uint8_t *)&hdl->meta, sizeof(DataLog_Meta_t)) == FRAM_OK) {
                hdl->fsm_state = S_BOOT_WAIT_META;
            }
            break;

        case S_BOOT_WAIT_META:
            if (CheckFramBusyOrError(hdl)) { break; }

            if (hdl->meta.magic != DATALOG_MAGIC) {
                /* FRAM vierge ou corrompue : on repart de zéro */
                ResetMeta(&hdl->meta);
            }

            hdl->status    = DATALOG_STATUS_READY; /* Le boot est un succès ! */
            hdl->fsm_state = S_IDLE;
            hdl->busy      = false;
            break;

        /* --- WRITE : record puis métadonnées --- */
        case S_WRITE_RECORD:
        {
            if (CheckFramBusyOrError(hdl)) { break; }

            uint32_t new_id = hdl->meta.newest_id + 1U;

            /* Construction trame = [id:4][user:USER_DATA_SIZE] */
            memcpy(&hdl->write_frame[0], &new_id, 4);
            memcpy(&hdl->write_frame[4], hdl->pending_src, DATALOG_USER_DATA_SIZE);

            if (FRAM_Write(hdl->hfram, hdl->meta.write_ptr, hdl->write_frame, DATALOG_FRAME_SIZE) == FRAM_OK) {
                hdl->fsm_state = S_WRITE_WAIT;
            } else {
                /* Échec synchrone immédiat (ex: erreur de pointeur/limite mémoire) */
                hdl->status    = DATALOG_STATUS_ERROR;
                hdl->fsm_state = S_IDLE;
                hdl->busy      = false;
            }
            break;
        }

        case S_WRITE_WAIT:
            if (CheckFramBusyOrError(hdl)) { break; }

            /* Mise à jour des curseurs en RAM */
            hdl->meta.newest_id++;
            if (hdl->meta.record_count < DATALOG_MAX_RECORDS) {
                hdl->meta.record_count++;
            } else {
                hdl->meta.has_wrapped = 1U;
            }
            hdl->meta.write_ptr = AdvanceAddr(hdl->meta.write_ptr);

            hdl->fsm_state = S_WRITE_META;
            break;

        case S_WRITE_META:
            if (CheckFramBusyOrError(hdl)) { break; }

            if (FRAM_Write(hdl->hfram, DATALOG_META_ADDR, (uint8_t *)&hdl->meta, sizeof(DataLog_Meta_t)) == FRAM_OK) {
                hdl->fsm_state = S_WRITE_META_WAIT;
            } else {
                hdl->status    = DATALOG_STATUS_ERROR;
                hdl->fsm_state = S_IDLE;
                hdl->busy      = false;
            }
            break;

        case S_WRITE_META_WAIT:
            if (CheckFramBusyOrError(hdl)) { break; }

            hdl->status      = DATALOG_STATUS_READY; /* Écriture terminée avec succès ! */
            hdl->fsm_state   = S_IDLE;
            hdl->busy        = false;
            hdl->pending_src = NULL;
            break;

        /* --- READ : un record --- */
        case S_READ_RECORD:
            if (CheckFramBusyOrError(hdl)) { break; }

            if (FRAM_Read(hdl->hfram, hdl->read_ptr, hdl->read_frame, DATALOG_FRAME_SIZE) == FRAM_OK) {
                hdl->fsm_state = S_READ_WAIT;
            } else {
                hdl->status    = DATALOG_STATUS_ERROR;
                hdl->fsm_state = S_IDLE;
                hdl->busy      = false;
            }
            break;

        case S_READ_WAIT:
            if (CheckFramBusyOrError(hdl)) { break; }

            hdl->read_buffer_ready = true;
            hdl->status            = DATALOG_STATUS_READY; /* Lecture terminée avec succès ! */
            hdl->fsm_state         = S_IDLE;
            hdl->busy              = false;
            break;

        default:
            hdl->fsm_state = S_IDLE;
            hdl->busy      = false;
            break;
    }
}

DataLog_Status_t DataLog_GetStatus(DataLog_t *hdl)
{    if (hdl == NULL) return DATALOG_STATUS_UNINIT;
    return hdl->status;
}

DataLog_Result_t DataLog_Write(DataLog_t *hdl, const void *user_data)
{
    if (hdl == NULL || user_data == NULL) return DATALOG_ERR_PARAM;

    if (hdl->fsm_state != S_IDLE) {
        return DATALOG_ERR_BUSY;
    }

    hdl->pending_src = user_data;

    hdl->status      = DATALOG_STATUS_BUSY;
    hdl->busy        = true;

    hdl->fsm_state   = S_WRITE_RECORD;

    return DATALOG_OK;
}

DataLog_Result_t DataLog_SeekOldest(DataLog_t *hdl)
{
    if (hdl == NULL) return DATALOG_ERR_PARAM;

    if (hdl->fsm_state != S_IDLE) { return DATALOG_ERR_BUSY; }

    hdl->read_ptr          = GetOldestAddr(&hdl->meta);
    hdl->read_target_id    = GetOldestId(&hdl->meta);

    hdl->read_active       = (hdl->meta.record_count > 0U);

    hdl->read_buffer_ready = false;

    return DATALOG_OK;
}

DataLog_Result_t DataLog_SeekById(DataLog_t *hdl, uint32_t record_id)
{
    if (hdl == NULL) { return DATALOG_ERR_PARAM; }

    if (hdl->fsm_state != S_IDLE) { return DATALOG_ERR_BUSY; }

    uint32_t oldest_id = GetOldestId(&hdl->meta);

    if (hdl->meta.record_count == 0U || record_id > hdl->meta.newest_id)
    {
        hdl->read_active = false;
        return DATALOG_OK;
    }

    if (record_id < oldest_id) {
        record_id = oldest_id;
    }

    hdl->read_ptr          = AddrForId(&hdl->meta, record_id);
    hdl->read_target_id    = record_id;
    hdl->read_active       = true;
    hdl->read_buffer_ready = false;

    return DATALOG_OK;
}

DataLog_ReadResult_t DataLog_ReadNext(DataLog_t *hdl, void *user_buffer, uint32_t *out_record_id)
{
    if (hdl == NULL || user_buffer == NULL) { return DATALOG_READ_ERR_PARAM; }

    if (hdl->status == DATALOG_STATUS_ERROR) { return DATALOG_READ_ERR_HW; }

    /* Vérification de la fin de fichier (EOF) */
    if (!hdl->read_active || hdl->read_target_id > hdl->meta.newest_id) {
        hdl->read_active = false;
        return DATALOG_READ_EOF;
    }

    if (hdl->read_buffer_ready) {
        hdl->read_buffer_ready = false;

        uint32_t id;
        memcpy(&id, &hdl->read_frame[0], 4);
        if (out_record_id != NULL) {
            *out_record_id = id;
        }

        memcpy(user_buffer, &hdl->read_frame[4], DATALOG_USER_DATA_SIZE);

        /* Avance des curseurs pour le prochain appel */
        hdl->read_ptr = AdvanceAddr(hdl->read_ptr);
        hdl->read_target_id++;

        return DATALOG_READ_OK;
    }

    /* Si la donnée n'est pas encore prête */

    if (hdl->fsm_state == S_IDLE) {
        /* Le Datalogger est libre, on lance la requête matérielle (DMA) */
        hdl->fsm_state = S_READ_RECORD;
        hdl->status    = DATALOG_STATUS_BUSY;
        hdl->busy      = true;
        return DATALOG_READ_PENDING;
    }
    else if (hdl->fsm_state == S_READ_RECORD || hdl->fsm_state == S_READ_WAIT) {
        /* La transaction de lecture a déjà démarré, on patiente */
        return DATALOG_READ_PENDING;
    }
    else {
        /* Le Datalogger est occupé à faire autre chose (ex: Write, Boot) !
           On refuse la lecture pour éviter de corrompre la FSM. */
        return DATALOG_READ_ERR_BUSY;
    }
}

uint32_t DataLog_GetNewestId(DataLog_t *hdl)
{
    if (hdl == NULL) return 0;
    return hdl->meta.newest_id;
}

uint32_t DataLog_GetOldestId(DataLog_t *hdl)
{
    if (hdl == NULL) return 0;
    return GetOldestId(&hdl->meta); /* <-- Attention au "&" pour passer le pointeur ! */
}

uint32_t DataLog_GetRecordCount(DataLog_t *hdl)
{
    if (hdl == NULL) return 0;
    return hdl->meta.record_count;
}

DataLog_Result_t DataLog_Format(DataLog_t *hdl)
{
    if (hdl == NULL) return DATALOG_ERR_PARAM;

    if (hdl->fsm_state != S_IDLE) {
        return DATALOG_ERR_BUSY;
    }

    ResetMeta(&hdl->meta);

    hdl->status    = DATALOG_STATUS_BUSY;
    hdl->busy      = true;
    hdl->fsm_state = S_WRITE_META;

    return DATALOG_OK;
}
