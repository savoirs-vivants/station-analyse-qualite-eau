#include "test_datalog.h"
#include "driverDataLog.h"
#include "driverFRAM.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/* Handle DataLog dédié aux tests (separe de celui de l'app pour ne pas
 * polluer les metadonnees de production). Le handle FRAM est partage. */
static DataLog_t testLog;
static FRAM_t   *testFRAM;   /* Pointeur vers le handle app */

typedef struct __attribute__((packed)) {
    uint32_t value;
    uint32_t magic;
    uint32_t filler;
} TestRecord_t;

_Static_assert(sizeof(TestRecord_t) == DATALOG_USER_DATA_SIZE,
               "TestRecord_t doit correspondre a DATALOG_USER_DATA_SIZE");

/* Macro d'assertion simple. */
#define EXPECT(cond, msg)                                           \
    do {                                                            \
        if (!(cond)) {                                              \
            printf("  [FAIL] %s (ligne %d)\r\n", msg, __LINE__);    \
            return false;                                           \
        } else {                                                    \
            printf("  [ OK ] %s\r\n", msg);                         \
        }                                                           \
    } while (0)

/* Macro pour comparer deux entiers et afficher leurs valeurs en cas d'échec */
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


/* --- Helper BÉTON d'attente ---
 * Attend que la FSM finisse, mais s'arrête si une erreur matérielle survient. */
static bool WaitIdle(void)
{
    uint32_t guard = HAL_GetTick() + 2000U;
    DataLog_Status_t status;

    do {
        FRAM_Process(testFRAM);
        DataLog_Process(&testLog);
        status = DataLog_GetStatus(&testLog);

        if (status == DATALOG_STATUS_ERROR) {
            printf("  [FAIL] WaitIdle : Le DataLog a detecte une erreur materielle FRAM !\r\n");
            return false;
        }

        if (HAL_GetTick() > guard) {
            printf("  [TIMEOUT] WaitIdle bloque !\r\n");
            return false;
        }
    } while (status != DATALOG_STATUS_READY);

    return true;
}


/* -------------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------------- */

/** TEST 1 : Init + état initial propre sur FRAM vierge */
static bool Test_InitBasic(void)
{
    printf("\r\n--- TEST 1 : Init ---\r\n");

    /* L'Init retourne maintenant un Result_t */
    EXPECT(DataLog_Init(&testLog, testFRAM) == DATALOG_OK, "Init acceptee");

    /* On attend la fin du Boot (lecture des métadonnées) */
    EXPECT(WaitIdle(), "Séquence de boot terminee sans erreur");

    /* Le statut doit être READY */
    EXPECT(DataLog_GetStatus(&testLog) == DATALOG_STATUS_READY, "Driver READY apres init");
    return true;
}

/** TEST 2 : Écriture / lecture simple de 3 records */
static bool Test_WriteRead(void)
{
    printf("\r\n--- TEST 2 : Write + Read ---\r\n");

    uint32_t base_id    = DataLog_GetNewestId(&testLog);
    uint32_t base_count = DataLog_GetRecordCount(&testLog);

    /* Écriture de 3 records */
    for (uint32_t i = 0; i < 3; i++)
    {
        TestRecord_t rec = {
            .value  = 0x10000000U + i,
            .magic  = 0xCAFE0000U + i,
            .filler = i
        };
        EXPECT(DataLog_Write(&testLog, &rec) == DATALOG_OK, "Write accepte par l'API");
        EXPECT(WaitIdle(), "Ecriture DMA terminee avec succes");
    }

    EXPECT(DataLog_GetNewestId(&testLog) == base_id + 3U, "newest_id avance de 3");

    /* Relecture : on se positionne sur le 1er des 3 nouveaux */
    EXPECT(DataLog_SeekById(&testLog, base_id + 1U) == DATALOG_OK, "Seek accepte par l'API");

    for (uint32_t i = 0; i < 3; i++)
    {
        TestRecord_t out;
        uint32_t id;
        DataLog_ReadResult_t r;

        uint32_t guard = HAL_GetTick() + 1000U;
        do {
            FRAM_Process(testFRAM);
            DataLog_Process(&testLog);
            r = DataLog_ReadNext(&testLog, &out, &id);

            if (r == DATALOG_READ_ERR_HW) {
                printf("  [FAIL] Erreur materielle pendant la lecture !\r\n");
                return false;
            }
            if (HAL_GetTick() > guard) {
                printf("  [TIMEOUT] ReadNext\r\n");
                return false;
            }
        } while (r == DATALOG_READ_PENDING);

        EXPECT(r == DATALOG_READ_OK, "Lecture OK");
        EXPECT_EQ(id, base_id + 1U + i, "record_id correct");
        EXPECT_EQ(out.value, 0x10000000U + i, "value correct");
        EXPECT_EQ(out.magic, 0xCAFE0000U + i, "magic correct");
    }

    /* Après les 3 lectures, on doit être à EOF */
    TestRecord_t dummy;
    uint32_t dummy_id;
    uint32_t guard = HAL_GetTick() + 500U;
    DataLog_ReadResult_t r;
    do {
        FRAM_Process(testFRAM);
        DataLog_Process(&testLog);
        r = DataLog_ReadNext(&testLog, &dummy, &dummy_id);
        if (HAL_GetTick() > guard) break;
    } while (r == DATALOG_READ_PENDING);

    EXPECT(r == DATALOG_READ_EOF, "EOF apres dernier record");

    (void)base_count;
    return true;
}

