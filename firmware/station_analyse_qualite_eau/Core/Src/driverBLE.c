/*
 * driverBLE.c
 */

#include "driverBLE.h"
#include <string.h>

/* --- Timeouts et Constantes --- */
#define BLE_TIMEOUT_CONNEXION 60000 // 60s pour se connecter et envoyer la requête
#define BLE_TIMEOUT_ACK       60000 // 60s d'attente max après l'envoi
#define BLE_BOOT_TIME         500   // 500ms d'attente au démarrage du module

#define TO_CMD				  500 // TODO : A supprimer

typedef struct {
    const char *cmd;
    uint32_t timeout_ms;
    const char *expected_reply;
} BLE_AT_Cmd_t;

static const BLE_AT_Cmd_t config_sequence[] = {
		{"$$$", 500, "CMD"},
		{"S-,StationEau\r", 500, "AOK"},
		{"---\r", 500, "END"}
};

#define CONFIG_STEPS (sizeof(config_sequence) / sizeof(config_sequence[0])) // TODO : A mon avis cela ne foncitonne plus...


/* --- Helper Interne --- */
static void UART_Send_Cmd(BLE_t *hble, const char *cmd) {
    /* Nettoyage du buffer RX avant chaque nouvel envoi */
    memset(hble->rx_buffer, 0, sizeof(hble->rx_buffer));
    hble->rx_size = 0;
    hble->rx_flag = false;

    __HAL_UART_CLEAR_OREFLAG(hble->huart);

    /* On relance l'écoute DMA */
    HAL_UARTEx_ReceiveToIdle_DMA(hble->huart, (uint8_t*)hble->rx_buffer, sizeof(hble->rx_buffer));

    /* Envoi de la commande */
    strncpy(hble->tx_buffer, cmd, sizeof(hble->tx_buffer) - 1);
    HAL_UART_Transmit_IT(hble->huart, (const uint8_t*)hble->tx_buffer, strlen(hble->tx_buffer));

    hble->timer_start = HAL_GetTick();
}


void BLE_Init(BLE_t *hble, UART_HandleTypeDef *huart)
{
    hble->huart = huart;
    hble->state = BLE_STATE_IDLE;
    hble->rx_flag = false;
    memset(hble->tx_buffer, 0, sizeof(hble->tx_buffer));

    BLE_PRINT("Initialisation terminee.\r\n");
}

BLE_Result_t BLE_Configure(BLE_t *hble)
{
	if (hble == NULL) return BLE_ERR_PARAM;

	hble->state = BLE_STATE_CONFIGURING;
	hble->status = BLE_STATUS_CONFIGURING;

	hble->timer_start = HAL_GetTick();

	return BLE_OK;
}

BLE_Result_t BLE_Start(BLE_t *hble)
{
	if (hble == NULL) return BLE_ERR_PARAM;
	if (hble->state != BLE_STATE_IDLE) return BLE_BUSY;

	hble->timer_start = HAL_GetTick();
	hble->state = BLE_STATE_START;

	return BLE_OK;
}

