#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_bt_defs.h"

#include "pv_bt_utils.h"
#include "pv_devicelist.h"
#include "pv_logging.h"
#include "pv_fs.h"
#include "pv_sdc.h"

#define TAG "PV_DEVICELIST"

// #define DEVICE_DATA_FILE_PATH       SD_CARD_BASE_PATH "/deviceListData.txt"  /* File only stores the device count */
#define TEMP_DEVICE_DATA_FILE_PATH  SD_CARD_BASE_PATH "/deviceListData_temp.csv"


/**
 * @brief Initializes the device list by checking if the device list file exists and creating it if it does not.
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t pv_device_list_init(void){
    /* Check if device list file exists, if not create it */
    struct stat st;
    if (stat(DEVICE_LIST_PATH_INTERNAL, &st) != 0) {
        PV_LOGW(TAG, "Device list file does not exist at %s, creating new file", DEVICE_LIST_PATH_INTERNAL);
        return pv_device_list_create();
    }

    PV_LOGI(TAG, "Device list file already exists at %s", DEVICE_LIST_PATH_INTERNAL);
    return ESP_OK;
}

/**
 * @brief Creates or resets the device list CSV file.
 * @param None
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t pv_device_list_create(void){

    /* Device List Format */
    /*
    bda,android_id,device_name
    "ff:ff:ff:ff:78:f2",1234567812345678,"John's Phones"
    */

    /* Delete old file if exists */
    remove(DEVICE_LIST_PATH_INTERNAL);

    /* Create a new file */
    FILE *fp = fopen(DEVICE_LIST_PATH_INTERNAL, "w");
    if (fp == NULL) {
        PV_LOGE(TAG, "Failed to create device list file at %s", DEVICE_LIST_PATH_INTERNAL);
        return ESP_FAIL;
    }
    
    /* Write the header line (no space after comma) */
    fprintf(fp, "bda,android_id,device_name\n");

    /* Close the file */
    if (fclose(fp) != 0) {
        PV_LOGE(TAG, "Failed to close device list file at %s", DEVICE_LIST_PATH_INTERNAL);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Gets the number of devices in the device list.
 * @param None
 * @return Number of devices on success, -1 on failure.
 */
int pv_device_list_get_count(void){
    int count = 0;

    FILE *fp = fopen(DEVICE_LIST_PATH_INTERNAL, "r");
    if (fp == NULL) {
        PV_LOGE(TAG, "Failed to open device list file at %s", DEVICE_LIST_PATH_INTERNAL);
        return -1;
    }

    char line[256];

    /* Skip header */
    fgets(line, sizeof(line), fp);

    /* Count lines */
    while (fgets(line, sizeof(line), fp) != NULL) {
        count++;
    }

    fclose(fp);
    return count;
}


/**
 * @brief Make a copy of the internal device list, but excluding the bda column for transmission to android devices.
 *        This is to conform to not exposing bda to android devices.
 */
static esp_err_t pv_device_list_copy_public(void) {
    FILE *src = fopen(DEVICE_LIST_PATH_INTERNAL, "r");
    if (!src) {
        PV_LOGE(TAG, "Failed to open internal device list for public copy");
        return ESP_FAIL;
    }
    FILE *dst = fopen(DEVICE_LIST_PATH_PUBLIC, "w");
    if (!dst) {
        PV_LOGE(TAG, "Failed to open public device list for writing");
        fclose(src);
        return ESP_FAIL;
    }

    char line[256];
    int line_number = 0;
    while (fgets(line, sizeof(line), src)) {
        if (line_number == 0) {
            // Write new header without bda
            fprintf(dst, "android_id,device_name\n");
        } else {
            pv_android_device_id_t android_id;
            char device_name[PV_DEVICE_NAME_MAX_LENGTH];
            // Parse and write only android_id and device_name
            if (sscanf(line, "\"%*127[^\"]\",%" PRIu64 ",\"%127[^\"]\"", &android_id, device_name) == 2) {
                fprintf(dst, "%" PRIu64 ",\"%s\"\n", android_id, device_name);
            }
        }
        line_number++;
    }
    fclose(src);
    fclose(dst);

    return ESP_OK;
}

/**
 * @brief Checks if a device with the specified android_id exists in the device list.
 * @param android_id The android_id to check for.
 * @param new_name The name to assign to the device if it is being added for the first time or updated.
 * @return ESP_OK on success, ESP_FAIL if the device is not found or on file operation errors.
 */
esp_err_t pv_device_list_update_device_name(pv_android_device_id_t android_id, const char* new_name) {
    return pv_device_list_add_device(NULL, android_id, new_name); // bda is not needed for name update
}

/**
 * @brief Adds the name of a device in the device list. If the device already exists, its name is updated.
 *
 * Searches the device list file for the entry with the specified android_id and updates its name to the provided new name.
 * The function creates a temporary file to store the updated list, replaces the original file upon success, and ensures data integrity.
 *
 * @param bda The Bluetooth device address of the device to add or update in the device list.
 * @param android_id The android device ID of the device whose name should be updated.
 * @param new_name The new name to assign to the device.
 * @return ESP_OK on success, ESP_FAIL if the device is not found or on file operation errors.
 */
esp_err_t pv_device_list_add_device(const esp_bd_addr_t bda, pv_android_device_id_t android_id, const char *new_name){
    esp_err_t err = ESP_OK;

    FILE *fp = fopen(DEVICE_LIST_PATH_INTERNAL, "r");
    if (fp == NULL) {
        PV_LOGE(TAG, "Failed to open device list file at %s", DEVICE_LIST_PATH_INTERNAL);
        return ESP_FAIL;
    }

    
    /* Create temporary file */
    FILE *temp_fp = fopen(TEMP_DEVICE_DATA_FILE_PATH, "w");
    if (temp_fp == NULL) {
        PV_LOGE(TAG, "Failed to create temp file");
        fclose(fp);
        return ESP_FAIL;
    }

    char line[256];
    bool found = false;
    int line_number = 0;

    /* Copy all lines, updating the target line */
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line_number == 0) {
            /* Copy header as-is */
            fputs(line, temp_fp);
            line_number++;
            continue;
        }

        pv_android_device_id_t current_id;
        char current_name[PV_DEVICE_NAME_MAX_LENGTH];
        char current_bda[BD_ADDR_STR_LENGTH];        
        
        /* Parse the line (no space after comma) */
        if (sscanf(line, "\"%17[^\"]\",%" PRIu64 ",\"%127[^\"]\"", current_bda, &current_id, current_name) == 3) {
            if (current_id == android_id) {
                /* Write updated line (no space after comma) */
                fprintf(temp_fp, "\"%s\",%" PRIu64 ",\"%s\"\n", current_bda, android_id, new_name);
                PV_LOGI(TAG, "Updated device with android_id %" PRIu64 " bda %s to have name %s", android_id, current_bda, new_name);
                found = true;
            } else {
                /* Copy unchanged */
                fputs(line, temp_fp);
            }
        } else {
            /* Copy malformed lines as-is */
            fputs(line, temp_fp);
        }
        
        line_number++;
    }

    /* If not updated, it is a new device -> append android_id,name */
    /* Create Log file */
    if (!found) {
        if (bda) {
            char new_bda[BD_ADDR_STR_LENGTH];
            bda2str(bda, new_bda, sizeof(new_bda));
            fprintf(temp_fp, "\"%s\",%" PRIu64 ",\"%s\"\n", new_bda, android_id, new_name);
            PV_LOGI(TAG, "Added new device with android_id %" PRIu64 " bda %s and name %s to device list", android_id, new_bda, new_name);            
        } else {
            PV_LOGW(TAG, "BDA not provided for device with android_id %" PRIu64 " and device not found in device list for perform name update.", android_id);
        }

    }

    fclose(fp);
    fclose(temp_fp);

    


    /* Replace original with temp file */
    remove(DEVICE_LIST_PATH_INTERNAL);
    rename(TEMP_DEVICE_DATA_FILE_PATH, DEVICE_LIST_PATH_INTERNAL);


    if(!found)
    {
        /* creating log file */

        /* Update entries all other devices' log files*/
        FILE *fp = fopen(DEVICE_LIST_PATH_INTERNAL, "r");
        if (fp == NULL) {
            PV_LOGE(TAG, "Failed to open device list file at %s", DEVICE_LIST_PATH_INTERNAL);
            return ESP_FAIL;
        }

        char line[256];
        /* Skip header */
        fgets(line, sizeof(line), fp);

        while (fgets(line, sizeof(line), fp) != NULL) {
            char bda[BD_ADDR_STR_LENGTH];
            pv_android_device_id_t other_android_id;
            char device_name[PV_DEVICE_NAME_MAX_LENGTH];
            if (ESP_OK != pv_parse_device_list_entry(line, bda, &other_android_id, device_name)) {
                PV_LOGE(TAG, "Failed to parse device list entry: %s", line);
                continue;
            }

            struct stat st = {0};
            FILE *log_file;

            snprintf(dir_path, sizeof(dir_path), "%s/%llu", SD_CARD_BASE_PATH, android_id);

            // Check if directory exists
            if (stat(dir_path, &st) != 0) {
                // Directory does not exist, create it
                if (mkdir(dir_path, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
                    PV_LOGE(TAG, "Failed to create directory %s", dir_path);
                    return ESP_FAIL;
                }
            }

            // Construct full log file path
            PV_LOGD(TAG, "Constructing log file for first time for serial number %llu", android_id);
            snprintf(log_file_path, LOG_FILE_PATH_NAME_LENGTH, "%s/%s", dir_path, LOG_FILE_NAME);

            log_file = fopen(log_file_path, "r");
            if (!log_file) {
                PV_LOGE(TAG, "Failed to open log file");
                return false; // Log file does not exist, therefore file is not backed up
            }

            // Read the log file line by line and copy to new log file
            while (fgets(read_log_entry, LOG_ENTRY_MAX_LENGTH, log_file) != NULL) {
                char old_local_path[LOG_ENTRY_MAX_LENGTH];
                char old_remote_path[LOG_ENTRY_MAX_LENGTH];
                parse_log_entry(read_log_entry, old_local_path, old_remote_path);
                pv_backup_log_append(android_id, old_local_path, old_remote_path); 
            }
        }

        fclose(fp);
        fclose(log_file);
    }
    /* Update the public shareable device list file */
    return pv_device_list_copy_public();
}

