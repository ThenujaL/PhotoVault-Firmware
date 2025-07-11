
#include <stdio.h>
#include "board_config.h"
#include "pv_sdc.h"
#include "pv_fs.h"
#include "driver/sdspi_host.h"
#include "pv_logging.h"


#define TAG "PV_MAIN"


void app_main(void)
{
    esp_err_t ret;
    const char *filepath = SD_CARD_BASE_PATH "/test.txt";

    /* Configure peripherals */
    ret = pv_init_sdc();
    if (ret != ESP_OK) {
        PV_LOGE(TAG, "Failed to initialize SD Card.");
        return;
    }

    ret = pv_init_fs();
    if (ret != ESP_OK) {
        PV_LOGE(TAG, "Failed to initialize file system.");
        return;
    }


    FILE *f = fopen(filepath, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        return;
    }

    fprintf(f, "Hello, SD card!\n");
    fclose(f);

    f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", filepath);
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        PV_LOGI(TAG, "Read line: %s", line);
    }

    fclose(f);
    
    /* Run peripheral tests */
    pv_test_sdc();
}
