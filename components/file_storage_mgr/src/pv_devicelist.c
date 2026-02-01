#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "pv_devicelist.h"
#include "pv_logging.h"
#include "pv_fs.h"

#define DEVICE_DATA_FILE_PATH       SD_CARD_BASE_PATH "/deviceListData.txt"  /* File only stores the device count */
#define TEMP_DEVICE_DATA_FILE_PATH  SD_CARD_BASE_PATH "/deviceListData_temp.txt"

/**
 * @brief Creates or resets the device list CSV file.
 * @param None
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t pv_device_list_create(void){

    /* Device List Format */
    /*
    device_id,device_name
    12345678,"John's Phones"
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
    fprintf(fp, "device_id,device_name\n");

    /* Close the file */
    if (fclose(fp) != 0) {
        PV_LOGE("PV_DEVICELIST", "Failed to close device list file at %s", DEVICE_LIST_PATH);
        return ESP_FAIL;
    }


    /* Device List Data */
    /*
    device_count: <number_of_devices>
    next_id: <next_device_id>
    */

    /* Create the device count file */
    FILE *count_fp = fopen(DEVICE_DATA_FILE_PATH, "w");
    if (count_fp == NULL) {
        PV_LOGE("PV_DEVICELIST", "Failed to create device count file at %s", DEVICE_DATA_FILE_PATH);
        return ESP_FAIL;
    }

    fprintf(count_fp, "device_count: 0\n");
    fprintf(count_fp, "next_id: 0\n");

    if (fclose(count_fp) != 0) {
        PV_LOGE("PV_DEVICELIST", "Failed to close device count file at %s", DEVICE_DATA_FILE_PATH);
        return ESP_FAIL;
    }

    return ESP_OK;
}


/**
 * @brief Adds a new device to the device list (robust version).
 * @param device_name The name of the device to add
 * @param device_id_out Pointer to store the assigned device ID (optional, can be NULL)
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t pv_device_list_add_device(const char *device_name, int *device_id_out) {
    if (device_name == NULL) {
        PV_LOGE("PV_DEVICELIST", "Device name cannot be NULL");
        return ESP_FAIL;
    }

    /* Validate device name */
    size_t name_len = strlen(device_name);
    if (name_len == 0) {
        PV_LOGE("PV_DEVICELIST", "Device name cannot be empty");
        return ESP_FAIL;
    }
    if (name_len > 127) {
        PV_LOGE("PV_DEVICELIST", "Device name too long (max 127 characters)");
        return ESP_FAIL;
    }
    if (strchr(device_name, '"') != NULL) {
        PV_LOGE("PV_DEVICELIST", "Device name cannot contain quotes");
        return ESP_FAIL;
    }

    /* Step 1: Read current state */
    FILE *data_fp = fopen(DEVICE_DATA_FILE_PATH, "r");
    if (data_fp == NULL) {
        PV_LOGE("PV_DEVICELIST", "Failed to open device data file at %s", DEVICE_DATA_FILE_PATH);
        return ESP_FAIL;
    }

    int device_count = 0;
    int next_id = 0;
    char line[128];
    bool found_count = false;
    bool found_next_id = false;

    while (fgets(line, sizeof(line), data_fp) != NULL) {
        if (sscanf(line, "device_count: %d", &device_count) == 1) {
            found_count = true;
        }
        if (sscanf(line, "next_id: %d", &next_id) == 1) {
            found_next_id = true;
        }
    }
    fclose(data_fp);

    if (!found_count || !found_next_id) {
        PV_LOGE("PV_DEVICELIST", "Malformed device data file");
        return ESP_FAIL;
    }

    int new_device_id = next_id;

    /* Append device to list (no space after comma) */
    FILE *list_fp = fopen(DEVICE_LIST_PATH, "a");
    if (list_fp == NULL) {
        PV_LOGE("PV_DEVICELIST", "Failed to open device list file at %s", DEVICE_LIST_PATH);
        return ESP_FAIL;
    }

    if (fprintf(list_fp, "%d,\"%s\"\n", new_device_id, device_name) < 0) {
        PV_LOGE("PV_DEVICELIST", "Failed to write to device list file");
        fclose(list_fp);
        return ESP_FAIL;
    }
    
    if (fclose(list_fp) != 0) {
        PV_LOGE("PV_DEVICELIST", "Failed to close device list file");
        return ESP_FAIL;
    }

    /* Update data file */
    data_fp = fopen(DEVICE_DATA_FILE_PATH, "w");
    if (data_fp == NULL) {
        PV_LOGE("PV_DEVICELIST", "Failed to open device data file for writing");
        PV_LOGE("PV_DEVICELIST", "Device added but count not updated - data inconsistent!");
        return ESP_FAIL;
    }

    fprintf(data_fp, "device_count: %d\n", device_count + 1);
    fprintf(data_fp, "next_id: %d\n", next_id + 1);
    
    if (fclose(data_fp) != 0) {
        PV_LOGE("PV_DEVICELIST", "Failed to close device data file");
        return ESP_FAIL;
    }

    PV_LOGI("PV_DEVICELIST", "Successfully added device \"%s\" with ID %d (total: %d devices)", 
            device_name, new_device_id, device_count + 1);

    /* Return the assigned ID if requested */
    if (device_id_out != NULL) {
        *device_id_out = new_device_id;
    }

    return ESP_OK;
}


