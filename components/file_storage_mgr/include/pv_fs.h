#pragma once

#include "esp_err.h"

#define SD_CARD_BASE_PATH               "/sdcard"
#define SD_CARD_MOUNT_POINT             SD_CARD_BASE_PATH
#define SD_CARD_DRIVE_NUM               0U

#define FATFS_MAX_FILES                 1U
#define FATFS_WORKBUF_SIZE              4096U
#define FATFS_ALLOCATION_UNIT_SIZE      4U * 1024U // 4KB allocation unit size

#define FORMAT_SD_CARD_ON_MOUNT         1U



esp_err_t pv_init_fs(void);