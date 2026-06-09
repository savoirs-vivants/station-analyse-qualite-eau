/*
 * app_main_test.c
 * Station qualite d'eau - Orchestration principale
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app_main_test.h"
#include "usart.h"
#include "main.h"
#include "rtc.h"
#include "adc.h"
#include "spi.h"
#include "lptim.h"
#include "dma.h"

#include "driverDS18B20.h"
#include "driverLPPCayenne.h"
#include "driverAnalog.h"
#include "driverBLE.h"
#include "driverFRAM.h"
#include "driverDataLog.h"
#include "driverLoRaWAN.h"
#include "driverRTC.h"

/* === TESTS AU DEMARRAGE === */
#define ENABLE_DATALOG_TESTS   0
#if ENABLE_DATALOG_TESTS
#include "test_datalog.h"
#endif

#define ENABLE_LORAWAN_TESTS   0
#if ENABLE_LORAWAN_TESTS
#include "test_lorawan.h"
#endif

#define ENABLE_DS18B20_TESTS   0
#if ENABLE_DS18B20_TESTS
#include "test_ds18b20.h"
#endif

#define ENABLE_FRAM_TESTS   0
#if ENABLE_FRAM_TESTS
#include "test_fram.h"
#endif

#define ENABLE_ANALOG_TESTS   0
#if ENABLE_ANALOG_TESTS
#include "test_analog.h"
#endif

#define ENABLE_LPP_TESTS   0
#if ENABLE_LPP_TESTS
#include "test_lpp.h"
#endif

#define ENABLE_BLE_TESTS   0
#if ENABLE_BLE_TESTS
#include "test_ble.h"
#endif

extern void SystemClock_Config(void);
extern COM_InitTypeDef BspCOMInit;

#define BLE_PARSING_TIMER 25

/* ==========================================================================
 * TYPES APPLICATIFS
 * ========================================================================== */

typedef struct __attribute__((packed)) { // packed pour être stocké en mémoire compacté, à la suite
    uint32_t timestamp;
    int16_t  temperature;   	/* 1/16 °C (donc décallage de 4 bits vers la droite) */ // température de l'eau
    int16_t  turbidity;    		/* mV */ // TODO :
    int16_t  conductivity;    	/* mV */ // On pourrait les passer éventuelement en non signé comme on ne devrait jamais avoir de tensions négatives.
    uint8_t  status;
    uint8_t  _pad;
} MyRecord_t;						 // TODO : ajouter quand les capteurs seront pris en chagre --> temp air, hum air, hauteur, débit
									 // /!\ à bien modifier les valeurs dans le driver datalogger pour adapter la taille de la trame

_Static_assert(sizeof(MyRecord_t) == DATALOG_USER_DATA_SIZE, "Mauvaise taille");

typedef enum {
    APP_STATE_INIT_LORA,
    APP_STATE_WAIT_MEASURE,
    APP_STATE_SAVE_RECORD,
    APP_STATE_WAIT_SAVE,
    APP_STATE_SEND_LORA,
    APP_STATE_WAIT_TX,
	APP_STATE_WAIT_LORA_SLEEP,
    APP_STATE_SLEEP,
    APP_STATE_BLE_ACTIVE
} MainApp_State_t;

/* ==========================================================================
 * HANDLES GLOBAUX
 * ========================================================================== */

static DS18B20_t          mySensor;
static LPP_Payload_t      myLpp;
static BLE_t              myBLE;
static Analog_t           myAnalog;
static FRAM_t             myFRAM;
static DataLog_t          myLog;
static LoRaWAN_t          myLoRa;

/* ==========================================================================
 * ETAT APPLICATIF
 * ========================================================================== */

static MainApp_State_t    current_app_state = APP_STATE_INIT_LORA;
static volatile bool      flag_demande_ble  = false;
static uint32_t           ble_last_sent_id  = 0;
static uint32_t TO_Mesure = 10000;
static uint32_t TO_Last   = 0;

RTC_TimeTypeDef sTime = {0};
RTC_DateTypeDef sDate = {0};
RTC_TimeTypeDef sTime_read;
RTC_DateTypeDef sDate_read;

void startMeasures(void);

/* ==========================================================================
 * SETUP
 * ========================================================================== */

