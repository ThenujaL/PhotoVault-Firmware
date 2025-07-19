
#include <stdio.h>
#include "board_config.h"
#include "pv_sdc.h"
#include "pv_fs.h"
#include "driver/sdspi_host.h"
#include "pv_logging.h"
#include "transfer_control.h"
#include "bluetooth_mgr.h"


#define TAG "PV_MAIN"


void app_main(void)
{
    esp_err_t ret;

    // Start SD Card First Before Transfer Controllor and Bluetooth

    /* Configure peripherals */
    ret = pv_init_sdc();
    if (ret != ESP_OK) {
        PV_LOGE(TAG, "Failed to initialize SD Card.");
        // return;
    }

    //set up bluetooth after this cmd ready to connect
    //This will setup Transfer Controllor and Start BT Arbiter State Machine
    register_bluetooth_callbacks();
    
    ret = pv_init_fs();
    if (ret != ESP_OK) {
        PV_LOGE(TAG, "Failed to initialize file system.");
        return;
    }
    


    /* Run peripheral tests */
    // pv_test_sdc();

    // Run transfer control tests (Transfer control requiers bluetooth handle to send over bluetooth can no longer be run without first connecting to bluetooth)
    // start_transfer_control_tests();
}