/**
 * @brief Update the name of a device in the device list.
 *
 * Searches the device list file for the entry with the specified device ID and updates its name to the provided new name.
 * The function creates a temporary file to store the updated list, replaces the original file upon success, and ensures data integrity.
 *
 * @param device_id The ID of the device whose name should be updated.
 * @param new_name The new name to assign to the device.
 * @return ESP_OK on success, ESP_FAIL if the device is not found or on file operation errors.
 */
esp_err_t pv_device_list_update_name(const int device_id, const char *new_name) {
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

        int current_id;
        char current_name[128];
        
        /* Parse the line (no space after comma) */
        if (sscanf(line, "%d,\"%127[^\"]\"", &current_id, current_name) == 2) {
            if (current_id == device_id) {
                /* Write updated line (no space after comma) */
                fprintf(temp_fp, "%d,\"%s\"\n", device_id, new_name);
                found = true;
            } else {
                /* Copy unchanged */
                fputs(line, temp_fp);
            }
        } else {
            /* Copy malformed lines as-is (or handle error) */
            fputs(line, temp_fp);
        }
        
        line_number++;
    }

    fclose(fp);
    fclose(temp_fp);

    if (!found) {
        PV_LOGE("PV_DEVICELIST", "Device ID %d not found in device list", device_id);
        remove(TEMP_DEVICE_DATA_FILE_PATH);  // Clean up temp file
        return ESP_FAIL;
    }

    /* Replace original with temp file */
    remove(DEVICE_LIST_PATH);
    rename(TEMP_DEVICE_DATA_FILE_PATH, DEVICE_LIST_PATH);

    return ESP_OK;
}

/**
 * @brief Delete a device from the device list by its ID.
 *
 * Searches the device list file for the entry with the specified device ID and removes it from the list.
 * The function creates a temporary file to store the updated list, replaces the original file upon success, and ensures data integrity.
 *
 * @param device_id The ID of the device to delete from the device list.
 * @return ESP_OK on success, ESP_FAIL if the device is not found or on file operation errors.
 */
esp_err_t pv_device_list_delete_device(const int device_id) {
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

    /* Copy all lines except the target device */
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line_number == 0) {
            /* Copy header as-is */
            fputs(line, temp_fp);
            line_number++;
            continue;
        }

        int current_id;
        char current_name[128];
        
        /* Parse the line (no space after comma) */
        if (sscanf(line, "%d,\"%127[^\"]\"", &current_id, current_name) == 2) {
            if (current_id == device_id) {
                /* Skip this line (delete the device) */
                found = true;
            } else {
                /* Copy unchanged */
                fputs(line, temp_fp);
            }
        } else {
            /* Copy malformed lines as-is (or handle error) */
            fputs(line, temp_fp);
        }
        
        line_number++;
    }

    fclose(fp);
    fclose(temp_fp);

    if (!found) {
        PV_LOGE("PV_DEVICELIST", "Device ID %d not found in device list", device_id);
        remove(TEMP_DEVICE_DATA_FILE_PATH);  // Clean up temp file
        return ESP_FAIL;
    }

    /* Replace original with temp file */
    remove(DEVICE_LIST_PATH);
    rename(TEMP_DEVICE_DATA_FILE_PATH, DEVICE_LIST_PATH);

    return ESP_OK;
}

/**
 * @brief Checks if a device with the given name already exists.
 * @param device_name The name to check
 * @return true if exists, false otherwise
 */
bool pv_device_list_name_exists(const char *device_name) {
    if (device_name == NULL) {
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

        int current_id;
        char current_name[128];
        
        /* Parse the line (no space after comma) */
        if (sscanf(line, "%d,\"%127[^\"]\"", &current_id, current_name) == 2) {
            if (strcmp(current_name, device_name) == 0) {
                fclose(fp);
                return true;
            }
        }
    }

    fclose(fp);
    return false;
}