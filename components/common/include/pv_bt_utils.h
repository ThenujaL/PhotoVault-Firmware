#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "esp_bt_defs.h"


#define MAX_DEVICE_CONNECTIONS 7
#define ANDROID_ID_NUM_DIGITS 16


/**
 * @brief Compares two Bluetooth device addresses for equality.
 * @param a First Bluetooth device address.
 * @param b Second Bluetooth device address.
 * @return true if the addresses are equal, false otherwise.
 */
static inline bool pv_cmp_db_addr(esp_bd_addr_t a, esp_bd_addr_t b){

    for(int i = 0; i < ESP_BD_ADDR_LEN; i++){
        if (a[i] != b[i]) return false;
    }

    return true;

}

/**
 * @brief Converts a Bluetooth device address to a string representation.
 * @param bda The Bluetooth device address.
 * @param str The output string buffer (must be at least 18 bytes).
 * @param size The size of the output string buffer.
 * @return Pointer to the output string buffer, or NULL on error.
 */
static inline char *bda2str(const esp_bd_addr_t bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    return str;
}

/**
 * @brief Compares a command string with received data.
 * @param CMD The command string to compare.
 * @param DATA The received data to compare against.
 * @param len The length of the received data.
 * @return true if the command matches the data, false otherwise.
 */
static inline bool cmd_compare(char * CMD, uint8_t * DATA, uint16_t len)
{
    for(int i = 0; i<len; i++)
    {
        if(CMD[i] != DATA[i])
        {
            return false;
        }
    }
    return true;
}