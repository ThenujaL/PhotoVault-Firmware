#include "transfer_control_tests.h"
#include "transfer_control.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdio.h>


void happy_path(){
    printf("Happy Path\n");
    const char *mock_file_content = "MobileDeviceData";
    xTaskCreate(dummy_bt_task, "dummy_bt_task", 2048, (void*)mock_file_content, 4, NULL);
    xTaskCreate(dummy_backup_task, "dummy_backup_task", 2048, NULL, 4, NULL);
    while (success_flag == 0) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait for the happy path test to complete
    }
    success_flag = 0;
}

void failure_path(){
    printf("Failure Path\n");
    const char *failure_content = FAILURE_PATTERN;
    xTaskCreate(dummy_bt_task, "dummy_bt_task", 2048, (void *) failure_content, 4, NULL);
    xTaskCreate(dummy_backup_task, "dummy_backup_task", 2048, NULL, 4, NULL);
    while (success_flag == 0) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait for the failure path test to complete
    }
    success_flag = 0;
}
void overflow_path(){
    printf("Overflow Path\n");
    char buffer[INITIAL_BUFFER_SIZE+5];
    memset(buffer, 'd', INITIAL_BUFFER_SIZE+4);
    buffer[INITIAL_BUFFER_SIZE+4] = '\0';
    xTaskCreate(dummy_bt_task, "dummy_bt_task", 2048, (void*) buffer, 4, NULL);
    xTaskCreate(dummy_backup_task, "dummy_backup_task", 2048, NULL, 4, NULL);
    while (success_flag == 0) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait for the overflow path test to complete
    }
    success_flag = 0;
}
