/*
 * driverLoRaApp.h
 *
 *  Created on: 8 avr. 2026
 *      Author: valen
 */

#ifndef INC_DRIVERLORAAPP_H_
#define INC_DRIVERLORAAPP_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "driverLoRa.h"

typedef struct {
    const char *cmd;
    uint32_t timeout_ms;
} LoRa_Command_t;

typedef struct {
    LoRa_t *hardware_driver; // Lien vers le Moteur
    const LoRa_Command_t *sequence;      // La liste des commandes
    uint8_t total_steps;                 // Nombre de commandes
    uint8_t current_step;
    bool is_ready;
} LoRaWAN_Manager_t;

void LoRa_App_Init(LoRaWAN_Manager_t *hlorapp, LoRa_t *hlora, const LoRa_Command_t *cmds);
void LoRa_App_Process(LoRaWAN_Manager_t *hlorapp);

#endif /* INC_DRIVERLORAAPP_H_ */
