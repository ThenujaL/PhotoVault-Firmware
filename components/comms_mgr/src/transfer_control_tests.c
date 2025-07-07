#include "transfer_control_tests.h"
#include "transfer_control.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdio.h>


void happy_path(){
    printf("Happy Path\n");
    transfer_control_init();
    xTaskCreate(dummy_bt_task, "dummy_bt_task", 2048, NULL, 4, NULL);
    xTaskCreate(dummy_backup_task, "dummy_backup_task", 2048, NULL, 4, NULL);
    while (success_flag == 0) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait for the happy path test to complete
    }
}
