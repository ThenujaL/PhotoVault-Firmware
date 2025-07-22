#pragma once

#include "esp_err.h"
#include "sdmmc_cmd.h"
#include <sys/stat.h>
#include "esp_vfs_fat.h"

#include "pv_fs.h"

// TODO: Move this to a more appropriate file during integration
/* Update Log Defines*/
#define DEVICE_DIRECTORY_NAME_MAX_LENGTH 64
#define BACKUP_PATH_MAX_LENGTH 128
#define LOG_ENTRY_MAX_LENGTH 256
#define LOG_FILE_NAME "log.csv"

/* FUNCTION DEFS */
esp_err_t pv_init_sdc(void);
void pv_test_sdc(void);
void pv_card_get(sdmmc_card_t **out_card);
esp_err_t pv_update_backup_log(const char *serial_number, const char *file_path); // TODO: Move this to a more appropriate file during integration
bool pv_is_backedUp(const char *serial_number, const char *file_path); // TODO: Move this to a more appropriate file during integration