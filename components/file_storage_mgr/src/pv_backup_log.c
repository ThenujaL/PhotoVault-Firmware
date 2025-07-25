#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#include "pv_sdc.h"
#include "pv_fs.h"
#include "esp_log.h"
#include "pv_logging.h"

#define TAG "PV_UPDATE_LOG"

/* GLOBAL VARIABLES (mainly to reduce stack usage) */
static char log_file_path[LOG_FILE_PATH_NAME_LENGTH];
static char dir_path[DEVICE_DIRECTORY_NAME_MAX_LENGTH] = {0};
static char log_entry[LOG_ENTRY_MAX_LENGTH] = {0};


/***************************************************************************
 * Function:    pv_backup_log_append
 * Purpose:     Updates the backup log with the given filepath that was backed up.
 *              It creates a directory for the serial number if it does not exist.
 * Parameters:  serial_number - The serial number to identify the device.
 *              file_path - The path of file (on the mobile device) that was backed up
 * Returns:     ESP_OK on success
 *              ESP_FAIL else
 * Note:        The log file will caontain entries in the format:
 *              "file_path",<valid_bit> // valid is 1 if the file is not deleted, 0 if it is deleted
 *              The log file will be created in the directory: SD_CARD_BASE_PATH/serial_number
 ***************************************************************************/
esp_err_t pv_backup_log_append(const char *serial_number, const char *file_path) {
    struct stat st = {0};
    FILE *log_file;
    

    PV_LOGD(TAG, "Updating backup log for serial number %s with file path %s", serial_number, file_path);

    snprintf(dir_path, sizeof(dir_path), "%s/%s", SD_CARD_BASE_PATH, serial_number);

    // Check if directory exists
    if (stat(dir_path, &st) != 0) {
        // Directory does not exist, create it
        if (mkdir(dir_path, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
            PV_LOGE(TAG, "Failed to create directory %s", dir_path);
            return ESP_FAIL;
        }
    }

    // Construct full log file path
    PV_LOGD(TAG, "Constructing log file path for serial number %s", serial_number);
    snprintf(log_file_path, LOG_FILE_PATH_NAME_LENGTH, "%s/%s", dir_path, LOG_FILE_NAME);

    log_file = fopen(log_file_path, "a");
    if (!log_file) {
        PV_LOGE(TAG, "Failed to open or create log file");
        return ESP_FAIL;
    }


    // Check if already logged
    PV_LOGD(TAG, "Checking if file %s is already logged for serial number %s", file_path, serial_number);
    if (pv_is_backedUp(serial_number, file_path)) {
        PV_LOGW(TAG, "File %s already logged for serial number %s", file_path, serial_number);
        fclose(log_file);
        return ESP_OK; // File already logged, no need to append
    }

    // Write entry to log: "file_path"
    PV_LOGD(TAG, "Writing log entry for file %s", file_path);
    if (snprintf(log_entry, LOG_ENTRY_MAX_LENGTH, "\"%s\"\n", file_path) >= LOG_ENTRY_MAX_LENGTH) {
        PV_LOGE(TAG, "Log entry exceeds maximum length defined by LOG_ENTRY_MAX_LENGTH");
        fclose(log_file);
        return ESP_FAIL;
    }
    fprintf(log_file, log_entry);
    fclose(log_file);

    PV_LOGD(TAG, "Updated backup log for serial number %s with file path %s", serial_number, file_path);
    
    return ESP_OK;
}

/***************************************************************************
 * Function:    pv_is_backedUp
 * Purpose:     Check if a file is backed up by checking the log file for device 
 *              device with the given serial number. * 
 * Parameters:  serial_number - The serial number to identify the device.
 *              file_path - The path of file (on the mobile device) to check
 * Returns:     true if file is backed up and valid (not deleted)
 *              false else
 ***************************************************************************/
bool pv_is_backedUp(const char *serial_number, const char *file_path) {

    char match_str[LOG_ENTRY_MAX_LENGTH] = {0};
    struct stat st = {0};
    FILE *log_file;

    snprintf(dir_path, sizeof(dir_path), "%s/%s", SD_CARD_BASE_PATH, serial_number);

    // Check if directory exists
    if (stat(dir_path, &st) != 0) {
        // Directory does not exist, therefore file is not backed up
        return false;
    }

    // Construct full log file path
    snprintf(log_file_path, LOG_FILE_PATH_NAME_LENGTH, "%s/%s", dir_path, LOG_FILE_NAME);

    log_file = fopen(log_file_path, "r");
    if (!log_file) {
        PV_LOGE(TAG, "Failed to open log file");
        return false; // Log file does not exist, therefore file is not backed up
    }

    // Construct the match string to search for
    snprintf(match_str, sizeof(match_str), "\"%s\"", file_path);

    // Read the log file line by line to find the file_path
    while (fgets(log_entry, LOG_ENTRY_MAX_LENGTH, log_file) != NULL) {
        // Remove newline if present
        log_entry[strcspn(log_entry, "\r\n")] = 0;

        // Check if the line contains the file_path
        if (strcmp(log_entry, match_str) == 0){
            fclose(log_file);
            return true; // File is backed up            
        }
    }


    fclose(log_file);
    return false; // File not found in log or is marked as deleted
}


/***************************************************************************
 * Function:    pv_get_log_file_length
 * Purpose:     Gets the length of the log file for a given serial number.
 * Parameters:  serial_number - The serial number to identify the device.
 *              file_path - The path of the file to send.
 * Returns:     ESP_OK on success
 *              ESP_FAIL else
 ***************************************************************************/
esp_err_t pv_get_log_file_length(const char *serial_number, uint32_t *length) {

    // Construct full log file path
    snprintf(log_file_path, LOG_FILE_PATH_NAME_LENGTH, "%s/%s/%s", SD_CARD_BASE_PATH, serial_number, LOG_FILE_NAME);

    return pv_get_file_length(log_file_path, length);
}