/** TEST 3 : Persistance des métadonnées. */
static bool Test_Persistence(void)
{
    printf("\r\n--- TEST 3 : Persistance metadonnees ---\r\n");

    uint32_t id_before    = DataLog_GetNewestId(&testLog);
    uint32_t count_before = DataLog_GetRecordCount(&testLog);

    memset(&testLog, 0, sizeof(testLog));

    EXPECT(DataLog_Init(&testLog, testFRAM) == DATALOG_OK, "Re-init acceptee");
    EXPECT(WaitIdle(), "Re-boot termine sans erreur");

    EXPECT_EQ(DataLog_GetNewestId(&testLog), id_before, "newest_id preserve");
    EXPECT_EQ(DataLog_GetRecordCount(&testLog), count_before, "count preserve");
    return true;
}

/** TEST 4 : SeekById sur un ID écrasé -> retour au plus vieux dispo */
static bool Test_SeekOnErasedId(void)
{
    printf("\r\n--- TEST 4 : SeekById sur ID trop vieux ---\r\n");

    uint32_t oldest = DataLog_GetOldestId(&testLog);
    if (oldest == 0U)
    {
        printf("  [SKIP] Pas encore de records ecrases\r\n");
        return true;
    }

    /* On cherche un ID qui n'existe plus */
    EXPECT(DataLog_SeekById(&testLog, oldest - 1U) == DATALOG_OK, "Seek sur vieil ID accepte");

    /* Le prochain ReadNext doit pointer sur oldest */
    TestRecord_t out;
    uint32_t id;
    uint32_t guard = HAL_GetTick() + 1000U;
    DataLog_ReadResult_t r;
    do {
        FRAM_Process(testFRAM);
        DataLog_Process(&testLog);
        r = DataLog_ReadNext(&testLog, &out, &id);
        if (HAL_GetTick() > guard) break;
    } while (r == DATALOG_READ_PENDING);

    EXPECT(r == DATALOG_READ_OK, "Lecture OK apres rabattage");
    EXPECT_EQ(id, oldest, "id rabattu sur le plus vieux disponible");
    return true;
}

/* -------------------------------------------------------------------
 * Runner principal
 * ------------------------------------------------------------------- */

bool TestDataLog_RunAll(FRAM_t *hfram)
{
    printf("\r\n========================================\r\n");
    printf("     BATTERIE DE TESTS DATALOG\r\n");
    printf("========================================\r\n");

    if (hfram == NULL) {
        printf("  [FAIL] Pointeur FRAM_t fourni est NULL !\r\n");
        return false;
    }

    testFRAM = hfram;

    if (!Test_InitBasic())       { return false; }
    if (!Test_WriteRead())       { return false; }
    if (!Test_Persistence())     { return false; }
    if (!Test_SeekOnErasedId())  { return false; }

    printf("\r\n========================================\r\n");
    printf("     TOUS LES TESTS DATALOG PASSES\r\n");
    printf("========================================\r\n\r\n");
    return true;
}
