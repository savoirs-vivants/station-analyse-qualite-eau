/*
 * driverBLE.h
 *
 * Driver non-bloquant pour module Bluetooth Microchip (RN487x/RN4678)
 * Mode Transparent UART
 */

#ifndef DRIVER_BLE_H_
#define DRIVER_BLE_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32u0xx_hal.h"
#include "main.h"          // Pour avoir accès aux #define des broches (EN_LS_BLE_Pin)

/* --- Gestion du Débogage --- */
#define BLE_DEBUG_ENABLE 1 // Mettre à 0 pour désactiver les logs en production

#if BLE_DEBUG_ENABLE
    #include <stdio.h>
    #define BLE_PRINT(...) printf("[BLE] " __VA_ARGS__)
#else
    #define BLE_PRINT(...) // Remplace par du vide si désactivé
#endif

/* --- Enumérations --- */
typedef enum {
    BLE_STATE_IDLE,
    BLE_STATE_START,
	BLE_STATE_CONFIGURING,
	BLE_STATE_CONFIGURING_WAIT,
    BLE_STATE_WAIT_CMD,
	BLE_STATE_STREAMING,
	BLE_STATE_STREAMING_WAIT,
    BLE_STATE_WAIT_ACK,
    BLE_STATE_STOP,
	BLE_STATE_ERROR
} BLE_State_t;

typedef enum {
    BLE_STATUS_READY,
    BLE_STATUS_BUSY,
    BLE_STATUS_ERROR,
	BLE_STATUS_CONNECTED,
	BLE_STATUS_STREAMING,
	BLE_STATUS_CONFIGURING
} BLE_Status_t;

typedef enum {
	BLE_OK,
	BLE_ERR_PARAM,
	BLE_ERR_HW,
	BLE_BUSY
} BLE_Result_t;

/* --- Structure du Driver --- */
typedef struct {
    UART_HandleTypeDef *huart;
    BLE_State_t state;
    BLE_Status_t status;

    uint32_t timer_start;;

    volatile bool rx_flag;
    volatile bool tx_flag;
    uint8_t rx_buffer[255]; // TODO : A mettre dasn un define
    uint16_t rx_size;
    uint8_t config_step;
    uint8_t retry_count;

    char tx_buffer[256]; // Buffer de données à envoyer
} BLE_t;

/* --- Prototypes --- */
void BLE_Init(BLE_t *hble, UART_HandleTypeDef *huart);
BLE_Result_t BLE_Configure(BLE_t *hble);
BLE_Result_t BLE_Start(BLE_t *hble);
void BLE_Process(BLE_t *hble);
void BLE_RxCallback(BLE_t *hble);

// Setters & Getters
BLE_Result_t BLE_SendChunk(BLE_t *hble, const char *chunk);
BLE_Result_t BLE_EndStream(BLE_t *hble);
BLE_Status_t BLE_GetStatus(BLE_t *hble);
void BLE_Sleep(BLE_t *hble);
void BLE_WakeUp(BLE_t *hble);

/* --- Callbacks pour les interruptions --- */
void BLE_RxEventCallback(BLE_t *hble, uint16_t Size);
void BLE_TxCpltCallback(BLE_t *hble);

#endif /* DRIVER_BLE_H_ */
