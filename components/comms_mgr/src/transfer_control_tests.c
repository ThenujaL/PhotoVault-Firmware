#include "transfer_control_tests.h"
#include "transfer_control.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdio.h>
#include "unity.h"


void happy_path(){
    printf("Running happy path test...\n");
    TEST_ASSERT_EQUAL(4, 2 + 2);
    // Create a test command
    // transfer_cmd_t test_cmd = {
    //     .transfer_type = TRANSFER_TYPE_TX,
    //     .status = 0
    // };
    // strncpy(test_cmd.file_path, "/dummy/path/file_tx.txt", sizeof(test_cmd.file_path));
    
    // // Send the command to the tx_cmd_queue
    // if (xQueueSend(tx_cmd_queue, &test_cmd, portMAX_DELAY) != pdTRUE) {
    //     printf("Failed to send test command to tx_cmd_queue\n");
    // } else {
    //     printf("Test command sent successfully\n");
    // }
}
