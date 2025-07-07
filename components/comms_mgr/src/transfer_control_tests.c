#include "transfer_control_tests.h"
#include "transfer_control.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdio.h>

void start_transfer_control_tests(void) {
    transfer_cmd_t test_cmd = {
        .transfer_type = TRANSFER_TYPE_TX,
        .status = 0
    };
    strncpy(test_cmd.file_path, "/dummy/path/file_tx.txt", sizeof(test_cmd.file_path));
    xQueueSend(tx_cmd_queue, &test_cmd, portMAX_DELAY);
}
