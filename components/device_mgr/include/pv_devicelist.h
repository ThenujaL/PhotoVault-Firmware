#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_bt_defs.h"

#include "pv_fs.h"

#define DEVICE_LIST_MAX_SIZE                    50                                      /* Maximum number actice device */

#define DEVICE_LIST_PATH                        SD_CARD_BASE_PATH "/deviceList.csv"
#define PV_PIN_PATH                             SD_CARD_BASE_PATH "/pv_pin.txt"
#define PV_DEVICE_NAME_MAX_LENGTH               128                                     /* Maximum length of device name string */
#define BD_ADDR_STR_LENGTH                      18                                      /* Length of Bluetooth Device Address string "00:11:22:33:44:55" + null terminator */

esp_err_t pv_device_list_create(void);
int pv_device_list_get_count(void);
esp_err_t pv_device_list_add_device(esp_bd_addr_t bd_addr, const char *new_name);
esp_err_t pv_device_list_delete_device(esp_bd_addr_t bd_addr);
bool pv_device_list_id_exists(esp_bd_addr_t bd_addr);
bool pv_device_list_get_name_by_id(esp_bd_addr_t bd_addr, char *out_name, size_t name_buf_size) ;


/* Tests */
void pv_test_devicelist(void);