#pragma once

#include "esp_err.h"
#include "sdmmc_cmd.h"
#include <sys/stat.h>
#include "esp_vfs_fat.h"

#include "pv_fs.h"
#include "pv_devicelist.h"

// TODO: Move this to a more appropriate file during integration
/* Update Log Defines*/
#define DEVICE_DIRECTORY_NAME_MAX_LENGTH 64
#define BACKUP_PATH_MAX_LENGTH 128
#define LOG_ENTRY_MAX_LENGTH 256*2
#define LOG_FILE_NAME "log.csv"
#define TMP_LOG_FILE_NAME "log.tmp" // Temporary log file name during deletion
#define LOG_FILE_PATH_NAME_LENGTH (DEVICE_DIRECTORY_NAME_MAX_LENGTH + 1 + sizeof(LOG_FILE_NAME))
#define TEST_SERIAL_NUMBER "12345678" // Test serial number for log file
#define DEFAULT_CLIENT_SERIAL_NUMBER "DEFAULTSERVIAL" // Default serial number for log file

/* GLOBAL VARIABLES (mainly to reduce stack usage) */
static char log_file_path[LOG_FILE_PATH_NAME_LENGTH];
static char dir_path[DEVICE_DIRECTORY_NAME_MAX_LENGTH] = {0};
static char read_log_entry[LOG_ENTRY_MAX_LENGTH] = {0};
static char write_log_entry[LOG_ENTRY_MAX_LENGTH] = {0};

/* FUNCTION DEFS */
esp_err_t pv_init_sdc(void);
void pv_test_sdc(void);
void pv_card_get(sdmmc_card_t **out_card);
esp_err_t pv_backup_log_append(pv_android_device_id_t android_id, const char *file_path_local, const char *file_path_remote);
bool pv_is_backedUp(pv_android_device_id_t android_id, const char *file_path_local, const char *file_path_remote); // TODO: Move this to a more appropriate file during integration
esp_err_t pv_get_log_file_length(uint32_t *length);
void parse_log_entry(const char* log_entry, char* file_path_local, char* file_path_remote);
esp_err_t pv_delete_log_entry(pv_android_device_id_t android_id, const char*local_file_path, const char *remote_file_path);
void pv_get_local_path_from_remote(pv_android_device_id_t android_id, const char* remote_path, char* local_path, size_t local_path_size);
void pv_get_remote_path_from_local(pv_android_device_id_t android_id, const char* local_path, char* remote_path, size_t remote_path_size);
esp_err_t pv_create_temp_log(pv_android_device_id_t android_id);
esp_err_t pv_construct_log_entry(const char *file_path_local, const char *file_path_remote);