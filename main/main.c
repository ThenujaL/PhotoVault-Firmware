
#include <stdio.h>
#include "board_config.h"
#include "pv_bdev.h"
#include "driver/sdspi_host.h"
#include "pv_logging.h"


#define TAG "PV_MAIN"


void app_main(void)
{
    esp_err_t ret;

    /* Configure peripherals */
    ret = pv_init_sdc();
    if (ret != ESP_OK) {
        PV_LOGE(TAG, "Failed to initialize SD Card.");
        return;
    }


    printf("Hello world from PV application!\n");
    
    /* Run peripheral tests */
    pv_test_sdc();
}
