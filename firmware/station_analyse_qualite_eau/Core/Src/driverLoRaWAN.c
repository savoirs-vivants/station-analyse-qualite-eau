#include "driverLoRaWAN.h"
#include <string.h>
#include <stdio.h>

// --- Configuration des commandes AT internes ---
typedef struct {
    const char *cmd;
    uint32_t timeout_ms;
    const char *expected_reply;
} LoRa_AT_Cmd_t;

static const LoRa_AT_Cmd_t config_sequence[] = {
    {"AT\r\n", 2000, "OK"},
    {"AT+MODE=LWOTAA\r\n", 2000, "+MODE:"},
    {"AT+CLASS=A\r\n", 2000, "+CLASS:"},
    {"AT+ID=DevEui,\"FA5F4F58A217F713\"\r\n", 2000, "+ID:"},
    {"AT+ID=AppEui,\"0000000000000000\"\r\n", 2000, "+ID:"},
    {"AT+KEY=APPKEY,\"828C1D3468551154C0F9A582D82E3F86\"\r\n", 2000, "+KEY:"}
};
#define CONFIG_STEPS (sizeof(config_sequence) / sizeof(config_sequence[0]))

/* --- Helper Interne --- */
static void UART_Send_Cmd(LoRaWAN_t *lora, const char *cmd) {
    /* Nettoyage du buffer RX avant chaque nouvel envoi */
    memset(lora->rx_buffer, 0, sizeof(lora->rx_buffer));
    lora->rx_length = 0;
    lora->rx_complete = false;

    /* On relance l'écoute DMA */
    HAL_UARTEx_ReceiveToIdle_DMA(lora->huart, (uint8_t*)lora->rx_buffer, sizeof(lora->rx_buffer));

    /* Envoi de la commande */
    strncpy(lora->tx_buffer, cmd, sizeof(lora->tx_buffer) - 1);
    HAL_UART_Transmit_IT(lora->huart, (const uint8_t*)lora->tx_buffer, strlen(lora->tx_buffer));

    lora->cmd_start_time = HAL_GetTick();
}

// --- API Publique ---

LoRa_Result_t LoRaWAN_Init(LoRaWAN_t *lora, UART_HandleTypeDef *huart) {
    if (lora == NULL || huart == NULL) return LORA_ERR_PARAM;

    memset(lora, 0, sizeof(LoRaWAN_t));
    lora->huart = huart;
    lora->state = S_LORA_IDLE;
    lora->status = LORA_STATUS_READY;
    lora->is_joined = false;

    return LORA_OK;
}

LoRa_Result_t LoRaWAN_Configure(LoRaWAN_t *lora) {
	if (lora == NULL) return LORA_ERR_PARAM;
    if (lora->status == LORA_STATUS_BUSY) return LORA_ERR_BUSY;

    lora->config_step = 0;
    lora->retry_count = 0;
    lora->status = LORA_STATUS_BUSY;
    lora->state  = S_LORA_CONFIG_SEND;
    return LORA_OK;
}

LoRa_Result_t LoRaWAN_Join(LoRaWAN_t *lora) {
	if (lora == NULL) return LORA_ERR_PARAM;
    if (lora->status == LORA_STATUS_BUSY) return LORA_ERR_BUSY;

    lora->retry_count = 0;
    lora->status = LORA_STATUS_BUSY;
    lora->state  = S_LORA_JOIN_SEND;
    return LORA_OK;
}

LoRa_Result_t LoRaWAN_SendHex(LoRaWAN_t *lora, const char *hex_payload) {
	if (lora == NULL) return LORA_ERR_PARAM;
    if (lora->status == LORA_STATUS_BUSY) return LORA_ERR_BUSY;
    if (!lora->is_joined) return LORA_ERR_NOT_JOINED;

    /* Formatage de la commande dans le buffer TX */
    snprintf((char*)lora->tx_buffer, sizeof(lora->tx_buffer), "AT+MSGHEX=\"%s\"\r\n", hex_payload);

    lora->status = LORA_STATUS_BUSY;
    lora->state  = S_LORA_TX_SEND;
    return LORA_OK;
}

