/*
 * driverDS18B20.c
 *
 *  Created on: 30 mars 2026
 *      Author: valen
 */

#include <stdint.h>

#include "usart.h"
#include "main.h"
#include "driverDS18B20.h"

/* * Précalcul des commandes 1-Wire pour l'UART.
 * Exemple pour 0xCC (Skip ROM) : 1100 1100 en binaire
 * /!\ C'est transmis de LSB à MSB !
 * Bit 0: 0 -> 0x00
 * Bit 1: 0 -> 0x00
 * Bit 2: 1 -> 0xFF
 * ... etc.
 */
const uint8_t DS_CMD_SKIP_ROM[8]  = {0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF}; 		// 0xCC = 0b11001100
const uint8_t DS_CMD_CONVERT_T[8] = {0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00}; 		// 0x44 = 0b01000100
const uint8_t DS_CMD_READ_SCRATCH[8] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF}; 	// 0xBE = 0b10111110
const uint8_t DS_CMD_READ[72] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
								 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; 			// 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF

const uint8_t DS_CMD_RESET[1] = {0xF0};														// Reset pulse


DS18B20_Result_t DS18B20_Init(DS18B20_t * hds, UART_HandleTypeDef * huart)
{
	if (hds == NULL || huart == NULL) return DS_ERR_PARAM; 	// Vérification des paramètres

	hds->huart = huart;
	hds->state = DS_STATE_IDLE;
	hds->already_converted = false;
	hds->temperature_conversion_done = false;
	hds->error_code = DS_ERROR_NONE;
	hds->last_error = DS_ERROR_NONE;
	hds->retry_count = 0;

	return DS_OK;								// Capteur bien initialisé
}

/**
 * @brief  Change le baudrate de l'UART de manière sécurisée.
 * @note   Fonction interne (static), non exposée dans le .h
 */
static DS18B20_Result_t DS18B20_SetBaudrate(DS18B20_t *hds, uint32_t baudrate)
{
    if (hds == NULL || hds->huart == NULL) return DS_ERR_PARAM;	// Vérification des paramètres

    __HAL_UART_DISABLE(hds->huart);
	if (baudrate == 9600) {
		hds->huart->Instance->BRR = LPUART_BRR_9600;
		hds->huart->Init.BaudRate = 9600;
	} else {
		hds->huart->Instance->BRR = LPUART_BRR_115200;
		hds->huart->Init.BaudRate = 115200;
	}
	__HAL_UART_ENABLE(hds->huart);

	return DS_OK;
}

DS18B20_Result_t DS18B20_Start(DS18B20_t *hds)
{
	if (hds == NULL) return DS_ERR_PARAM;

	if (hds->state == DS_STATE_ERROR) {
		return DS_ERR_HW_LOCKED;
	} else if (hds->state != DS_STATE_IDLE) {		// Vérification si une mesure n'est pas déjà en cours
		return DS_ERR_BUSY;
	}

	DS_LOG("Demarrage mesure...");

	hds->state = DS_STATE_START_RESET;				// Démarrage de la FSM de conversion
	hds->temperature_conversion_done = false;

	return DS_OK;
}

/**
 * @brief  Gère les erreurs de la FSM, nettoie le contexte et gère les tentatives.
 * @note   Fonction interne (static).
 */
static void DS18B20_HandleError(DS18B20_t *hds, DS18B20_Error_t error_code)
{
    if (hds == NULL || hds->huart == NULL) return;

    HAL_UART_Abort_IT(hds->huart);

    hds->last_error = error_code;
    hds->retry_count++;

    //hds->tx_done = false;
    //hds->rx_done = false;
    hds->temperature_conversion_done = false;
    hds->already_converted = false;

    if (hds->retry_count <= DS18B20_MAX_RETRIES) {
        DS_LOG("Erreur %d. Retry %d/%d...", error_code, hds->retry_count, DS18B20_MAX_RETRIES);
        hds->timer_start = HAL_GetTick();
        hds->state = DS_STATE_WAIT_RETRY;
    } else {
        DS_LOG("ECHEC CRITIQUE (Code %d). Abandon.", error_code);
        hds->state = DS_STATE_ERROR;
        HAL_UART_Abort(hds->huart);
    }
}

