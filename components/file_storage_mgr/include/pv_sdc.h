#pragma once

#include "esp_err.h"
#include "sdmmc_cmd.h"
#include <sys/stat.h>
#include "esp_vfs_fat.h"

#include "pv_fs.h"
#include "pv_bt_utils.h"

// TODO: Move this to a more appropriate file during integration
/* Update Log Defines*/
#define DEVICE_DIRECTORY_NAME_MAX_LENGTH 64
#define BACKUP_PATH_MAX_LENGTH 256
#define LOG_ENTRY_MAX_LENGTH 256
#define LOG_FILE_NAME "log.csv"
#define TMP_LOG_FILE_NAME "log.tmp" // Temporary log file name during deletion
#define LOG_FILE_PATH_NAME_LENGTH (DEVICE_DIRECTORY_NAME_MAX_LENGTH + 1 + sizeof(LOG_FILE_NAME))
#define TEST_SERIAL_NUMBER "12345678" // Test serial number for log file
#define DEFAULT_CLIENT_SERIAL_NUMBER "DEFAULTSERVIAL" // Default serial number for log file

/**
 * @brief Structure to hold file metadata for backup and transfer operations.
 */
typedef struct {
    char remote_file_path[BACKUP_PATH_MAX_LENGTH + 1]; /* Path of file on the mobile device, as specified in the metadata json (+1 for null terminator)*/
    char local_file_path[sizeof(SD_CARD_BASE_PATH) + 1 + ANDROID_ID_NUM_DIGITS + 1 + BACKUP_PATH_MAX_LENGTH + 1]; /* Absolute path on device, including mount point and device directory (SD_CARD_BASE_PATH/ANDROID_ID/remote_file_path), +1 for slashes and null terminator */
    uint32_t file_size;
    char new_remote_file_path[BACKUP_PATH_MAX_LENGTH + 1];     /* Used for renaming files, otherwise left empty */
    char new_local_file_path[sizeof(SD_CARD_BASE_PATH) + 1 + ANDROID_ID_NUM_DIGITS + 1 + BACKUP_PATH_MAX_LENGTH + 1];
} pv_file_metadata_t;

/* FUNCTION DEFS */
esp_err_t pv_init_sdc(void);
void pv_test_sdc(void);
void pv_card_get(sdmmc_card_t **out_card);
esp_err_t pv_backup_log_append(const char *serial_number, const char *file_path); // TODO: Move this to a more appropriate file during integration
bool pv_is_backedUp(const char *serial_number, const char *file_path); // TODO: Move this to a more appropriate file during integration
esp_err_t pv_get_log_file_length(const char *serial_number, uint32_t *length);
esp_err_t pv_delete_log_entry(const char *serial_number, const char *file_path);