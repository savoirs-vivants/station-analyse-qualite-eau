/*
 * driverRTC.h
 *
 *  Created on: 7 mai 2026
 *      Author: valen
 */

#ifndef INC_DRIVERRTC_H_
#define INC_DRIVERRTC_H_

#include "rtc.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ==========================================
 * CONFIGURATION DE L'HEURE DE COMPILATION
 * ========================================== */
// Mettre à 1 si vous compilez pendant l'heure d'été (Avril à Octobre)
// Mettre à 0 si vous compilez pendant l'heure d'hiver (Novembre à Mars)
#define IS_SUMMER_TIME 1

void configRTC(RTC_HandleTypeDef *hrtc, RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate);
uint32_t Get_UNIX_Timestamp(RTC_TimeTypeDef *sTime_read, RTC_DateTypeDef *sDate_read);


#endif /* INC_DRIVERRTC_H_ */