LoRa_Result_t LoRaWAN_Sleep(LoRaWAN_t *lora) {
    if (lora == NULL) return LORA_ERR_PARAM;
    if (lora->status == LORA_STATUS_BUSY) return LORA_ERR_BUSY;

    /* On verrouille le statut public et on lance la machine d'état */
    lora->status = LORA_STATUS_BUSY;
    lora->state  = S_LORA_GOTO_SLEEP;

    return LORA_OK;
}

void LoRaWAN_ResetError(LoRaWAN_t *lora) {
    if (lora != NULL && lora->status == LORA_STATUS_ERROR) {
        /* On efface l'ardoise et on remet le driver au travail */
        lora->retry_count = 0;
        lora->state = S_LORA_IDLE;
        lora->status = LORA_STATUS_READY;

        printf("[LoRa] Erreur acquittee. Retour a l'etat READY.\r\n");
    }
}

LoRa_Result_t LoRaWAN_WakeUp(LoRaWAN_t *lora) {
    if (lora == NULL) return LORA_ERR_PARAM;

    /* On accepte de réveiller le module s'il dort, ou même s'il est en erreur
       (au cas où l'erreur serait liée à un sommeil qui s'est mal passé) */
    if (lora->status == LORA_STATUS_SLEEP || lora->status == LORA_STATUS_ERROR) {
        lora->status = LORA_STATUS_BUSY;
        lora->state = S_LORA_WAKING_UP;
        return LORA_OK;
    }

    return LORA_ERR_BUSY; /* S'il est déjà prêt ou en train de faire autre chose */
}

/* ==========================================================================
 * MACHINE D'ÉTAT PRINCIPALE
 * ========================================================================== */

