/*
 * driverFRAM.c
 *
 * Created on: 16 avr. 2026
 * Author: ton_nom
 */

#include "driverFRAM.h"
#include <string.h>

FRAM_Result_t FRAM_Init(FRAM_t *hfram, SPI_HandleTypeDef *hspi, GPIO_TypeDef *gpio_port, uint16_t gpio_pin)
{
	if (hfram == NULL || hspi == NULL || gpio_port == NULL || gpio_pin == 0) return FRAM_ERR_PARAM;

	hfram->hspi = hspi;
	hfram->cs_gpio_port = gpio_port;
	hfram->cs_gpio_pin = gpio_pin;
	hfram->address = 0;

	hfram->state = FRAM_STATE_IDLE;
	hfram->status = FRAM_STATUS_READY;
	hfram->error_code = FRAM_INT_ERR_NONE;
	hfram->cmd_flag = false;
	hfram->start_time = 0;

	HAL_GPIO_WritePin(hfram->cs_gpio_port, hfram->cs_gpio_pin, SET);

	return FRAM_OK;
}

FRAM_Result_t FRAM_Write(FRAM_t *hfram, uint32_t address, uint8_t *data, uint16_t data_size)
{
	if (hfram == NULL || data == NULL || data_size == 0) return FRAM_ERR_PARAM;

	if ((address + data_size) > FRAM_MAX_ADDRESS) return FRAM_ERR_PARAM;

	if(hfram->state != FRAM_STATE_IDLE) return FRAM_ERR_BUSY;

	hfram->address = address;
	hfram->tx_data = data;
	hfram->tx_data_size = data_size;

	hfram->status = FRAM_STATUS_BUSY;
	hfram->error_code = FRAM_INT_ERR_NONE;

	hfram->state = FRAM_STATE_WRITE_EN;

	return FRAM_OK;
}

FRAM_Result_t FRAM_Read(FRAM_t *hfram, uint32_t address, uint8_t *rx_data, uint16_t data_size)
{
	if (hfram == NULL || rx_data == NULL || data_size == 0) return FRAM_ERR_PARAM;

	if ((address + data_size) > FRAM_MAX_ADDRESS) return FRAM_ERR_PARAM;

	if(hfram->state != FRAM_STATE_IDLE) return FRAM_ERR_BUSY;

	hfram->address = address;
	hfram->rx_data = rx_data;
	hfram->rx_data_size = data_size;

	hfram->status = FRAM_STATUS_BUSY;
	hfram->error_code = FRAM_INT_ERR_NONE;

	hfram->state = FRAM_STATE_READ_EN;

	return FRAM_OK;
}

/**
 * @brief  Gère les erreurs critiques du bus SPI/DMA.
 * Aborte la transaction en cours, libère le bus et passe le driver en erreur.
 * @note   Fonction interne (static).
 */
static void FRAM_HandleError(FRAM_t *hfram, FRAM_ErrorCode_t error_code)
{
    if (hfram == NULL) return;

    if (hfram->hspi != NULL) {
        HAL_SPI_Abort(hfram->hspi); // Utilise Abort classique, pas IT si on est hors IRQ
    }

    if (hfram->cs_gpio_port != NULL) {
         HAL_GPIO_WritePin(hfram->cs_gpio_port, hfram->cs_gpio_pin, SET);
    }

    hfram->error_code = error_code;
    hfram->state = FRAM_STATE_IDLE;
    hfram->status = FRAM_STATUS_ERROR;

    printf("FRAM ERREUR FATALE (Code %d)\r\n", error_code);
}

FRAM_Status_t FRAM_GetStatus(FRAM_t *hfram)
{
	if (hfram == NULL) return FRAM_ERR_PARAM;

    if (hfram->state == FRAM_STATE_IDLE) {
        return FRAM_STATUS_READY;
    }
    else if (hfram->state == FRAM_STATE_ERROR) {
        return FRAM_STATUS_ERROR;
    }
    else {
        return FRAM_STATUS_BUSY;
    }
}

