#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_bt_defs.h"

#include "pv_devicelist.h"


#define PV_PIN_LENGTH                         4  /* Length of PhotoVault PIN in bytes */
#define PV_PIN_PATH                                 SD_CARD_BASE_PATH "/pv_pin.txt"

typedef uint8_t pv_pin_t[PV_PIN_LENGTH];

typedef enum {
    PV_AUTH_SUCCESS = 0,
    PV_AUTH_ERR,
    PV_AUTH_SET_UP_REQUIRED,
} pv_auth_err_t;

esp_err_t pv_pin_init(void);
bool pv_cmp_pin(const pv_pin_t pin);
esp_err_t pv_set_pin(const pv_pin_t pin);
esp_err_t pv_add_connection(uint32_t handle, esp_bd_addr_t bd_addr);
void pv_remove_connection_by_id(pv_android_device_id_t android_id);
esp_err_t pv_remove_connection(uint32_t handle);
bool pv_is_device_authorized(uint32_t handle);
esp_err_t pv_set_authenticated(uint32_t handle, pv_android_device_id_t android_id, bool authenticated);
esp_err_t pv_get_bda_from_handle(uint32_t handle, esp_bd_addr_t out_bda);
pv_auth_err_t pv_auth_cmd_handler(uint8_t *data, uint16_t len, uint32_t handle);


/**
 * @brief Converts a PhotoVault PIN from byte array format to string format.
 * @param pin The PIN in byte array format.
 * @param str The output string buffer (must be at least PV_PIN_BYTES_LENGTH + 1 bytes).
 * @param size The size of the output string buffer.
 */
static inline void pv_pin2str(const pv_pin_t pin, char *str, size_t size) {
    if (str == NULL || size < (PV_PIN_LENGTH + 1)) {
        return;
    }

    for (int i = 0; i < size; i++) {
        sprintf(str + i, "%c", '0' + pin[i]);
    }

    str[PV_PIN_LENGTH] = '\0'; // Null terminator
}