DS18B20_Status_t DS18B20_GetStatus(DS18B20_t *hds)
{
	if (hds == NULL) return DS_ERR_PARAM;

    if (hds->state == DS_STATE_IDLE) {
        return DS_STATUS_READY;
    }
    else if (hds->state == DS_STATE_ERROR) {
        return DS_STATUS_ERROR;
    }
    else {
        return DS_STATUS_BUSY;
    }
}

int16_t DS18B20_GetTemperature(DS18B20_t *hds) {
    return hds->temperature;
}

/**
 * @brief  Calcule le CRC-8 standard Dallas/Maxim (1-Wire).
 * @param  data Tableau des octets reçus.
 * @param  len  Nombre d'octets (généralement 8).
 * @return Le CRC calculé. S'il est égal au 9ème octet, les données sont valides.
 */
static uint8_t DS18B20_ComputeCRC(const uint8_t *data, uint8_t len) // TODO : Comprendre la fonction
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C; // Plynôme X^8 + X^5 + X^4 + 1 inversé
            }
            inbyte >>= 1;
        }
    }
    return crc;
}

void DS18B20_Process(DS18B20_t *hds) {
    switch (hds->state) {

        case DS_STATE_IDLE:
            break;

// ---------------- RESET PULSE ----------------

        case DS_STATE_START_RESET:
        	if (DS18B20_SetBaudrate(hds, 9600) != DS_OK) {
				DS18B20_HandleError(hds, DS_ERROR_UART_CONFIG);
				break;
			}

            hds->tx_done = false;
            hds->rx_done = false;

            __HAL_UART_DISABLE_IT(hds->huart, UART_IT_ERR);

            if (HAL_UART_Receive_DMA(hds->huart, hds->rx_buffer, 1) != HAL_OK) { DS18B20_HandleError(hds, DS_ERROR_DMA_INIT); break; }
            if (HAL_UART_Transmit_DMA(hds->huart, (uint8_t*)DS_CMD_RESET, 1) != HAL_OK) { DS18B20_HandleError(hds, DS_ERROR_DMA_INIT); break; }

            hds->timer_start = HAL_GetTick();
            hds->state = DS_STATE_WAIT_RESET_RX;
            break;

        case DS_STATE_WAIT_RESET_RX:
            if (hds->tx_done && hds->rx_done) {
                if (hds->rx_buffer[0] != 0xF0) {	// Est-ce que le capteur répond ?
					if (DS18B20_SetBaudrate(hds, 115200) != DS_OK) { DS18B20_HandleError(hds, DS_ERROR_UART_CONFIG); break; }
					hds->state = DS_STATE_SEND_SKIP_ROM;
				} else { DS18B20_HandleError(hds, DS_ERROR_NO_PRESENCE); }
            }
            else if ((HAL_GetTick() - hds->timer_start) > DS18B20_TIMEOUT_MS) { // Timeout
                HAL_UART_Abort_IT(hds->huart);
                DS18B20_HandleError(hds, DS_ERROR_UART_TIMEOUT);
            }
            break;

// ---------------- SKIP ROM ----------------

		case DS_STATE_SEND_SKIP_ROM:
			hds->tx_done = false;
			hds->rx_done = false;

			if (HAL_UART_Receive_DMA(hds->huart, hds->rx_buffer, 8) != HAL_OK) { DS18B20_HandleError(hds, DS_ERROR_DMA_INIT); break; }
			if (HAL_UART_Transmit_DMA(hds->huart, (uint8_t*)DS_CMD_SKIP_ROM, 8) != HAL_OK) { DS18B20_HandleError(hds, DS_ERROR_DMA_INIT); break; }

			hds->timer_start = HAL_GetTick();
			hds->state = DS_STATE_WAIT_SKIP_ROM_RX;
			break;

		case DS_STATE_WAIT_SKIP_ROM_RX:
			if (hds->tx_done && hds->rx_done) {
				if(hds->already_converted) { 										// Si la conversion a déjà eu lieu
					hds->state = DS_STATE_SEND_READ_SCRATCHPAD;
				}
				else {
					hds->state = DS_STATE_SEND_CONVERT;
				}
			}
			else if ((HAL_GetTick() - hds->timer_start) > DS18B20_TIMEOUT_MS) {
				HAL_UART_Abort(hds->huart);
				DS18B20_HandleError(hds, DS_ERROR_UART_TIMEOUT);
			}
			break;

// ---------------- CONVERT ----------------

		case DS_STATE_SEND_CONVERT:
			hds->tx_done = false;
			hds->rx_done = false;

            if (HAL_UART_Receive_DMA(hds->huart, hds->rx_buffer, 8) != HAL_OK) { DS18B20_HandleError(hds, DS_ERROR_DMA_INIT); break; }
            if (HAL_UART_Transmit_DMA(hds->huart, (uint8_t*)DS_CMD_CONVERT_T, 8) != HAL_OK) {
				DS18B20_HandleError(hds, DS_ERROR_DMA_INIT);
				break;
			}

			hds->timer_start = HAL_GetTick();
			hds->state = DS_STATE_WAIT_SEND_CONVERT_TX;
			break;

		case DS_STATE_WAIT_SEND_CONVERT_TX:
			if (hds->tx_done && hds->rx_done) {
				hds->timer_start = HAL_GetTick();
				hds->state = DS_STATE_WAIT_CONVERSION;
			}
			else if ((HAL_GetTick() - hds->timer_start) > DS18B20_TIMEOUT_MS) {
				HAL_UART_Abort(hds->huart);
				DS18B20_HandleError(hds, DS_ERROR_UART_TIMEOUT);
			}
			break;

		case DS_STATE_WAIT_CONVERSION:
			if ((HAL_GetTick() - hds->timer_start) > DS18B20_CONVERSION_TIME) {   // On laisse le capteur récupérer la température pendant 750ms de manière non-bloquante
				hds->already_converted = true;
				hds->state = DS_STATE_START_RESET;
			}
			break;

// ---------------- READ SCRATCHPAD ----------------

		case DS_STATE_SEND_READ_SCRATCHPAD:
			hds->tx_done = false;
			hds->rx_done = false;

            if (HAL_UART_Receive_DMA(hds->huart, hds->rx_buffer, 8) != HAL_OK) { DS18B20_HandleError(hds, DS_ERROR_DMA_INIT); break; }
            if (HAL_UART_Transmit_DMA(hds->huart, (uint8_t*)DS_CMD_READ_SCRATCH, 8) != HAL_OK) { DS18B20_HandleError(hds, DS_ERROR_DMA_INIT); break; }

			hds->timer_start = HAL_GetTick();
			hds->state = DS_STATE_WAIT_READ_SCRATCHPAD_TX;
			break;

		case DS_STATE_WAIT_READ_SCRATCHPAD_TX:
			if (hds->tx_done && hds->rx_done) {
				hds->timer_start = HAL_GetTick();
				hds->state = DS_STATE_READ_DATA;
			}
			else if ((HAL_GetTick() - hds->timer_start) > DS18B20_TIMEOUT_MS) {
				HAL_UART_Abort(hds->huart);
				DS18B20_HandleError(hds, DS_ERROR_UART_TIMEOUT);
			}
			break;

        case DS_STATE_READ_DATA:
        	hds->tx_done = false;
        	hds->rx_done = false;

            if (HAL_UART_Receive_DMA(hds->huart, hds->rx_buffer, 72) != HAL_OK) { DS18B20_HandleError(hds, DS_ERROR_DMA_INIT); break; }
            if (HAL_UART_Transmit_DMA(hds->huart, (uint8_t*)DS_CMD_READ, 72) != HAL_OK) { DS18B20_HandleError(hds, DS_ERROR_DMA_INIT); break; }

            hds->timer_start = HAL_GetTick();
            hds->state = DS_STATE_WAIT_READ_DATA;
        	break;

        case DS_STATE_WAIT_READ_DATA:
            if (hds->tx_done && hds->rx_done) {
            	hds->timer_start = HAL_GetTick();
                hds->state = DS_STATE_DECODE_TEMP;
            }
            else if ((HAL_GetTick() - hds->timer_start) > DS18B20_TIMEOUT_MS) {
            	HAL_UART_Abort(hds->huart);
                DS18B20_HandleError(hds, DS_ERROR_UART_TIMEOUT);
            }
        	break;

        case DS_STATE_DECODE_TEMP:
			{
				hds->already_converted = false;
				uint8_t scratchpad[9] = {0};

				/*
				for (uint8_t byte_idx = 0; byte_idx < 9; byte_idx++) {
					for (uint8_t bit_idx = 0; bit_idx < 8; bit_idx++) {
						uint8_t uart_byte = hds->rx_buffer[(byte_idx * 8) + bit_idx];
						if (uart_byte >= DS1Wire_BIT_1_THRESHOLD) {
							scratchpad[byte_idx] |= (1 << bit_idx);
						}
					}
				}
				*/
				uint8_t *ptr_uart = hds->rx_buffer; // Pointeur de lecture
				for (uint8_t byte_idx = 0; byte_idx < 9; byte_idx++) {
					uint8_t current_byte = 0;

					// Déroulage logique (optimisé par le compilateur)
					if (*ptr_uart++ >= DS1Wire_BIT_1_THRESHOLD) current_byte |= 0x01;
					if (*ptr_uart++ >= DS1Wire_BIT_1_THRESHOLD) current_byte |= 0x02;
					if (*ptr_uart++ >= DS1Wire_BIT_1_THRESHOLD) current_byte |= 0x04;
					if (*ptr_uart++ >= DS1Wire_BIT_1_THRESHOLD) current_byte |= 0x08;
					if (*ptr_uart++ >= DS1Wire_BIT_1_THRESHOLD) current_byte |= 0x10;
					if (*ptr_uart++ >= DS1Wire_BIT_1_THRESHOLD) current_byte |= 0x20;
					if (*ptr_uart++ >= DS1Wire_BIT_1_THRESHOLD) current_byte |= 0x40;
					if (*ptr_uart++ >= DS1Wire_BIT_1_THRESHOLD) current_byte |= 0x80;

					scratchpad[byte_idx] = current_byte;
				}

				uint8_t calculated_crc = DS18B20_ComputeCRC(scratchpad, 8);
				uint8_t received_crc = scratchpad[8];

				if (calculated_crc != received_crc) {
					DS_LOG("ERREUR CRC ! Attendu: %02X, Recu: %02X", calculated_crc, received_crc);
					DS18B20_HandleError(hds, DS_ERROR_CRC);
					break;
				}

				// Octet 0 = LSB, Octet 1 = MSB
				int16_t raw_temp = (int16_t)((scratchpad[1] << 8) | scratchpad[0]);

				hds->temperature = raw_temp;
				hds->temperature_conversion_done = true;
				hds->retry_count = 0;
				hds->last_error = DS_ERROR_NONE;
				hds->state = DS_STATE_IDLE;

				int16_t raw = hds->temperature;
				char sign = (raw < 0) ? '-' : ' ';
				uint16_t abs_raw = (raw < 0) ? -raw : raw;
				DS_LOG("Succes ! Temp: %c%d.%02d C (CRC OK)",
					   sign, abs_raw / 16, ((abs_raw % 16) * 100) / 16);
				break;
			}

        case DS_STATE_WAIT_RETRY:
        	if ((HAL_GetTick() - hds->timer_start) > DS18B20_RETRY_BREAK) {
        		hds->state = DS_STATE_START_RESET;
        	}
        	break;

        case DS_STATE_ERROR:
            break;
    }
}

void DS18B20_RxCpltCallback(DS18B20_t *hds)
{
	if (hds) hds->rx_done = true;
}
void DS18B20_TxCpltCallback(DS18B20_t *hds)
{
	if (hds) hds->tx_done = true;
}