void BLE_Process(BLE_t *hble)
{
    switch(hble->state)
    {
        case BLE_STATE_IDLE:
            break;

        case BLE_STATE_CONFIGURING:
            if (HAL_GetTick() - hble->timer_start > BLE_BOOT_TIME) {
            	if (hble->config_step < CONFIG_STEPS) {
					printf("[BLE] Config step %d/%d\r\n", hble->config_step + 1, CONFIG_STEPS);
					UART_Send_Cmd(hble, config_sequence[hble->config_step].cmd);
					hble->state = BLE_STATE_CONFIGURING_WAIT;
				} else {
					/* Fin de la configuration, on repasse en IDLE */
					hble->status = BLE_STATUS_READY;
					hble->state  = BLE_STATE_IDLE;
				}
            }
        	break;

        case BLE_STATE_CONFIGURING_WAIT:
        	if (hble->rx_flag) {
				hble->rx_flag = false;
				printf("[BLE] Retour %s\r\n", hble->rx_buffer);

				/* Si on trouve la réponse attendue */
				if (strstr((char*)hble->rx_buffer, config_sequence[hble->config_step].expected_reply) != NULL) {
					hble->config_step++;
					hble->retry_count = 0;
					hble->state = BLE_STATE_CONFIGURING; /* On boucle sur le suivant ! */
				}
				/* Si on reçoit une erreur AT */
				else if (strstr((char*)hble->rx_buffer, "ERR") != NULL) { // TODO : Ou alors aussi "?" qui vient lorsque il ne reconnait pas du tout la commande
					hble->retry_count++;
					BLE_PRINT("Retry %d\r\n", hble->retry_count);
					if (hble->retry_count >= 3) {
						hble->status = BLE_STATUS_ERROR;
						hble->state  = BLE_STATE_ERROR;
					} else {
						hble->state = BLE_STATE_CONFIGURING; /* On retente la même commande */
					}
				} else {
					/* Bruit, on relance l'écoute */
					HAL_UARTEx_ReceiveToIdle_DMA(hble->huart, (uint8_t*)hble->rx_buffer, sizeof(hble->rx_buffer));
				}
			}
			/* Timeout spécifique à cette commande */
			else if ((HAL_GetTick() - hble->timer_start) > config_sequence[hble->config_step].timeout_ms) {
				HAL_UART_AbortReceive(hble->huart);
				hble->state = BLE_STATE_CONFIGURING; /* On retente */
				hble->retry_count++;
				BLE_PRINT("Retry %d\r\n", hble->retry_count);

			}
        	break;

        case BLE_STATE_START:
            if (HAL_GetTick() - hble->timer_start > BLE_BOOT_TIME) {
                BLE_PRINT("Module pret. Attente requete utilisateur ('?')...\r\n");

                // On met l'UART en écoute (1 octet)
                __HAL_UART_CLEAR_FLAG(hble->huart, UART_CLEAR_OREF);
                memset(hble->rx_buffer, 0, sizeof(hble->rx_buffer));
				HAL_UARTEx_ReceiveToIdle_DMA(hble->huart, (uint8_t*)hble->rx_buffer, sizeof(hble->rx_buffer));


                hble->timer_start = HAL_GetTick();
                hble->state = BLE_STATE_WAIT_CMD;
            }
            break;

        case BLE_STATE_WAIT_CMD:
			if (hble->rx_flag) {
				hble->rx_flag = false;
				hble->rx_buffer[sizeof(hble->rx_buffer) - 1] = '\0';
				BLE_PRINT("Commande receptionnee : %s\r\n", hble->rx_buffer);
				if (strchr((char*)hble->rx_buffer, '?') != NULL) {
					BLE_PRINT("Le telephone est pret a recevoir !\r\n");
					hble->timer_start = HAL_GetTick();
					hble->status = BLE_STATUS_STREAMING;
					hble->state = BLE_STATE_STREAMING;  // On passe en mode "Tuyau ouvert"
				} else {
					BLE_PRINT("Commande inconnue : %s\r\n", hble->rx_buffer); // TODO : Est-ce que cela pose un problème ? on est pas sur que il y aura un \0 à la fin non ?
					__HAL_UART_CLEAR_FLAG(hble->huart, UART_CLEAR_OREF);
					memset(hble->rx_buffer, 0, sizeof(hble->rx_buffer));
					HAL_UARTEx_ReceiveToIdle_DMA(hble->huart, (uint8_t*)hble->rx_buffer, sizeof(hble->rx_buffer));

				}
			}
			if ((HAL_GetTick() - hble->timer_start) > BLE_TIMEOUT_CONNEXION) {
				HAL_UART_AbortReceive(hble->huart);
				hble->state = BLE_STATE_STOP;
			}
			break;

		case BLE_STATE_STREAMING:
			// Le driver ne fait RIEN de lui-même ici !
			// Il attend passivement que le main.c appelle BLE_SendChunk().
			if ((HAL_GetTick() - hble->timer_start) > BLE_TIMEOUT_ACK) {
				BLE_PRINT("TIMEOUT Inactivite (30s) après le signalement de disponibilité. Extinction.\r\n");
				HAL_UART_AbortReceive(hble->huart);
				hble->state = BLE_STATE_STOP;
			}
			break;

		case BLE_STATE_STREAMING_WAIT:
			if (hble->tx_flag) {
				hble->tx_flag = false;
				hble->state = BLE_STATE_STREAMING;
			}
			if ((HAL_GetTick() - hble->timer_start) > BLE_TIMEOUT_ACK) {
				BLE_PRINT("TIMEOUT Inactivite (30s) après le signalement de disponibilité. Extinction.\r\n");
				HAL_UART_AbortReceive(hble->huart);
				hble->state = BLE_STATE_STOP;
			}
			break;

        case BLE_STATE_WAIT_ACK:
            if (hble->rx_flag) {
                hble->rx_flag = false;
                hble->rx_buffer[sizeof(hble->rx_buffer) - 1] = '\0';
                BLE_PRINT("Commande receptionnee : %s\r\n", hble->rx_buffer);
                if (strchr((char*)hble->rx_buffer, 'Q') != NULL) {
                    BLE_PRINT("L'application a quitte. Extinction immediate.\r\n");
                    hble->state = BLE_STATE_STOP;
                } else {
					BLE_PRINT("Commande inconnue : %s\r\n", hble->rx_buffer); // TODO : idem qu'avant, est-ce que c'est sécure ? par rapport au caractère null à la fin de la chaine
					__HAL_UART_CLEAR_FLAG(hble->huart, UART_CLEAR_OREF);
					memset(hble->rx_buffer, 0, sizeof(hble->rx_buffer));
					HAL_UARTEx_ReceiveToIdle_DMA(hble->huart, (uint8_t*)hble->rx_buffer, sizeof(hble->rx_buffer));
                }
            }

            // Sécurité si l'utilisateur quitte la portée Bluetooth sans rien dire
            if (HAL_GetTick() - hble->timer_start > BLE_TIMEOUT_ACK) {
                BLE_PRINT("TIMEOUT Inactivite (30s). Extinction.\r\n");
                hble->state = BLE_STATE_STOP;
            }
            break;

        case BLE_STATE_STOP:
            // Extinction physique (SET = OFF)
        	//BLE_Sleep(hble);
        	//BLE_PRINT("Module hors tension.\r\n");
            hble->state = BLE_STATE_IDLE;
            hble->status = BLE_STATUS_READY;
            break;

        case BLE_STATE_ERROR:
        	break;
    }
}

