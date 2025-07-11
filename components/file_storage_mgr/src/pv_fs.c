
#include "esp_vfs_fat.h"
#include "diskio_sdmmc.h"
#include "diskio_impl.h"

#include "pv_logging.h"
#include "pv_fs.h"
#include "pv_sdc.h"


#define TAG "PV_FS"


esp_err_t pv_init_fs(void){
    FATFS *fs = NULL;
    esp_err_t err = ESP_OK;
    FRESULT f_res = FR_OK;
    BYTE pdrv = FF_DRV_NOT_USED;
    sdmmc_card_t *s_card = card_get(); // Get the SD card context
    if (s_card == NULL) {
        PV_LOGE(TAG, "SD card not initialized.");
        return ESP_ERR_INVALID_STATE; // SD card not initialized
    }
    sdmmc_card_print_info(stdout, s_card);

    /* Register SD/MMC diskio driver */
    ff_diskio_get_drive(&pdrv); // Get drive number for the card
    if (pdrv == FF_DRV_NOT_USED) {
        PV_LOGE(TAG, "No available drive number for SD/MMC card");
        return ESP_ERR_NO_MEM; // No available drive number
    }
    ff_diskio_register_sdmmc(pdrv, s_card);
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
    #ifdef FORMAT_SD_CARD_ON_MOUNT
        void *workbuf = ff_memalloc(FATFS_WORKBUF_SIZE);
        if (workbuf == NULL) {
            return ESP_ERR_NO_MEM;
        }
        LBA_t plist[] = {1000, 0, 0, 0}; // Partition table list, 100 sectors for the first partition, rest are empty

        MKFS_PARM opt = { // FATFS format parameters
            .fmt = FM_FAT32,
            .n_fat = 1,
            .align = 0,
            .n_root = 0, // Not applicable for FAT32
            .au_size = FATFS_ALLOCATION_UNIT_SIZE
        };
        fprintf(stdout, "partitioning SD card with allocation unit size: %ld\n", opt.au_size);
        f_res = f_fdisk(pdrv, plist, workbuf);
        if (f_res != FR_OK) {
            PV_LOGE(TAG, "Failed to partition SD card (0x%x)", f_res);
            return ESP_FAIL;
        }
        fprintf(stdout, "partitioned SD card successfully\n");
        fprintf(stdout, "formatting SD card with allocation unit size: %ld\n", opt.au_size);

        f_res = f_mkfs(drv, &opt, workbuf, FATFS_WORKBUF_SIZE);
        if (f_res != FR_OK) {
            PV_LOGE(TAG, "Failed to format SD card (0x%x)", f_res);
            return ESP_FAIL;
        }
    #endif 
    f_res = f_mount(fs, drv, 1);
    if (f_res != FR_OK) {
        PV_LOGE(TAG, "Failed to mount FATFS (0x%x)", f_res);
        return ESP_FAIL;
    }

    PV_LOGI(TAG, "FATFS mounted successfully at %s", SD_CARD_BASE_PATH);
    return ESP_OK;
}