/**
 * @brief Delete a device from the device list by its android_id.
 *
 * Searches the device list file for the entry with the specified android_id and removes it from the list.
 * The function creates a temporary file to store the updated list, replaces the original file upon success, and ensures data integrity.
 *
 * @param android_id The android_id of the device to delete from the device list.
 * @return ESP_OK on success, ESP_FAIL if the device is not found or on file operation errors.
 */
esp_err_t pv_device_list_delete_device(pv_android_device_id_t android_id) {
    FILE *fp = fopen(DEVICE_LIST_PATH_INTERNAL, "r");
    if (fp == NULL) {
        PV_LOGE(TAG, "Failed to open device list file at %s", DEVICE_LIST_PATH_INTERNAL);
        return ESP_FAIL;
    }

    /* Create temporary file */
    FILE *temp_fp = fopen(TEMP_DEVICE_DATA_FILE_PATH, "w");
    if (temp_fp == NULL) {
        PV_LOGE(TAG, "Failed to create temp file");
        fclose(fp);
        return ESP_FAIL;
    }

    char line[256];
    int line_number = 0;

    /* Copy all lines except the target device */
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line_number == 0) {
            /* Copy header as-is */
            fputs(line, temp_fp);
            line_number++;
            continue;
        }

        pv_android_device_id_t current_id;
        char current_name[PV_DEVICE_NAME_MAX_LENGTH];
        char current_bda[BD_ADDR_STR_LENGTH];
        
        /* Parse the line (no space after comma) */
        if (sscanf(line, "\"%127[^\"]\",%" PRIu64 ",\"%127[^\"]\"", current_bda, &current_id, current_name) == 3) {
            if (current_id == android_id) {
                /* Don't copy over the line that needs to be deleted */
                PV_LOGI(TAG, "Deleted device with android_id %" PRIu64 " bda %s and name %s from device list", android_id, current_bda, current_name);
            } else {
                /* Copy unchanged */
                fputs(line, temp_fp);
            }
        } else {
            /* Copy malformed lines as-is */
            fputs(line, temp_fp);
        }
        
        line_number++;
    }

    fclose(fp);
    fclose(temp_fp);

    /* Replace original with temp file */
    remove(DEVICE_LIST_PATH_INTERNAL);
    rename(TEMP_DEVICE_DATA_FILE_PATH, DEVICE_LIST_PATH_INTERNAL);

    /* Update the public shareable device list file */
    return pv_device_list_copy_public();
}