void setup(void)
{
    uint32_t uid[3];

    // Récupération des 3 blocs de 32 bits formant l'ID unique de 96 bits
    uid[0] = HAL_GetUIDw0();
    uid[1] = HAL_GetUIDw1();
    uid[2] = HAL_GetUIDw2();
    printf("UID STM32 : %08lX%08lX%08lX\r\n", uid[0], uid[1], uid[2]);

    printf(" --- Configuration RTC et Timestamp\r\n");
    configRTC(&hrtc, &sTime, &sDate);

    /* --- 1. Alimentations perifs externes --- */
    HAL_GPIO_WritePin(EN_LS_C5V_GPIO_Port, EN_LS_C5V_Pin, SET);
    HAL_GPIO_WritePin(EN_LS_LoRa_GPIO_Port, EN_LS_LoRa_Pin, SET);

    HAL_GPIO_WritePin(EN_LS_BLE_GPIO_Port, EN_LS_BLE_Pin, RESET);
    HAL_Delay(2000);
    HAL_GPIO_WritePin(EN_LS_BLE_GPIO_Port, EN_LS_BLE_Pin, SET);

    HAL_Delay(5000); /* Temps de stabilisation des regulateurs */

    /* --- 2. Init drivers bas niveau --- */
    DS18B20_Init(&mySensor, &hlpuart2);
    LPP_Init(&myLpp);
    Analog_Init(&myAnalog, &hadc1);
    BLE_Init(&myBLE, &huart3);
    FRAM_Init(&myFRAM, &hspi3, SPI3_CS0_GPIO_Port, SPI3_CS0_Pin);
    LoRaWAN_Init(&myLoRa, &hlpuart1);

    /* --- 3. Tests unitaires optionnels --- */
#if ENABLE_FRAM_TESTS
    if (!TestFRAM_RunAll(&myFRAM)) printf("\r\n[!!!] TESTS FRAM ECHOUES\r\n\r\n");
    HAL_Delay(1000);
#endif
#if ENABLE_DATALOG_TESTS
    if (!TestDataLog_RunAll(&myFRAM)) printf("\r\n[!!!] TESTS DATALOG ECHOUES\r\n\r\n");
    HAL_Delay(1000);
#endif
#if ENABLE_LORAWAN_TESTS
    if (!TestLoRaWAN_RunAll(&myLoRa)) printf("\r\n[!!!] TESTS LORAWAN ECHOUES\r\n\r\n");
    HAL_Delay(1000);
#endif
#if ENABLE_DS18B20_TESTS
    if (!TestDS18B20_RunAll(&mySensor)) printf("\r\n[!!!] TESTS DS18B20 ECHOUES\r\n\r\n");
    HAL_Delay(1000);
#endif
#if ENABLE_ANALOG_TESTS
    if (!TestAnalog_RunAll(&myAnalog)) printf("\r\n[!!!] TESTS ANALOG ECHOUES\r\n\r\n");
    HAL_Delay(1000);
#endif
#if ENABLE_LPP_TESTS
    if (!TestLPP_RunAll()) printf("\r\n[!!!] TESTS LPP ECHOUES\r\n\r\n");
    HAL_Delay(1000);
#endif
#if ENABLE_BLE_TESTS
    if (!TestBLE_RunAll(&myBLE)) printf("\r\n[!!!] TESTS BLE ECHOUES\r\n\r\n");
    HAL_Delay(1000);
#endif


    /* Lanement de la configuration LoRaWAN */
    printf("Configuration LoRaWAN...\r\n");
    LoRaWAN_Configure(&myLoRa);
    while (LoRaWAN_GetStatus(&myLoRa) != LORA_STATUS_READY) LoRaWAN_Process(&myLoRa);


    /* Lancement du Join LoRaWAN*/
    printf("Join LoRaWAN...\r\n");
    LoRaWAN_Join(&myLoRa);
    while (LoRaWAN_GetStatus(&myLoRa) != LORA_STATUS_READY) {
        LoRaWAN_Process(&myLoRa);
        if (LoRaWAN_GetStatus(&myLoRa) == LORA_STATUS_ERROR) {
        	printf("[MAIN] Erreur lors du Join LoRa ! \r\n");
        	break;
        }
    }

    /* Lanement de la configuration BLE */
	printf("Configuration BLE...\r\n");
	BLE_Configure(&myBLE);
	while (BLE_GetStatus(&myBLE) != BLE_STATUS_READY) BLE_Process(&myBLE);
    HAL_GPIO_WritePin(EN_LS_BLE_GPIO_Port, EN_LS_BLE_Pin, RESET);




    /* --- 4. Init DataLog --- */
    DataLog_Init(&myLog, &myFRAM);
    /* On attend que le DataLog ait fini de scanner la mémoire (statut READY) */
    while (DataLog_GetStatus(&myLog) != DATALOG_STATUS_READY)
    {
        FRAM_Process(&myFRAM);
        DataLog_Process(&myLog);
    }
    printf("DataLog pret : %lu records, dernier ID = %lu\r\n",
           DataLog_GetRecordCount(&myLog),
           DataLog_GetNewestId(&myLog));

    /*
    printf("RESET METa\r\n");
    if (DataLog_Format(&myLog) == DATALOG_OK) {
        printf("Formatage demande...\r\n");
    }
    while (DataLog_GetStatus(&myLog) != DATALOG_STATUS_READY)
    {
        FRAM_Process(&myFRAM);
        DataLog_Process(&myLog);
    }
    */

    /* --- 5. Config low-power --- */
    __HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_MSI);

    /* --- 6. Premiere mesure --- */
    startMeasures();
}


