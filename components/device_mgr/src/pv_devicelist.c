#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "esp_bt_defs.h"

#include "pv_bt_utils.h"
#include "pv_devicelist.h"
#include "pv_logging.h"
#include "pv_fs.h"

#define TAG "PV_DEVICELIST"

// #define DEVICE_DATA_FILE_PATH       SD_CARD_BASE_PATH "/deviceListData.txt"  /* File only stores the device count */
#define TEMP_DEVICE_DATA_FILE_PATH  SD_CARD_BASE_PATH "/deviceListData_temp.csv"


/**
 * @brief Creates or resets the device list CSV file.
 * @param None
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t pv_device_list_create(void){

    /* Device List Format */
    /*
    android_id,device_name
    1234567812345678,"John's Phones"
    */

    /* Delete old file if exists */
    remove(DEVICE_LIST_PATH);

    /* Create a new file */
    FILE *fp = fopen(DEVICE_LIST_PATH, "w");
    if (fp == NULL) {
        PV_LOGE("PV_DEVICELIST", "Failed to create device list file at %s", DEVICE_LIST_PATH);
        return ESP_FAIL;
    }
    
    /* Write the header line (no space after comma) */
    fprintf(fp, "android_id,device_name\n");

    /* Close the file */
    if (fclose(fp) != 0) {
        PV_LOGE("PV_DEVICELIST", "Failed to close device list file at %s", DEVICE_LIST_PATH);
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

    FILE *fp = fopen(DEVICE_LIST_PATH, "r");
    if (fp == NULL) {
        PV_LOGE("PV_DEVICELIST", "Failed to open device list file at %s", DEVICE_LIST_PATH);
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
 * @brief Adds the name of a device in the device list. If the device already exists, its name is updated.
 *
 * Searches the device list file for the entry with the specified android_id and updates its name to the provided new name.
 * The function creates a temporary file to store the updated list, replaces the original file upon success, and ensures data integrity.
 *
 * @param android_id The android device ID of the device whose name should be updated.
 * @param new_name The new name to assign to the device.
 * @return ESP_OK on success, ESP_FAIL if the device is not found or on file operation errors.
 */
esp_err_t pv_device_list_add_device(pv_android_device_id_t android_id, const char *new_name){
    FILE *fp = fopen(DEVICE_LIST_PATH, "r");
    if (fp == NULL) {
        PV_LOGE("PV_DEVICELIST", "Failed to open device list file at %s", DEVICE_LIST_PATH);
        return ESP_FAIL;
    }

    
    /* Create temporary file */
    FILE *temp_fp = fopen(TEMP_DEVICE_DATA_FILE_PATH, "w");
    if (temp_fp == NULL) {
        PV_LOGE("PV_DEVICELIST", "Failed to create temp file");
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

        
        
        /* Parse the line (no space after comma) */
        if (sscanf(line, "%" PRIu64 ",\"%127[^\"]\"", &current_id, current_name) == 2) {
            if (current_id == android_id) {
                /* Write updated line (no space after comma) */
                fprintf(temp_fp, "%" PRIu64 ",\"%s\"\n", android_id, new_name);
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
    if (!found) {
        fprintf(temp_fp, "%" PRIu64 ",\"%s\"\n", android_id, new_name);
    }

    fclose(fp);
    fclose(temp_fp);


    /* Replace original with temp file */
    remove(DEVICE_LIST_PATH);
    rename(TEMP_DEVICE_DATA_FILE_PATH, DEVICE_LIST_PATH);

    return ESP_OK;
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
    FILE *fp = fopen(DEVICE_LIST_PATH, "r");
    if (fp == NULL) {
        PV_LOGE("PV_DEVICELIST", "Failed to open device list file at %s", DEVICE_LIST_PATH);
        return ESP_FAIL;
    }

    /* Create temporary file */
    FILE *temp_fp = fopen(TEMP_DEVICE_DATA_FILE_PATH, "w");
    if (temp_fp == NULL) {
        PV_LOGE("PV_DEVICELIST", "Failed to create temp file");
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
        
        /* Parse the line (no space after comma) */
        if (sscanf(line, "%" PRIu64 ",\"%127[^\"]\"", &current_id, current_name) == 2) {
            if (current_id == android_id) {
                /* Don't copy over the line that needs to be deleted */
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
    remove(DEVICE_LIST_PATH);
    rename(TEMP_DEVICE_DATA_FILE_PATH, DEVICE_LIST_PATH);

    return ESP_OK;
}

/**
 * @brief Checks if a device with the given id already exists.
 * @param android_id The android_id of the device to check for existence in the device list.
 * @return true if exists, false otherwise
 */
bool pv_device_list_id_exists(pv_android_device_id_t android_id) {

    FILE *fp = fopen(DEVICE_LIST_PATH, "r");
    if (fp == NULL) {
        return false;
    }

    char line[256];
    int line_number = 0;


    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line_number++ == 0) {
            continue;  // Skip header
        }

        char current_name[PV_DEVICE_NAME_MAX_LENGTH];
        pv_android_device_id_t current_android_id;

        /* Parse the line (no space after comma) */
        if (sscanf(line, "%" PRIu64 ",\"%127[^\"]\"", &current_android_id, current_name) == 2) {
            if (current_android_id == android_id) {
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

    FILE *fp = fopen(DEVICE_LIST_PATH, "r");
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
        
        /* Parse the line (no space after comma) */
        if (sscanf(line, "%" PRIu64 ",\"%127[^\"]\"", &current_android_id, current_name) == 2) {
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