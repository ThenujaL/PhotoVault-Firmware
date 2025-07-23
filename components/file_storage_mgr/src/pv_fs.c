#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>


#include "esp_vfs_fat.h"
#include "diskio_sdmmc.h"
#include "diskio_impl.h"

#include "pv_logging.h"
#include "pv_fs.h"
#include "pv_sdc.h"


#define TAG "PV_FS"

/* STATIC VARIABLES */
static BYTE pdrv = FF_DRV_NOT_USED;

/***************************************************************************
 * Function:    pv_init_fs
 * Purpose:     Initializes the FAT filesystem for the SD card, registers it
 *              with the Virtual File System (VFS), and mounts it. If no
 *              filesystem is found, it optionally formats the card and
 *              attempts to mount again.
 * Parameters:  None
 * Returns:     ESP_OK on successful mount.
 *              ESP_ERR_INVALID_STATE if the SD card is not initialized.
 *              ESP_ERR_NO_MEM if no drive number is available.
 *              ESP_FAIL on other failures
 * Notes:       pv_init_sdc() must be called before this function
 ***************************************************************************/
esp_err_t pv_init_fs(void){
    FATFS *fs = NULL;
    esp_err_t err = ESP_OK;
    FRESULT f_res = FR_OK;
    sdmmc_card_t *card = NULL;

    pv_card_get(&card);
    if (card == NULL) {
        PV_LOGE(TAG, "SD card not initialized.");
        return ESP_ERR_INVALID_STATE; // SD card not initialized
    }

    /* Register SD/MMC diskio driver */
    ff_diskio_get_drive(&pdrv); // Get drive number for the card
    if (pdrv == FF_DRV_NOT_USED) {
        PV_LOGE(TAG, "No available drive number for SD/MMC card");
        return ESP_ERR_NO_MEM; // No available drive number
    }
    ff_diskio_register_sdmmc(pdrv, card);
    ff_sdmmc_set_disk_status_check(pdrv, true); // Enable disk status check
    char drv[3] = {(char)('0' + pdrv), ':', 0};
    esp_vfs_fat_conf_t conf = {
        .base_path = SD_CARD_BASE_PATH,
        .fat_drive = drv,
        .max_files = FATFS_MAX_FILES,
    };

    /* Connect FATFS to VFS to enable POSIX APIs */
    err = esp_vfs_fat_register_cfg(&conf, &fs);
    if (err == ESP_ERR_INVALID_STATE) {
        // it's okay, already registered with VFS
    } else if (err != ESP_OK) {
        PV_LOGE(TAG, "Failed to register FATFS with VFS (0x%x)", err);
        return err;
    }

    if (fs == NULL) {
        PV_LOGE(TAG, "FATFS pointer is NULL after registration");
        return ESP_FAIL;
    }

    /* Mount the filesystem */
    f_res = f_mount(fs, drv, 1);
    if (f_res != FR_OK) {
        // If mount fails, check if we need to format the SD card and try to mount again
        if ((f_res == FR_NO_FILESYSTEM || f_res == FR_INT_ERR) && FORMAT_SD_CARD_ON_MOUNT_FAIL) {
            PV_LOGW(TAG, "No filesystem found, formatting SD card and trying to mount again...");
            if (pv_fmt_sdc() != ESP_OK) {
                PV_LOGE(TAG, "Failed to format SD card");
                return ESP_FAIL;
            }

            f_res = f_mount(fs, drv, 1); // Try to mount again after formatting
            if (f_res != FR_OK) {
                PV_LOGE(TAG, "Failed to mount FATFS after formatting (0x%x)", f_res);
                return ESP_FAIL; // Mount failed after formatting
            }
        }
        else {
            PV_LOGE(TAG, "Failed to mount FATFS (0x%x)", f_res);
            return ESP_FAIL; // Mount failed
        }
    }
    
    PV_LOGI(TAG, "FATFS mounted successfully at %s", SD_CARD_BASE_PATH);
    return ESP_OK;
}


/***************************************************************************
 * Function:    pv_fmt_sdc
 * Purpose:     Formats the SD card with a FAT filesystem.
 * Parameters:  None
 * Returns:     ESP_OK on successful mount.
 *              ESP_ERR_NO_MEM if insufficient memory for FS operations
 *              ESP_FAIL on other failures
 * Notes:       pv_init_fs() must be called before this function
 ***************************************************************************/
esp_err_t pv_fmt_sdc(void) {
    LBA_t plist[] = {1000, 0, 0, 0}; // Partition table list, 100 sectors for the first partition, rest are empty
    void *workbuf = NULL;
    FRESULT f_res = FR_OK;
    MKFS_PARM opt = { // FATFS format parameters
        .fmt = FM_FAT32,
        .n_fat = 1,
        .align = 0,
        .n_root = 0, // Not applicable for FAT32
        .au_size = FATFS_ALLOCATION_UNIT_SIZE
    };

    char drv[3] = {(char)('0' + pdrv), ':', 0};

    /* Try to unmount, we don't care about the result */
    f_mount(NULL, drv, 0);

    /* Allocate memory for partition and format operations */
    workbuf = ff_memalloc(FATFS_WORKBUF_SIZE);
    if (workbuf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    /* Partition disk with partition table */
    f_res = f_fdisk(pdrv, plist, workbuf);
    if (f_res != FR_OK) {
        PV_LOGE(TAG, "Failed to partition SD card (0x%x)", f_res);
        return ESP_FAIL;
    }

    /* Format FAT file system on SD card */
    f_res = f_mkfs(drv, &opt, workbuf, FATFS_WORKBUF_SIZE);
    if (f_res != FR_OK) {
        PV_LOGE(TAG, "Failed to format SD card (0x%x)", f_res);
        return ESP_FAIL;
    }

    ff_memfree(workbuf);
    PV_LOGI(TAG, "SD card formatted successfully");
    return ESP_OK;

}

/***************************************************************************
 * Function:    pv_delete_dir
 * Purpose:     Deletes a directory and all its contents recursively.
 * Parameters:  path - The path of the directory to delete.
 * Returns:     ESP_OK on success
 *              ESP_FAIL else
 ***************************************************************************/
esp_err_t pv_delete_dir(const char *path){
    DIR *d = opendir(path);
    struct dirent *entry;
    char filepath[1024];

    if (!d) {
        PV_LOGE(TAG, "opendir");
        return -1;
    }

    while ((entry = readdir(d)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

        struct stat statbuf;
        if (stat(filepath, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                // It's a directory; recurse
                if (pv_delete_dir(filepath) != 0) {
                    closedir(d);
                    return -1;
                }
            } else {
                // It's a file; delete it
                if (remove(filepath) != 0) {
                    PV_LOGE(TAG, "remove file");
                    closedir(d);
                    return -1;
                }
            }
        }
    }

    closedir(d);

    // Finally delete the directory itself
    if (rmdir(path) != 0) {
        perror("rmdir");
        return -1;
    }

    return 0;
}

/***************************************************************************
 * Function:    pv_get_file_length
 * Purpose:     Gets the length of a file in bytes.
 * Parameters:  file_path - The path of the file to send.
 *              length - Pointer to store the file length.
 * Returns:     ESP_OK on success
 *              ESP_FAIL else
 ***************************************************************************/
esp_err_t pv_get_file_length(const char *file_path, uint32_t *length) {
    struct stat st;
    if (stat(file_path, &st) != 0) {
        PV_LOGE(TAG, "Failed to get file size for %s", file_path);
        return ESP_FAIL;
    }
    *length = (uint32_t)st.st_size;
    return ESP_OK;
}