esp_err_t pv_parse_device_list_entry(char* line, char *bda, pv_android_device_id_t *android_id, char* device_name) {
    if (sscanf(line, "\"%127[^\"]\",%" PRIu64 ",\"%127[^\"]\"", bda, android_id, device_name) == 3) {
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}


/**
 * @brief Checks if a device with the given id already exists.
 * @param android_id The android_id of the device to check for existence in the device list.
 * @return true if exists, false otherwise
 */
bool pv_device_list_id_exists(esp_bd_addr_t bd_addr) {

    FILE *fp = fopen(DEVICE_LIST_PATH_INTERNAL, "r");
    if (fp == NULL) {
        return false;
    }

    char line[256];
    int line_number = 0;
    char bda_str_ref[BD_ADDR_STR_LENGTH];
    bda2str(bd_addr, bda_str_ref, sizeof(bda_str_ref));


    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line_number++ == 0) {
            continue;  // Skip header
        }

        char current_name[PV_DEVICE_NAME_MAX_LENGTH];
        char current_bda[BD_ADDR_STR_LENGTH];
        pv_android_device_id_t current_android_id;

        /* Parse the line (no space after comma) */
        if (sscanf(line, "\"%127[^\"]\",%" PRIu64 ",\"%127[^\"]\"", current_bda, &current_android_id, current_name) == 3) {
            if (strcmp(current_bda, bda_str_ref) == 0) {
                fclose(fp);
                return true;
            } 
        }
    }

    fclose(fp);
    return false;
}


