#ifndef DRIVER_LORA_H
#define DRIVER_LORA_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32u0xx_hal.h" // Seul include nécessaire pour connaître UART_HandleTypeDef

#define LORA_RX_BUFFER_SIZE 128
#define LORA_CMD_TIMEOUT_MS 2000

typedef enum {
    LORA_STATE_IDLE,
    LORA_STATE_TX,
    LORA_STATE_RX_WAIT,
    LORA_STATE_ANALYSIS,
    LORA_STATE_OK,
    LORA_STATE_ERROR,
    LORA_STATE_TIMEOUT
} LoRa_State_t;

typedef struct {
    UART_HandleTypeDef *huart;
    LoRa_State_t state;
    uint8_t rx_buffer[LORA_RX_BUFFER_SIZE];
    uint16_t rx_length;
    uint32_t cmd_start_time;
    uint32_t current_timeout_ms;

    // --- NOUVEAU : Drapeaux d'événements (levés par les IT) ---
    volatile bool event_tx_complete;
    volatile bool event_rx_complete;
} LoRa_t;

// --- Fonctions publiques (qui prennent toutes le Handle en paramètre) ---
void LoRa_Init(LoRa_t *hlora, UART_HandleTypeDef *huart);
bool LoRa_SendCmdAsync(LoRa_t *hlora, const char *cmd, uint32_t timeout_ms);
void LoRa_ResetState(LoRa_t *hlora);

void LoRa_Process(LoRa_t *hlora);

// Fonctions à appeler dans les callbacks du main.c
void LoRa_TxCallback(LoRa_t *hlora);
void LoRa_RxEventCallback(LoRa_t *hlora, uint16_t Size);

#endif // DRIVER_LORA_H