/* --- Getters & Callbacks --- */

BLE_Status_t BLE_GetStatus(BLE_t *hble)
{
    return hble->status;
}


void BLE_RxEventCallback(BLE_t *hble, uint16_t Size)
{
    hble->rx_flag = true;
    hble->rx_size = Size;
}
void BLE_TxCpltCallback(BLE_t *hble)
{
    hble->tx_flag = true;
}

BLE_Result_t BLE_SendChunk(BLE_t *hble, const char *chunk) {
	if (hble == NULL || chunk == NULL) return BLE_ERR_PARAM;
    if (HAL_UART_Transmit_DMA(hble->huart, (uint8_t*)chunk, strlen(chunk)) == HAL_BUSY) return BLE_BUSY;
    BLE_PRINT("CHUNCK ENVOYE : %s\r\n", chunk);
    return BLE_OK;
}

BLE_Result_t BLE_EndStream(BLE_t *hble) {
	if(hble == NULL) return BLE_ERR_PARAM;
    BLE_PRINT("Fin du flux demandee par l'application.\r\n");
    BLE_PRINT("End Stream - Attente du caractère pour quitter 'Q'\r\n");
    // On bascule vers l'attente de l'acquittement 'Q', ou l'extinction
	__HAL_UART_CLEAR_FLAG(hble->huart, UART_CLEAR_OREF);
	memset(hble->rx_buffer, 0, sizeof(hble->rx_buffer));
	HAL_UARTEx_ReceiveToIdle_DMA(hble->huart, (uint8_t*)hble->rx_buffer, sizeof(hble->rx_buffer));
    hble->timer_start = HAL_GetTick();
    hble->state = BLE_STATE_WAIT_ACK;

    return BLE_OK;
}






void BLE_Sleep(BLE_t *hble) { // TODO : a supp
    // Le driver sait qu'il doit tuer son UART et couper sa propre ligne d'alimentation
    HAL_UART_DeInit(hble->huart);
    HAL_GPIO_WritePin(EN_LS_BLE_GPIO_Port, EN_LS_BLE_Pin, RESET);
}

void BLE_WakeUp(BLE_t *hble) { // TODO : a supp
    // Le driver sait comment se ressusciter
    HAL_UART_Init(hble->huart);
    HAL_GPIO_WritePin(EN_LS_BLE_GPIO_Port, EN_LS_BLE_Pin, SET);
}