/**
 * @brief Retrieves the name of a device by its ID.
 * @param android_id The android_id of the device whose name should be retrieved.
 * @param out_name Buffer to store the retrieved device name.
 * @param name_buf_size Size of the out_name buffer.
 * @return true if the device is found and name is retrieved, false otherwise.
 * @note the buf sive should also include space for the null terminator
 */
bool pv_device_list_get_name_by_id(pv_android_device_id_t android_id, char *out_name, size_t name_buf_size) {

    if (out_name == NULL || name_buf_size == 0) {
        return false;
    }

    FILE *fp = fopen(DEVICE_LIST_PATH_INTERNAL, "r");
    if (fp == NULL) {
        return false;
    }

    char line[256];
    int line_number = 0;
    

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line_number++ == 0) {
            continue;  // Skip header
        }

        pv_android_device_id_t current_android_id;
        char current_name[PV_DEVICE_NAME_MAX_LENGTH];
        char current_bda[BD_ADDR_STR_LENGTH];
        
        /* Parse the line (no space after comma) */
        if (sscanf(line, "\"%127[^\"]\",%" PRIu64 ",\"%127[^\"]\"", current_bda, &current_android_id, current_name) == 3) {
            if (current_android_id == android_id) {
                /* Found the device - copy the name */
                strncpy(out_name, current_name, name_buf_size - 1);
                out_name[name_buf_size - 1] = '\0'; // Ensure null termination
                fclose(fp);
                return true;
            } 
        }
    }

    fclose(fp);
    return false;
}