void LoRaWAN_Process(LoRaWAN_t *lora) {
    if (lora->state == S_LORA_IDLE || lora->state == S_LORA_ERROR || lora->state == S_LORA_SLEEP) {
        return; /* Rien à faire de manière asynchrone */
    }

    switch (lora->state) {

        /* ---------------------------------------------------
         * ETAPE 1 : CONFIGURATION
         * --------------------------------------------------- */
        case S_LORA_CONFIG_SEND:
            if (lora->config_step < CONFIG_STEPS) {
                printf("[LoRa] Config step %d/%d\r\n", lora->config_step + 1, CONFIG_STEPS);
                UART_Send_Cmd(lora, config_sequence[lora->config_step].cmd);
                lora->state = S_LORA_CONFIG_WAIT;
            } else {
                /* Fin de la configuration, on repasse en IDLE ou on lance le Join directement */
                lora->status = LORA_STATUS_READY;
                lora->state  = S_LORA_IDLE;
            }
            break;

        case S_LORA_CONFIG_WAIT:
            if (lora->rx_complete) {
                lora->rx_complete = false;
                printf("[LoRa] Retour %s\r\n", lora->rx_buffer);

                /* Si on trouve la réponse attendue */
                if (strstr((char*)lora->rx_buffer, config_sequence[lora->config_step].expected_reply) != NULL) {
                    lora->config_step++;
                    lora->retry_count = 0;
                    lora->state = S_LORA_CONFIG_SEND; /* On boucle sur le suivant ! */
                }
                /* Si on reçoit une erreur AT */
                else if (strstr((char*)lora->rx_buffer, "ERROR") != NULL) {
                    lora->retry_count++;
                    if (lora->retry_count >= 3) {
                        lora->status = LORA_STATUS_ERROR;
                        lora->state  = S_LORA_ERROR;
                    } else {
                        lora->state = S_LORA_CONFIG_SEND; /* On retente la même commande */
                    }
                } else {
                    /* Bruit, on relance l'écoute */
                    HAL_UARTEx_ReceiveToIdle_DMA(lora->huart, (uint8_t*)lora->rx_buffer, sizeof(lora->rx_buffer));
                }
            }
            /* Timeout spécifique à cette commande */
            else if ((HAL_GetTick() - lora->cmd_start_time) > config_sequence[lora->config_step].timeout_ms) {
                HAL_UART_AbortReceive(lora->huart);
                lora->state = S_LORA_CONFIG_SEND; /* On retente */
            }
            break;

        /* ---------------------------------------------------
         * ETAPE 2 : REJOINDRE LE RESEAU (JOIN)
         * --------------------------------------------------- */
        case S_LORA_JOIN_SEND:
            printf("[LoRa] Tentative de Join...\r\n");
            UART_Send_Cmd(lora, "AT+JOIN\r\n");
            lora->state = S_LORA_JOIN_WAIT;
            break;

        case S_LORA_JOIN_WAIT:
            if (lora->rx_complete) {
            	printf("[LoRa] Retour %s\r\n", lora->rx_buffer);
                lora->rx_complete = false;

                if (strstr((char*)lora->rx_buffer, "Done") != NULL) {
                    printf("[LoRa] Join OK !\r\n");
                    lora->is_joined = true;
                    lora->status = LORA_STATUS_READY;
                    lora->state  = S_LORA_IDLE;
                } else if (strstr((char*)lora->rx_buffer, "failed") != NULL) {
                    /* Echec du Join (ex: pas de Gateway à portée) */
                    lora->status = LORA_STATUS_ERROR;
                    lora->state  = S_LORA_ERROR;
                } else {
                    HAL_UARTEx_ReceiveToIdle_DMA(lora->huart, (uint8_t*)lora->rx_buffer, sizeof(lora->rx_buffer));
                }
            }
            else if ((HAL_GetTick() - lora->cmd_start_time) > 15000) { /* Timeout 15s */
                HAL_UART_AbortReceive(lora->huart);
                lora->status = LORA_STATUS_ERROR;
                lora->state  = S_LORA_ERROR;
            }
            break;

        /* ---------------------------------------------------
         * ETAPE 3 : ENVOI DE DONNEES (TX)
         * --------------------------------------------------- */
        case S_LORA_TX_SEND:
				/* On ne fait PAS de strncpy sur lui-même, on envoie direct ! */
				memset(lora->rx_buffer, 0, sizeof(lora->rx_buffer));
				lora->rx_complete = false;
				HAL_UARTEx_ReceiveToIdle_DMA(lora->huart, (uint8_t*)lora->rx_buffer, sizeof(lora->rx_buffer));

				HAL_UART_Transmit_IT(lora->huart, (const uint8_t*)lora->tx_buffer, strlen(lora->tx_buffer));
				lora->cmd_start_time = HAL_GetTick();
				lora->state = S_LORA_TX_WAIT;
				break;

        case S_LORA_TX_WAIT:
            if (lora->rx_complete) {
            	printf("[LoRa] Retour %s\r\n", lora->rx_buffer);
                lora->rx_complete = false;

                /* Le module renvoie "Done" ou "OK" quand le payload est parti en l'air */
                if (strstr((char*)lora->rx_buffer, "Done") != NULL) {
                    lora->status = LORA_STATUS_READY;
                    lora->state  = S_LORA_IDLE;
                } else if (strstr((char*)lora->rx_buffer, "Please join") != NULL) {
                    lora->is_joined = false;
                    lora->status = LORA_STATUS_ERROR;
                    lora->state  = S_LORA_ERROR;
                } else {
                    HAL_UARTEx_ReceiveToIdle_DMA(lora->huart, (uint8_t*)lora->rx_buffer, sizeof(lora->rx_buffer));
                }
            }
            else if ((HAL_GetTick() - lora->cmd_start_time) > 10000) {
                HAL_UART_AbortReceive(lora->huart);
                lora->status = LORA_STATUS_ERROR;
                lora->state  = S_LORA_ERROR;
            }
            break;

            /* ---------------------------------------------------
			 * SOMMEIL (LOW POWER)
			 * --------------------------------------------------- */
			case S_LORA_GOTO_SLEEP:
				UART_Send_Cmd(lora, "AT+LOWPOWER\r\n");
				lora->state = S_LORA_WAIT_SLEEP;
				break;

			case S_LORA_WAIT_SLEEP:
				if (lora->rx_complete) {
					lora->rx_complete = false;
					printf("[LoRa] Retour Sleep: %s\r\n", lora->rx_buffer);

					/* Le module confirme qu'il s'endort (ex: "+LOWPOWER: SLEEP" ou "OK") */
					if (strstr((char*)lora->rx_buffer, "LOWPOWER") != NULL || strstr((char*)lora->rx_buffer, "OK") != NULL) {

						/* Optionnel mais très recommandé : Couper le DMA UART du STM32
						   pour éviter qu'un bruit électrique ne réveille le microcontrôleur */
						HAL_UART_AbortReceive(lora->huart);

						lora->status = LORA_STATUS_SLEEP; /* Statut public */
						lora->state  = S_LORA_SLEEP;      /* Verrouille la machine d'état */
					}
					else if (strstr((char*)lora->rx_buffer, "ERROR") != NULL || strstr((char*)lora->rx_buffer, "ERR") != NULL) {
						/* La commande a été refusée, mais on est toujours sur le réseau ! */
						lora->status = LORA_STATUS_ERROR;
						lora->state  = S_LORA_ERROR;
					}
					else {
						/* Bruit sur la ligne, on relance l'écoute */
						HAL_UARTEx_ReceiveToIdle_DMA(lora->huart, (uint8_t*)lora->rx_buffer, sizeof(lora->rx_buffer));
					}
				}
				/* Timeout très court : si ça ne dort pas en 500ms, c'est planté ! */
				else if ((HAL_GetTick() - lora->cmd_start_time) > 500) {
					HAL_UART_AbortReceive(lora->huart);
					lora->status = LORA_STATUS_ERROR;
					lora->state  = S_LORA_ERROR;
				}
				break;

				/* ---------------------------------------------------
				 * RÉVEIL (WAKE UP)
				 * --------------------------------------------------- */
				case S_LORA_WAKING_UP:
					/* 1. La pichenette : On envoie un caractère brut en mode bloquant (rapide).
						  Pas de UART_Send_Cmd car on n'attend aucune réponse lisible. */
					{
						uint8_t wakeup_char = 'A';
						HAL_UART_Transmit(lora->huart, &wakeup_char, 1, 10);
					}
					lora->cmd_start_time = HAL_GetTick();
					lora->state = S_LORA_WAKING_UP_WAIT;
					break;

				case S_LORA_WAKING_UP_WAIT:
					if ((HAL_GetTick() - lora->cmd_start_time) > 20)
					{
						HAL_UART_AbortReceive(lora->huart);

						/* Note: Si __HAL_UART_FLUSH_DRREGISTER n'existe pas sur ton STM32U0,
						   remplace-le par __HAL_UART_CLEAR_OREFLAG(lora->huart); */
						__HAL_UART_FLUSH_DRREGISTER(lora->huart);

						HAL_UARTEx_ReceiveToIdle_DMA(lora->huart, (uint8_t*)lora->rx_buffer, sizeof(lora->rx_buffer));

						lora->status = LORA_STATUS_READY;
						lora->state  = S_LORA_IDLE;
					}
					break;
        default:
            break;
    }
}

LoRa_Status_t LoRaWAN_GetStatus(LoRaWAN_t *lora)
{
    /* 1. Blindage classique contre le vide spatial */
    if (lora == NULL) {
        return LORA_STATUS_UNINIT;
    }

    /* 2. On retourne le statut public mis à jour par la machine d'état */
    return lora->status;
}

void LoRaWAN_RxEventCallback(LoRaWAN_t *lora, uint16_t Size) {
    lora->rx_length = Size;
    lora->rx_complete = true;
}
