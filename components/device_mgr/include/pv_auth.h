#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_bt_defs.h"

#include "pv_devicelist.h"


#define PV_PIN_BYTES_LENGTH                         4  /* Length of PhotoVault PIN in bytes */
#define PV_PIN_PATH                                 SD_CARD_BASE_PATH "/pv_pin.txt"

typedef uint8_t pv_pin_t[PV_PIN_BYTES_LENGTH];

bool pv_cmp_pin(const pv_pin_t pin);
esp_err_t pv_set_pin(const pv_pin_t pin);
esp_err_t pv_add_connection(uint32_t handle, esp_bd_addr_t bd_addr);
esp_err_t pv_remove_connection(uint32_t handle);
bool pv_is_device_authorized(uint32_t handle);
esp_err_t pv_set_authenticated(uint32_t handle, pv_android_device_id_t android_id, bool authenticated);
esp_err_t pv_get_bda_from_handle(uint32_t handle, esp_bd_addr_t out_bda);