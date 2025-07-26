#pragma once

#include "esp_err.h"

#define SD_CARD_BASE_PATH               "/sdcard"                   // Base path for the SD card

#define SD_CARD_MOUNT_POINT             SD_CARD_BASE_PATH           // Mount point for the SD card
#define SD_CARD_DRIVE_NUM               0U                          // Drive number for the SD card

#define FATFS_MAX_FILES                 4U                          // Maximum number of files that can be opened simultaneously
#define FATFS_WORKBUF_SIZE              4096U                       // 4KB work buffer size for FATFS operations
#define FATFS_ALLOCATION_UNIT_SIZE      4U * 1024U                  // 4KB allocation unit size

#define FORMAT_SD_CARD_ON_MOUNT_FAIL    1U                          // Format SD card if mounting fails


/* FUNCTION DEFS */
esp_err_t pv_init_fs(void);
esp_err_t pv_fmt_sdc(void);
esp_err_t pv_delete_dir(const char *path);
esp_err_t pv_get_file_length(const char *file_path, uint32_t *length);
esp_err_t pv_create_file(const char *file_path);