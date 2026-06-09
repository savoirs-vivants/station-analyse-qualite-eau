#include "driverLPPCayenne.h"

/* Helper interne pour sécuriser (clamper) les valeurs flottantes et éviter les débordements d'entiers */
#define CLAMP(val, min, max)  ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

LPP_Result_t LPP_Init(LPP_Payload_t *payload)
{
    if (payload == NULL) return LPP_ERR_PARAM;
    payload->cursor = 0;
    return LPP_OK;
}

LPP_Result_t LPP_AddTemperature(LPP_Payload_t *payload, uint8_t channel, float celsius)
{
    if (payload == NULL) return LPP_ERR_PARAM;

    /* Vérification de la place disponible (Canal:1 + Type:1 + Data:2 = 4 octets) */
    if (payload->cursor + 4 > LPP_MAX_PAYLOAD_SIZE) return LPP_ERR_SIZE;

    /* Sécurité anti-overflow : LPP Temp utilise un int16_t avec res 0.1°C
       Limites mathématiques absolues : -3276.8°C à +3276.7°C */
    float safe_celsius = CLAMP(celsius, -3276.8f, 3276.7f);
    int16_t temp_val = (int16_t)(safe_celsius * 10.0f);

    payload->buffer[payload->cursor++] = channel;
    payload->buffer[payload->cursor++] = LPP_TYPE_TEMPERATURE;
    payload->buffer[payload->cursor++] = (uint8_t)(temp_val >> 8);    // MSB
    payload->buffer[payload->cursor++] = (uint8_t)(temp_val & 0xFF);  // LSB

    return LPP_OK;
}

LPP_Result_t LPP_AddAnalogInput(LPP_Payload_t *payload, uint8_t channel, float value)
{
    if (payload == NULL) return LPP_ERR_PARAM;
    if (payload->cursor + 4 > LPP_MAX_PAYLOAD_SIZE) return LPP_ERR_SIZE;

    /* Sécurité anti-overflow : LPP Analog utilise un int16_t avec res 0.01
       Limites mathématiques absolues : -327.68 à +327.67 */
    float safe_value = CLAMP(value, -327.68f, 327.67f);
    int16_t analog_val = (int16_t)(safe_value * 100.0f);

    payload->buffer[payload->cursor++] = channel;
    payload->buffer[payload->cursor++] = LPP_TYPE_ANALOG_INPUT;
    payload->buffer[payload->cursor++] = (uint8_t)(analog_val >> 8);    // MSB
    payload->buffer[payload->cursor++] = (uint8_t)(analog_val & 0xFF);  // LSB

    return LPP_OK;
}

LPP_Result_t LPP_AddHumidity(LPP_Payload_t *payload, uint8_t channel, float rh_percentage)
{
    if (payload == NULL) return LPP_ERR_PARAM;

    /* Vérification de la place disponible (Canal:1 + Type:1 + Data:1 = 3 octets) */
    if (payload->cursor + 3 > LPP_MAX_PAYLOAD_SIZE) return LPP_ERR_SIZE;

    /* Sécurité anti-overflow : LPP Hum utilise un uint8_t avec res 0.5%
       Limites mathématiques : 0.0% à 100.0% (0 à 200 en encodé) */
    float safe_rh = CLAMP(rh_percentage, 0.0f, 100.0f);
    uint8_t hum_val = (uint8_t)(safe_rh * 2.0f);

    payload->buffer[payload->cursor++] = channel;
    payload->buffer[payload->cursor++] = LPP_TYPE_HUMIDITY;
    payload->buffer[payload->cursor++] = hum_val;

    return LPP_OK;
}

int LPP_GetATCommand(LPP_Payload_t *payload, char *at_cmd_buffer, size_t max_len)
{
    if (payload == NULL || at_cmd_buffer == NULL) return -1;

    /* Calcul taille requise : "AT+MSGHEX=\"" (11) + Hex (cursor*2) + "\"\r\n\0" (4) */
    size_t required_len = 11 + (payload->cursor * 2) + 4;

    /* Vérification de la place disponible dans le buffer de destination */
    if (max_len < required_len) return -1;

    char *ptr = at_cmd_buffer;
    const char hex_chars[] = "0123456789ABCDEF";

    /* 1. Ajout de l'en-tête AT */
    const char *prefix = "AT+MSGHEX=\"";
    while (*prefix) {
        *ptr++ = *prefix++;
    }

    /* 2. Conversion binaire vers chaîne Hexadécimale */
    for (size_t i = 0; i < payload->cursor; i++) {
        *ptr++ = hex_chars[(payload->buffer[i] >> 4) & 0x0F];
        *ptr++ = hex_chars[payload->buffer[i] & 0x0F];
    }

    /* 3. Clôture de la commande AT */
    *ptr++ = '"';
    *ptr++ = '\r';
    *ptr++ = '\n';
    *ptr++ = '\0';

    return (int)(ptr - at_cmd_buffer - 1);
}

int LPP_GetPayloadHex(LPP_Payload_t *payload, char *hex_buffer, size_t max_len)
{
    if (payload == NULL || hex_buffer == NULL) return -1;

    /* Calcul taille requise : Hex (cursor*2) + '\0' (1) */
    size_t required_len = (payload->cursor * 2) + 1;

    if (max_len < required_len) return -1;

    char *ptr = hex_buffer;
    const char hex_chars[] = "0123456789ABCDEF";

    for (size_t i = 0; i < payload->cursor; i++) {
        *ptr++ = hex_chars[(payload->buffer[i] >> 4) & 0x0F];
        *ptr++ = hex_chars[payload->buffer[i] & 0x0F];
    }

    *ptr = '\0';

    return (int)(ptr - hex_buffer);
}
