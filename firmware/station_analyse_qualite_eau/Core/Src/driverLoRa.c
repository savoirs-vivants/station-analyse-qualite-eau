#include "driverLoRa.h"
#include <string.h>
#include <stdio.h>

void LoRa_Init(LoRa_t *hlora, UART_HandleTypeDef *huart) {
    hlora->huart = huart;
    hlora->state = LORA_STATE_IDLE;
    memset(hlora->rx_buffer, 0, LORA_RX_BUFFER_SIZE);
    hlora->rx_length = 0;
    hlora->cmd_start_time = 0;
    hlora->event_rx_complete = false;
    hlora->event_tx_complete = false;
}

void LoRa_ResetState(LoRa_t *hlora) {
    hlora->state = LORA_STATE_IDLE;
}

bool LoRa_SendCmdAsync(LoRa_t *hlora, const char *cmd, uint32_t timeout_ms) {
    if (hlora->state != LORA_STATE_IDLE) {
        return false;
    }

    memset(hlora->rx_buffer, 0, LORA_RX_BUFFER_SIZE);
    hlora->rx_length = 0;

    hlora->event_tx_complete = false;
    hlora->event_rx_complete = false;

    hlora->current_timeout_ms = timeout_ms;
    hlora->cmd_start_time = HAL_GetTick();
    hlora->state = LORA_STATE_TX; // Démarrage de la FSM

    HAL_UARTEx_ReceiveToIdle_DMA(hlora->huart, hlora->rx_buffer, LORA_RX_BUFFER_SIZE); // INT lorsque la ligne est Idle
    HAL_UART_Transmit_IT(hlora->huart, (const uint8_t*)cmd, strlen(cmd));

    return true;
}

void LoRa_Process(LoRa_t *hlora) {
    switch (hlora->state) {
    	case LORA_STATE_IDLE:
    		break;
        case LORA_STATE_TX:
            if (hlora->event_tx_complete) {
                hlora->event_tx_complete = false;
                hlora->state = LORA_STATE_RX_WAIT;
            }
            break;

        case LORA_STATE_RX_WAIT:
            if (hlora->event_rx_complete) {
                hlora->event_rx_complete = false;
                hlora->state = LORA_STATE_ANALYSIS;
            }
            else if ((HAL_GetTick() - hlora->cmd_start_time) > hlora->current_timeout_ms) {
                HAL_UART_AbortReceive_IT(hlora->huart);
                hlora->state = LORA_STATE_TIMEOUT;
            }
            break;

        case LORA_STATE_ANALYSIS:
			hlora->rx_buffer[LORA_RX_BUFFER_SIZE - 1] = '\0';

			char *text_to_analyze = (char*)hlora->rx_buffer;

			// Nettoyage des zéros parasites
			while ((*text_to_analyze == '\0' || *text_to_analyze == '\r' || *text_to_analyze == '\n' || *text_to_analyze == ' ')
				   && text_to_analyze < (char*)(hlora->rx_buffer + hlora->rx_length)) {
				text_to_analyze++;
			}

			if (strlen(text_to_analyze) == 0) {
				// Relance en mode "Append" (Ajout à la suite)
				HAL_UARTEx_ReceiveToIdle_DMA(hlora->huart, hlora->rx_buffer + hlora->rx_length, LORA_RX_BUFFER_SIZE - hlora->rx_length);
				hlora->state = LORA_STATE_RX_WAIT;
				break;
			}

			printf("Commande recue : %s\r\n", text_to_analyze);

			// --- 1. FILTRE DES SUCCES MAJEURS (TOUJOURS EN PREMIER !) ---
			if (strstr(text_to_analyze, "Done") != NULL ||
				strstr(text_to_analyze, "Joined already") != NULL ||
				strstr(text_to_analyze, "OK") != NULL)
			{
				hlora->state = LORA_STATE_OK;
			}

			// --- 2. FILTRE DES ERREURS FATALES ---
			else if (strstr(text_to_analyze, "ERROR") != NULL ||
					 strstr(text_to_analyze, "ERR") != NULL ||
					 strstr(text_to_analyze, "failed") != NULL ||
					 strstr(text_to_analyze, "busy") != NULL ||
					 strstr(text_to_analyze, "Please join") != NULL ||
					 strstr(text_to_analyze, "No free channel") != NULL||
					 strstr(text_to_analyze, "No band") != NULL ||
					 strstr(text_to_analyze, "Length error") != NULL)
			{
				hlora->state = LORA_STATE_ERROR;
			}

			// --- 3. FILTRE DES ETATS INTERMEDIAIRES ---
			else if (strstr(text_to_analyze, "Start") != NULL ||
					 strstr(text_to_analyze, "NORMAL") != NULL ||
					 strstr(text_to_analyze, "Wait ACK") != NULL ||
					 strstr(text_to_analyze, "NetID") != NULL ||
					 strstr(text_to_analyze, "RXWIN") != NULL)
			{
				// Protection anti-débordement
				if (hlora->rx_length < LORA_RX_BUFFER_SIZE - 10) {
					// On relance le DMA LÀ OÙ IL S'EST ARRÊTÉ, pas au début !
					HAL_UARTEx_ReceiveToIdle_DMA(hlora->huart, hlora->rx_buffer + hlora->rx_length, LORA_RX_BUFFER_SIZE - hlora->rx_length);
					hlora->state = LORA_STATE_RX_WAIT;
				} else {
					hlora->state = LORA_STATE_ERROR; // Buffer saturé
				}
			}

			// --- 4. COMMANDES SIMPLES (Ex: "+MODE: LWOTAA") ---
			else
			{
				hlora->state = LORA_STATE_OK;
			}
			break;

        default:
            break;
    }
}

void LoRa_TxCallback(LoRa_t *hlora) {
    hlora->event_tx_complete = true;
}

void LoRa_RxEventCallback(LoRa_t *hlora, uint16_t Size) {
    // Au lieu de hlora->rx_length = Size;
    hlora->rx_length += Size; // On accumule les morceaux de buffer !
    hlora->event_rx_complete = true;
}
