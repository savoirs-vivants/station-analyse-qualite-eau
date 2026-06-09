/*
 * driverLoRaApp.c
 *
 *  Created on: 8 avr. 2026
 *      Author: valen
 */

#include "driverLoRaApp.h"

void LoRa_App_Init(LoRaWAN_Manager_t * hlorapp, LoRa_t * hlora, const LoRa_Command_t * cmds)
{
    hlorapp->hardware_driver = hlora;
    hlorapp->is_ready = false;
    hlorapp->sequence = cmds;
    hlorapp->total_steps = 7; // TODO : détecter le nombre de commande total
    hlorapp->current_step = 0;
}

void LoRa_App_Process(LoRaWAN_Manager_t *hlorapp)
{
    // 1. Si toute la séquence est terminée, on ne fait plus rien.
    if (hlorapp->is_ready)
    {
        return;
    }

    // 2. On regarde dans quel état se trouve le driver matériel
    switch (hlorapp->hardware_driver->state)
    {
        case LORA_STATE_IDLE:
            // Le moteur est au repos. On lance l'envoi de la commande correspondant à l'étape actuelle.
            LoRa_SendCmdAsync(hlorapp->hardware_driver,
                              hlorapp->sequence[hlorapp->current_step].cmd,
                              hlorapp->sequence[hlorapp->current_step].timeout_ms);
            break;

        case LORA_STATE_OK:
            // Le moteur a reçu une réponse positive ("OK").
            // (Note : on retire les printf ici pour respecter l'encapsulation du driver)

            hlorapp->current_step++; // On passe à l'étape suivante

            if (hlorapp->current_step >= hlorapp->total_steps) {
                hlorapp->is_ready = true; // Séquence complètement finie !
                LoRa_ResetState(hlorapp->hardware_driver);
            } else {
                // On réinitialise le moteur.
                // À la prochaine boucle (dans 1 milliseconde), le switch tombera sur "LORA_STATE_IDLE"
                // et enverra la commande suivante.
                LoRa_ResetState(hlorapp->hardware_driver);
            }
            break;

        case LORA_STATE_ERROR:
        case LORA_STATE_TIMEOUT:
            // Le moteur a rencontré un problème.
            // En faisant un Reset, on le remet à IDLE.
            // Vu qu'on n'a pas fait "hlorapp->current_step++", à la prochaine boucle,
            // il va renvoyer EXACTEMENT la même commande pour réessayer.
            LoRa_ResetState(hlorapp->hardware_driver);
            break;

        case LORA_STATE_TX:
        case LORA_STATE_RX_WAIT:
        case LORA_STATE_ANALYSIS:
            // Le moteur est en train de travailler (envoi, attente de réponse, ou analyse).
            // Le manager LoRaApp ne fait rien, il attend patiemment.
            break;

        default:
            break;
    }
}
