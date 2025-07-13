#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sd_test_io.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"

#define MOUNT_POINT "/sdcard"
#define CONFIG_EXAMPLE_PIN_MOSI 15
#define CONFIG_EXAMPLE_PIN_MISO 2
#define CONFIG_EXAMPLE_PIN_CLK 14
#define CONFIG_EXAMPLE_PIN_CS 13
extern void init_sd_card();
extern void deinit_sd_card();
// extern esp_err_t s_example_write_file(const char *path, char *data)