void FRAM_Process(FRAM_t *hfram)
{
	switch(hfram->state) {
		case FRAM_STATE_IDLE:
			break;

		// ==========================================
		// PARTIE ECRITURE (Avec verrou WREN)
		// ==========================================
		case FRAM_STATE_WRITE_EN:
		{
			static const uint8_t cmd_wren = 0x06; // Commande WREN

			hfram->start_time = HAL_GetTick();

			HAL_GPIO_WritePin(hfram->cs_gpio_port, hfram->cs_gpio_pin, RESET);
			if (HAL_SPI_Transmit_DMA(hfram->hspi, (uint8_t*)&cmd_wren, 1) != HAL_OK) {
				FRAM_HandleError(hfram, FRAM_INT_ERR_DMA_HALT);
				return;
			}
			hfram->state = FRAM_STATE_WAIT_WRITE_EN;
			break;
		}

		case FRAM_STATE_WAIT_WRITE_EN:
			if(hfram->cmd_flag) {
				HAL_GPIO_WritePin(hfram->cs_gpio_port, hfram->cs_gpio_pin, SET); // Valider WREN
				hfram->cmd_flag = false;

				// Petit délai pour respecter le tCEH de la puce
				//__NOP(); __NOP(); __NOP(); __NOP();

				hfram->state = FRAM_STATE_WRITE_DATA;
			}
			else if ((HAL_GetTick() - hfram->start_time) > FRAM_TIMEOUT_MS) {
				FRAM_HandleError(hfram, FRAM_INT_ERR_TIMEOUT_WREN);
			}
			break;

		case FRAM_STATE_WRITE_DATA:
		{
			static const uint8_t cmd_write = 0x02; // Commande WRITE

			hfram->tx_buffer[0] = cmd_write;
			hfram->tx_buffer[1] = (hfram->address >> 16) & 0xFF; // MSB
			hfram->tx_buffer[2] = (hfram->address >> 8) & 0xFF;
			hfram->tx_buffer[3] = hfram->address & 0xFF;         // LSB

			uint16_t payload_size = hfram->tx_data_size + 4;
			memcpy(&hfram->tx_buffer[4], hfram->tx_data, hfram->tx_data_size);

			hfram->start_time = HAL_GetTick();

			HAL_GPIO_WritePin(hfram->cs_gpio_port, hfram->cs_gpio_pin, RESET); // On rebaisse le CS
			if (HAL_SPI_Transmit_DMA(hfram->hspi, hfram->tx_buffer, payload_size) != HAL_OK) {
				FRAM_HandleError(hfram, FRAM_INT_ERR_DMA_HALT);
				return;
			}
			hfram->state = FRAM_STATE_WAIT_WRITE_DATA;
			break;
		}

		case FRAM_STATE_WAIT_WRITE_DATA:
			if(hfram->cmd_flag) {
				HAL_GPIO_WritePin(hfram->cs_gpio_port, hfram->cs_gpio_pin, SET);
				hfram->cmd_flag = false;

				hfram->state = FRAM_STATE_IDLE;
				hfram->status = FRAM_STATUS_READY;
			}
			else if ((HAL_GetTick() - hfram->start_time) > FRAM_TIMEOUT_MS) {
				FRAM_HandleError(hfram, FRAM_INT_ERR_TIMEOUT_WRITE);
			}
			break;

		// ==========================================
		// PARTIE LECTURE (Transaction continue)
		// ==========================================
		case FRAM_STATE_READ_EN:
		{
			static const uint8_t cmd_read = 0x03; // Commande READ

			hfram->tx_buffer[0] = cmd_read;
			hfram->tx_buffer[1] = (hfram->address >> 16) & 0xFF; // MSB
			hfram->tx_buffer[2] = (hfram->address >> 8) & 0xFF;
			hfram->tx_buffer[3] = hfram->address & 0xFF;         // LSB

			memset(&hfram->tx_buffer[4], 0, hfram->rx_data_size); // Dummy bytes

			hfram->start_time = HAL_GetTick();

			HAL_GPIO_WritePin(hfram->cs_gpio_port, hfram->cs_gpio_pin, RESET);
			if (HAL_SPI_TransmitReceive_DMA(hfram->hspi, hfram->tx_buffer, hfram->rx_buffer, hfram->rx_data_size + 4) != HAL_OK) {
				FRAM_HandleError(hfram, FRAM_INT_ERR_DMA_HALT);
				return;
			}
			hfram->state = FRAM_STATE_WAIT_READ_DATA;
			break;
		}

		case FRAM_STATE_WAIT_READ_DATA:
			if(hfram->cmd_flag) {
				HAL_GPIO_WritePin(hfram->cs_gpio_port, hfram->cs_gpio_pin, SET);
				hfram->cmd_flag = false;

				memcpy(hfram->rx_data, &hfram->rx_buffer[4], hfram->rx_data_size);

				hfram->state = FRAM_STATE_IDLE;
				hfram->status = FRAM_STATUS_READY;
			}
			/* AJOUT BÉTON : Sécurité Timeout */
			else if ((HAL_GetTick() - hfram->start_time) > FRAM_TIMEOUT_MS) {
				FRAM_HandleError(hfram, FRAM_INT_ERR_TIMEOUT_READ);
			}
			break;

		default:
			break;
	}
}

void FRAM_Rx_Callback(FRAM_t *hfram)
{
	hfram->cmd_flag = true;
}

void FRAM_Tx_Callback(FRAM_t *hfram)
{
	hfram->cmd_flag = true;
}
