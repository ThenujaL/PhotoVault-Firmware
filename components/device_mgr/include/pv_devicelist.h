#pragma once

#include <stdbool.h>

#include "esp_err.h"

#include "pv_fs.h"


# define DEVICE_LIST_PATH               SD_CARD_BASE_PATH "/deviceList.csv"

esp_err_t pv_device_list_create(void);
esp_err_t pv_device_list_add_device(const char *device_name, int *device_id_out);
esp_err_t pv_device_list_update_name(const int device_id, const char *new_name);
esp_err_t pv_device_list_delete_device(const int device_id);
bool pv_device_list_name_exists(const char *device_name);


/* Tests */
void pv_test_devicelist(void);