/* ==========================================================================
 * LOOP - FSM principale
 * ========================================================================== */

void loop(void)
{
    /* Process de tous les drivers (non-bloquant) */
    DS18B20_Process(&mySensor);
    LoRaWAN_Process(&myLoRa);
    Analog_Process(&myAnalog);
    BLE_Process(&myBLE);
    FRAM_Process(&myFRAM);
    DataLog_Process(&myLog);

    /* Priorite bouton BLE (centralisee) */
    if (flag_demande_ble && (current_app_state != APP_STATE_BLE_ACTIVE)) {
    	bool is_critical_state = (current_app_state == APP_STATE_SAVE_RECORD) ||
    	                         (current_app_state == APP_STATE_WAIT_SAVE)   ||
    	                         (current_app_state == APP_STATE_SEND_LORA)   ||
    	                         (current_app_state == APP_STATE_WAIT_TX);
        if(!is_critical_state) {
    	flag_demande_ble = false;
        printf("\r\n[!] APPUI BOUTON : Priorite BLE !\r\n");
        current_app_state = APP_STATE_BLE_ACTIVE;
        }
    }

    switch (current_app_state)
    {
        case APP_STATE_INIT_LORA: // TODO : presqeu accessoire vu que on le fait lej oin bloquant une seule fois au début... (à reconvertir en vérificarion de join ou supp ?)
        {
            LoRa_Status_t lora_status = LoRaWAN_GetStatus(&myLoRa);

            /* On check uniquement le statut public READY ou ERROR ! */
            if (lora_status == LORA_STATUS_READY) {
                printf("--- LoRaWAN Pret ---\r\n");
                TO_Last = HAL_GetTick();
                current_app_state = APP_STATE_WAIT_MEASURE;
            }
            else if (lora_status == LORA_STATUS_ERROR) {
                printf("--- Erreur Join LoRaWAN, on continue sans radio pour ce cycle ---\r\n");
                LoRaWAN_ResetError(&myLoRa); /* On acquitte l'erreur */
                TO_Last = HAL_GetTick();
                current_app_state = APP_STATE_WAIT_MEASURE;
            }
            break;
        }

        case APP_STATE_WAIT_MEASURE:
            if (DS18B20_GetStatus(&mySensor) == DS_STATUS_READY &&
                Analog_GetStatus(&myAnalog)  == ANALOG_STATUS_READY)
            {
                printf("--- Mesures OK ---\r\n");
                current_app_state = APP_STATE_SAVE_RECORD;
                HAL_GPIO_WritePin(EN_LS_C5V_GPIO_Port, EN_LS_C5V_Pin, RESET); // Coupe l'alim 5V
            }
            else if ((HAL_GetTick() - TO_Last) > TO_Mesure)
            {
                TO_Last = HAL_GetTick();
                printf("--- TO MESURE !!!! ---\r\n");
                current_app_state = APP_STATE_SAVE_RECORD;
                HAL_GPIO_WritePin(EN_LS_C5V_GPIO_Port, EN_LS_C5V_Pin, RESET); // Coupe l'alim 5V
            }
            break;

        case APP_STATE_SAVE_RECORD:
        {
        	printf("Timestamp : %lu\r\n", Get_UNIX_Timestamp(&sTime_read, &sDate_read));

        	/* Construction du record a sauvegarder en RAM/FRAM */
        	static MyRecord_t rec;
        	rec.timestamp    = Get_UNIX_Timestamp(&sTime_read, &sDate_read);
        	rec.temperature  = DS18B20_GetTemperature(&mySensor);
        	rec.turbidity    = (int16_t)(myAnalog.VOLT_values[1] * 1000.0f);
        	rec.conductivity = (int16_t)(myAnalog.VOLT_values[0] * 1000.0f);
        	rec.status       = 0;
        	rec._pad         = 0;

            if (DataLog_Write(&myLog, &rec) == DATALOG_OK) {
                current_app_state = APP_STATE_WAIT_SAVE;
            } else {
            	printf("[MAIN] Impossible de souvegarder les données ! \r\n");
            }
            break;
        }

        case APP_STATE_WAIT_SAVE:
            /* On a dé-commenté la sécurité du DataLog ! */
            if (DataLog_GetStatus(&myLog) == DATALOG_STATUS_READY) {
                printf("--- Record sauve (id=%lu) ---\r\n", DataLog_GetNewestId(&myLog));
                current_app_state = APP_STATE_SEND_LORA;
            }
            break;

        case APP_STATE_SEND_LORA:
            if (LoRaWAN_GetStatus(&myLoRa) == LORA_STATUS_READY)
            {
                LPP_Init(&myLpp);
                /* Mesures Capteurs */
                LPP_AddTemperature(&myLpp, 0, (float)DS18B20_GetTemperature(&mySensor) / 16.0); // 0 : Temp Eau
                LPP_AddAnalogInput(&myLpp, 0, myAnalog.VOLT_values[1]); // 0 : Turbidite
                LPP_AddAnalogInput(&myLpp, 1, myAnalog.VOLT_values[0]); // 1 : Conductivite
                //LPP_AddAnalogInput(&myLpp, 2, hauteur); // 2 : Hauteur
                //LPP_AddAnalogInput(&myLpp, 3, debit); // 3 : Debit
                //LPP_AddAnalogInput(&myLpp, 4, humidite); // 4 : Humidite
                //LPP_AddTemperature(&myLpp, 1, (float)myAnalog.internal_temp);          // Température carte mère // 1 : Temp Air ou DHT22
                ////LPP_AddAnalogInput(&myLpp, 5, (float)myAnalog.true_vdda_mv / 1000.0f); // 5 ? : Tension Batterie en Volts

                char payload_hex[120];
                LPP_GetPayloadHex(&myLpp, payload_hex, sizeof(payload_hex));

                printf("[MAIN] Pyload LoRa : %s\r\n", payload_hex);

                static LoRa_Result_t result_lora;
                result_lora = LoRaWAN_SendHex(&myLoRa, payload_hex);
                if (result_lora != LORA_OK) {// TODO : si il y a une erreur de join on refait la procedure ?
                	printf("[MAIN] LoRa pas OK, code %d\r\n", result_lora);
                }
                current_app_state = APP_STATE_WAIT_TX;
            }
            else if (LoRaWAN_GetStatus(&myLoRa) == LORA_STATUS_ERROR) {

            	printf("[MAIN] LoRa en Erreur, on skip l'envoi et dodo\r\n");
                current_app_state = APP_STATE_SLEEP;
            }
            break;

        case APP_STATE_WAIT_TX:
        {
            LoRa_Status_t lora_status = LoRaWAN_GetStatus(&myLoRa);

            /* Attente d'un statut définitif (Succès ou Echec) */
            if (lora_status == LORA_STATUS_READY || lora_status == LORA_STATUS_ERROR)
            {
                if (lora_status == LORA_STATUS_READY) {
                    printf("[MAIN] >>> LoRa OK\r\n");
                } else {
                    printf("[MAIN] >>> LoRa FAIL - Dodo quand meme\r\n");
                    LoRaWAN_ResetError(&myLoRa);
                }

                LoRaWAN_Sleep(&myLoRa);
                current_app_state = APP_STATE_WAIT_LORA_SLEEP;
            }
            break;
        }

        case APP_STATE_BLE_ACTIVE:
		{
			/* Sous-machine d'état (locale à ce case) */
			typedef enum {
				SUB_BLE_INIT = 0,
				SUB_BLE_WAIT_PHONE,
				SUB_BLE_SEND_UID,
				SUB_BLE_SEND_DATA,
				SUB_BLE_WAIT_READ,
				SUB_BLE_SEND_EOF,
				SUB_BLE_WAIT_FINISH
			} Sub_BLE_State_t;

			static Sub_BLE_State_t sub_state = SUB_BLE_INIT;

			static MyRecord_t out;
			static uint32_t id;
			static char chunk_buffer[64];
			static bool has_data_to_send = false;

			static uint32_t ble_pacing_timer = 0;

			switch (sub_state)
			{
				case SUB_BLE_INIT:
					if (BLE_GetStatus(&myBLE) == BLE_STATUS_READY) {
						BLE_Start(&myBLE);
						sub_state = SUB_BLE_WAIT_PHONE;
						printf("[MAIN] Attente de la commande Start '?'\r\n");

					    HAL_GPIO_WritePin(EN_LS_BLE_GPIO_Port, EN_LS_BLE_Pin, SET); // A voir si c'est bon niveau timing

					}
					break;

				case SUB_BLE_WAIT_PHONE:
					/* Attente de '?' (Le driver passera en STREAMING) */
					if (BLE_GetStatus(&myBLE) == BLE_STATUS_STREAMING) {
						DataLog_SeekOldest(&myLog);
						printf("Streaming BLE : %lu records a envoyer\r\n", DataLog_GetRecordCount(&myLog));
						sub_state = SUB_BLE_SEND_UID;
					}
					else if (BLE_GetStatus(&myBLE) == BLE_STATUS_READY) {
						// TODO : Ajouter un TimeOut
					}
					break;

				case SUB_BLE_SEND_UID:
				{
					static char uid_chunk[64];
					snprintf(uid_chunk, sizeof(uid_chunk), "UID:%08lX%08lX%08lX\r\n",
							 HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());

					if (BLE_SendChunk(&myBLE, uid_chunk) == BLE_OK) {
						has_data_to_send = false;
						ble_pacing_timer = HAL_GetTick();
						sub_state = SUB_BLE_SEND_DATA;
					}
					break;
				}

				case SUB_BLE_SEND_DATA:
				{
					if (HAL_GetTick() - ble_pacing_timer < BLE_PARSING_TIMER) break; // Parsing Timer pour ne pas envoyer trop de data en même temps pour ne pas noyer le module BLE.
					/* Étape A : Si on n'a rien en attente, on lit la FRAM */
					if (!has_data_to_send)
					{
						DataLog_ReadResult_t r = DataLog_ReadNext(&myLog, &out, &id);

						if (r == DATALOG_READ_OK) {
							printf("ID Meas : %lu\r\n", id);
							snprintf(chunk_buffer, sizeof(chunk_buffer), "%lu,%d,%d,%d,%d,%d,%d,%d\r\n",
									 (unsigned long)out.timestamp,	// ts
									 out.turbidity,					// turbi
									 out.conductivity,				// conduc
									 out.temperature, 	 	 	 	// temp eau
									 (int16_t)0b1000000000000000,  	// hauteur	// TODO : Une fois les capteurs intégré, les ajouter à la trame BLE
									 (int16_t)0b1000000000000000,  	// débit
									 (int16_t)0b1000000000000000, 	// temp air
									 (int16_t)0b1000000000000000	// hum air
									 );
							has_data_to_send = true; /* On a chargé le buffer ! */
						}
						else if (r == DATALOG_READ_EOF) {
							/* Plus aucune donnée à lire */
							sub_state = SUB_BLE_SEND_EOF;
							break;
						}
					}

					/* Étape B : On essaie de vider le buffer vers l'UART */
					if (has_data_to_send)
					{
						if (BLE_SendChunk(&myBLE, chunk_buffer) == BLE_OK) {
							ble_pacing_timer = HAL_GetTick();
							ble_last_sent_id = id;
							has_data_to_send = false; /* Envoi réussi, on pourra lire le suivant */
						}
						/* Si SendChunk renvoie BLE_BUSY, on ne fait rien.
						   Au prochain tour de boucle, on retentera l'envoi du même buffer ! */
					}
					break;
				}

				case SUB_BLE_SEND_EOF:
					if (HAL_GetTick() - ble_pacing_timer < BLE_PARSING_TIMER) break;
					if (BLE_SendChunk(&myBLE, "EOF\r\n") == BLE_OK) {
						BLE_EndStream(&myBLE); /* Demande au driver d'attendre le 'Q' */
						sub_state = SUB_BLE_WAIT_FINISH;
					}
					break;

				case SUB_BLE_WAIT_FINISH:
					/* On attend que le driver reçoive 'Q' et repasse en READY (ou s'éteigne) */
					if (BLE_GetStatus(&myBLE) == BLE_STATUS_READY) {
						sub_state = SUB_BLE_INIT; /* On réarme la Sub-FSM pour le prochain appui bouton */
						current_app_state = APP_STATE_SLEEP; /* On rend la main au système principal */
						flag_demande_ble = false; // Pour ne pas que le bounce du bouton fasse plusieurs interruptions
					}
					break;
			}
			break;
		}

        case APP_STATE_WAIT_LORA_SLEEP:
        {
			LoRa_Status_t lora_status = LoRaWAN_GetStatus(&myLoRa);

			/* On attend que la commande LOWPOWER soit validée (SLEEP)
			   ou qu'elle échoue en timeout (ERROR) */
			if (lora_status == LORA_STATUS_SLEEP || lora_status == LORA_STATUS_ERROR) {
				printf(">>> LoRa endormi.\r\n");
				current_app_state = APP_STATE_SLEEP;
			}
			break;
		}

        case APP_STATE_SLEEP:
            printf("Dodo...\r\n");
            HAL_Delay(20); /* Laisse l'UART finir de print */

            HAL_GPIO_WritePin(EN_LS_BLE_GPIO_Port, EN_LS_BLE_Pin, RESET);

            if (flag_demande_ble) {
            	flag_demande_ble = false; // Ajout ?
                HAL_GPIO_WritePin(EN_LS_BLE_GPIO_Port, EN_LS_BLE_Pin, SET);
                current_app_state = APP_STATE_BLE_ACTIVE;
                break;
            }

            HAL_ADC_Stop(&hadc1);
            HAL_ADC_DeInit(&hadc1);
            HAL_SPI_DeInit(&hspi3);
            BSP_COM_DeInit(COM1);
            HAL_UART_DeInit(&huart3);
            HAL_UART_DeInit(&huart4);
            HAL_UART_DeInit(&hlpuart1);
            HAL_UART_DeInit(&hlpuart2);
            HAL_UART_DeInit(&hlpuart3);


            GPIO_InitTypeDef GPIO_InitStruct = {0};

            /* ==========================================================
             * ETAPE 1 : BROCHES STANDARDS (3.3V) -> MODE ANALOGIQUE
             * ========================================================== */
            GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
            GPIO_InitStruct.Pull = GPIO_NOPULL;

            /* PORT A : Capteur 1 (PA0, PA1) + Console VCP (PA2, PA3) */
            GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

            /* --- Configuration pour le PORT C --- */
            /* PC0 et PC1 (LoRa) + PC10, PC11 et PC12 (SPI3) */
            GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 |  GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
            HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

            /* --- Configuration pour le PORT B --- */
            /* PB8 et PB9 (BLE) */
            GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
            HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

            /* --- Configuration pour le PORT D --- */
			/* PD2 (CS SPI3) */
			GPIO_InitStruct.Pin = GPIO_PIN_2;
			HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

            /* ==========================================================
             * ETAPE 2 : EXCEPTION 5V (DS18B20) -> MODE INPUT (High-Z)
             * ========================================================== */
            /* PC6 (DS18B20 en half-duplex avec pull-up externe 5V) */
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;  // Mode haute impédance (important !)
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            GPIO_InitStruct.Pin = GPIO_PIN_6;
            HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);




            /* --- Zzzzz (120 secondes de sommeil) --- */
            HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 120, RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0);
            HAL_SuspendTick();
            HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);




            /* --- REVEIL --- */
            SystemClock_Config();
            HAL_ResumeTick();
            HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);


            BSP_COM_Init(COM1, &BspCOMInit);
            printf("[MAIN] >> Reveil.\r\n");

            /* Relance les périphériques */
            //MX_DMA_Init();
            MX_ADC1_Init();
            ////MX_USART3_UART_Init();
            //MX_LPTIM1_Init();

            MX_SPI3_Init();
            /* --- Il faut rendre son rôle de sortie au Chip Select de la FRAM --- */
			GPIO_InitTypeDef GPIO_InitStruct_Wakeup = {0};
			GPIO_InitStruct_Wakeup.Pin = GPIO_PIN_2;
			GPIO_InitStruct_Wakeup.Mode = GPIO_MODE_OUTPUT_PP; // On repasse en sortie !
			GPIO_InitStruct_Wakeup.Pull = GPIO_NOPULL;
			GPIO_InitStruct_Wakeup.Speed = GPIO_SPEED_FREQ_LOW;
			HAL_GPIO_Init(GPIOD, &GPIO_InitStruct_Wakeup);

			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET); // On remet le CS à l'état inactif

            MX_LPUART1_UART_Init();
            MX_LPUART2_UART_Init();
            MX_LPUART3_UART_Init();
            MX_USART4_UART_Init();


            if (flag_demande_ble) {
                printf("Reveil par bouton -> BLE\r\n");
                current_app_state = APP_STATE_BLE_ACTIVE;
                HAL_GPIO_WritePin(EN_LS_BLE_GPIO_Port, EN_LS_BLE_Pin, SET);
                HAL_UART_Init(&huart3);
                break;
            } else {
            	printf("[MAIN] Pas de reveil par bouton\r\n");
                HAL_GPIO_WritePin(EN_LS_C5V_GPIO_Port, EN_LS_C5V_Pin, SET);
                HAL_Delay(500); // Laisse le temps au 5V de se stabiliser

                /* On envoie la pichenette de réveil au LoRa */
                LoRaWAN_WakeUp(&myLoRa);

                startMeasures();
                TO_Last = HAL_GetTick();
                current_app_state = APP_STATE_WAIT_MEASURE;
            }
            break;
    }
}

