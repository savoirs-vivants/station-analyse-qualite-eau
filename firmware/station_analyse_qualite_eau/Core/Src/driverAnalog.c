#include "driverAnalog.h"
#include <string.h>

#if ANALOG_DEBUG_ENABLE
    #include <stdio.h>
    #define ANALOG_PRINT(...) printf("[ANALOG] " __VA_ARGS__)
#else
    #define ANALOG_PRINT(...) // Remplacé par du vide si désactivé
#endif

Analog_Result_t Analog_Init(Analog_t *hanalog, ADC_HandleTypeDef *hadc)
{
    if (hanalog == NULL || hadc == NULL) return ANALOG_ERR_PARAM;

    memset(hanalog, 0, sizeof(Analog_t));
    hanalog->hadc = hadc;
    hanalog->state = ANALOG_IDLE;
    hanalog->status = ANALOG_STATUS_READY;

    ANALOG_PRINT("Lancement calibration ADC...\r\n");

    if (HAL_ADCEx_Calibration_Start(hadc) != HAL_OK) {
        ANALOG_PRINT("CRITIQUE : Echec de la calibration ADC !\r\n");
        hanalog->status = ANALOG_STATUS_ERROR;
        return ANALOG_ERR_HAL;
    }

    uint32_t cal_val = HAL_ADCEx_Calibration_GetValue(hadc);
    ANALOG_PRINT("Calibration ADC OK (Offset de compensation : %lu).\r\n", cal_val);

    return ANALOG_OK;
}

Analog_Result_t Analog_Start(Analog_t *hanalog)
{
    if (hanalog == NULL) return ANALOG_ERR_PARAM;
    if (hanalog->state != ANALOG_IDLE) return ANALOG_ERR_BUSY;

    hanalog->event_adc_complete = false;
    hanalog->error_code = 0;
    hanalog->status = ANALOG_STATUS_BUSY;
    hanalog->state = ANALOG_START_CONVERSION;

    return ANALOG_OK;
}

void Analog_Process(Analog_t *hanalog)
{
    if (hanalog == NULL || hanalog->state == ANALOG_IDLE) return;

    switch(hanalog->state)
    {
        case ANALOG_START_CONVERSION:
            HAL_ADC_Stop_DMA(hanalog->hadc);
            if (HAL_ADC_Start_DMA(hanalog->hadc, (uint32_t*)hanalog->ADC_values, NBR_ANALOG_SENSOR) != HAL_OK) {
                hanalog->error_code = 1;
                hanalog->status = ANALOG_STATUS_ERROR;
                hanalog->state = ANALOG_IDLE;
            } else {
                hanalog->start_time = HAL_GetTick();
                hanalog->state = ANALOG_WAIT_CONVERSION;
            }
            break;

        case ANALOG_WAIT_CONVERSION:
            if (hanalog->event_adc_complete) {
                hanalog->state = ANALOG_PROCESS_DATA;
            }
            else if ((HAL_GetTick() - hanalog->start_time) > 50) { // Timeout 50ms
                HAL_ADC_Stop_DMA(hanalog->hadc);
                hanalog->error_code = 2;
                hanalog->status = ANALOG_STATUS_ERROR;
                hanalog->state = ANALOG_IDLE;
            }
            break;


		// /!\ Comme on fonciotnne en sequenceur not fully configurable, cela se fait dans l'odre croissant des channels
        case ANALOG_PROCESS_DATA:
            /* 1. Calcul de la vraie tension VDDA via VREFINT (Rank 3 -> Index 2) */
            hanalog->true_vdda_mv = __HAL_ADC_CALC_VREFANALOG_VOLTAGE(hanalog->ADC_values[2], ADC_RESOLUTION_12B);

            /* 2. Quantum dynamique */
            float quantum = ((float)hanalog->true_vdda_mv / 1000.0f) / ADC_MAX_RESOLUTION_12B;

            /* 3. Capteurs externes (Rank 1 & 2 -> Index 0 & 1) */
            for(int i=0; i<2; i++) {
                hanalog->VOLT_values[i] = (float)hanalog->ADC_values[i] * quantum * ATTENUATOR_MULTIPLICATOR;
            }

            /* 4. Température Interne*/
            //hanalog->internal_temp = __HAL_ADC_CALC_TEMPERATURE(hanalog->true_vdda_mv, hanalog->ADC_values[3], ADC_RESOLUTION_12B);

            /* 5. Tension Batterie (pas VBAT !) */
			hanalog->v_battery = hanalog->ADC_values[3];



            hanalog->status = ANALOG_STATUS_READY;
            hanalog->state = ANALOG_IDLE;
            break;

        default:
            hanalog->state = ANALOG_IDLE;
            break;
    }
}

Analog_Status_t Analog_GetStatus(Analog_t *hanalog) {
    if (hanalog == NULL) return ANALOG_STATUS_ERROR;
    return hanalog->status;
}

void Analog_ConvCpltCallback(Analog_t *hanalog) {
    if (hanalog) hanalog->event_adc_complete = true;
}
