/*
 * driverRTC.c
 *
 *  Created on: 7 mai 2026
 *      Author: valen
 */

#include "driverRTC.h"

/* Fonction utilitaire pour convertir "May" en 5 */
static uint8_t Parse_Month(const char *monthStr) {
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (uint8_t i = 0; i < 12; i++) {
        if (strncmp(monthStr, months[i], 3) == 0) {
            return i + 1;
        }
    }
    return 1; // Janvier par défaut en cas d'erreur
}

void configRTC(RTC_HandleTypeDef *hrtc, RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate)
{
	char monthStr[4];
	    int day, year, hours, minutes, seconds;

	    sscanf(__DATE__, "%s %d %d", monthStr, &day, &year);
	    sscanf(__TIME__, "%d:%d:%d", &hours, &minutes, &seconds);

	    int offset = IS_SUMMER_TIME ? 2 : 1;

		hours = hours - offset;

		// Gestion du passage sous zéro (minuit)
		if (hours < 0) {
			hours = hours + 24;
			day = day - 1;
		}

	    sTime->Hours = hours;
	    sTime->Minutes = minutes;
	    sTime->Seconds = seconds;
	    sTime->DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	    sTime->StoreOperation = RTC_STOREOPERATION_RESET;

	    if (HAL_RTC_SetTime(hrtc, sTime, RTC_FORMAT_BIN) != HAL_OK) {
	        Error_Handler();
	    }

	    // 3. Remplir la structure Date
	    sDate->Date = day;
	    sDate->Month = Parse_Month(monthStr);
	    sDate->Year = year - 2000; // Le STM32 attend une année entre 0 et 99
	    // Note: Le calcul du jour de la semaine (WeekDay) nécessiterait un petit
	    // algorithme supplémentaire (ex: congruence de Zeller), mais il est
	    // souvent facultatif si vous ne vous servez que du timestamp UNIX.
	    sDate->WeekDay = RTC_WEEKDAY_MONDAY;

	    if (HAL_RTC_SetDate(hrtc, sDate, RTC_FORMAT_BIN) != HAL_OK) {
	        Error_Handler();
	    }
}

// Fonction pour convertir les structures HAL RTC en Timestamp UNIX
uint32_t Get_UNIX_Timestamp(RTC_TimeTypeDef *sTime_read, RTC_DateTypeDef *sDate_read)
{
    struct tm timeinfo;

    // Lire le calendrier RTC
    HAL_RTC_GetTime(&hrtc, sTime_read, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, sDate_read, RTC_FORMAT_BIN);

    // Remplir la structure standard 'tm'
    timeinfo.tm_sec   = sTime_read->Seconds;
    timeinfo.tm_min   = sTime_read->Minutes;
    timeinfo.tm_hour  = sTime_read->Hours;
    timeinfo.tm_mday  = sDate_read->Date;
    timeinfo.tm_mon   = sDate_read->Month - 1;       // tm_mon va de 0 à 11
    timeinfo.tm_year  = sDate_read->Year + 100;      // tm_year compte depuis 1900 (2000 + Year - 1900)
    timeinfo.tm_isdst = -1;                         // Détection automatique de l'heure d'été

    // Convertir en Timestamp UNIX
    time_t timestamp = mktime(&timeinfo);

    return (uint32_t)timestamp;
}