/* ==========================================================================
 * FONCTIONS UTILITAIRES
 * ========================================================================== */

void startMeasures(void)
{
    DS18B20_Start(&mySensor);
    Analog_Start(&myAnalog);
}

/* ==========================================================================
 * CALLBACKS HAL
 * ========================================================================== */

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == EN_BLE_Pin) {
        //EXTI->IMR1 &= ~EN_BLE_Pin;
        //HAL_LPTIM_Counter_Start_IT(&hlptim1);
        flag_demande_ble = true;
    }
}

void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
    /*if (hlptim->Instance == LPTIM1) {
        HAL_LPTIM_Counter_Stop_IT(&hlptim1);
        if (HAL_GPIO_ReadPin(EN_BLE_GPIO_Port, EN_BLE_Pin) == GPIO_PIN_RESET) {
            flag_demande_ble = true;
        }
        __HAL_GPIO_EXTI_CLEAR_IT(EN_BLE_Pin);
        EXTI->IMR1 |= EN_BLE_Pin;
    }*/
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) { Analog_ConvCpltCallback(&myAnalog); }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == mySensor.huart->Instance) { DS18B20_RxCpltCallback(&mySensor); }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == myLoRa.huart->Instance) { LoRaWAN_RxEventCallback(&myLoRa, Size); }
    if (huart->Instance == myBLE.huart->Instance) { BLE_RxEventCallback(&myBLE, Size); }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == mySensor.huart->Instance) { DS18B20_TxCpltCallback(&mySensor); }
    if (huart->Instance == myBLE.huart->Instance) { BLE_TxCpltCallback(&myBLE); }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == myFRAM.hspi->Instance) { FRAM_Tx_Callback(&myFRAM); }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == myFRAM.hspi->Instance) { FRAM_Rx_Callback(&myFRAM); }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == myFRAM.hspi) { FRAM_Rx_Callback(&myFRAM); }
}
