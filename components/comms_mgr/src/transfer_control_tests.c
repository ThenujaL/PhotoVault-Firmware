#include "transfer_control_tests.h"
#include "transfer_control.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdio.h>

#include "transfer_control.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>
#include <string.h>
#include <stdio.h>
#include "transfer_control.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

void dummy_bt_task(void* param)
{
    size_t item_size;
    char *data = (char *)xRingbufferReceive(tx_ringbuf, &item_size, portMAX_DELAY);
    if (data)
    {
        printf("Dummy Bluetooth consumer received: %.*s\n", (int)item_size, data);
        vRingbufferReturnItem(tx_ringbuf, data);
    }
    const char *mock_file_content = (const char *)param;
    size_t total_len = strlen(mock_file_content) + 1; // +1 for null terminator
    char buffer_to_send[total_len];
    snprintf(buffer_to_send, sizeof(buffer_to_send), "%s", mock_file_content);

    size_t chunk_size = 8;  // for example, send in 8-byte chunks
    BaseType_t sent = pdTRUE;
    size_t offset = 0;
    while (offset < total_len) {
        size_t remaining = total_len - offset;
        size_t send_len = (remaining < chunk_size) ? remaining : chunk_size;

        sent = xRingbufferSend(rx_ringbuf, buffer_to_send + offset, send_len, portMAX_DELAY);
        if (sent != pdTRUE) {
            printf("Failed to send chunk to RX ring buffer\n");
            break;
        }

        //printf("Dummy Bluetooth sent chunk: %.*s\n", (int)send_len, buffer_to_send + offset);
        offset += send_len;
    }
    vTaskDelete(NULL);
}

void dummy_backup_task()
{
    transfer_cmd_t tx_cmd = {
        .transfer_type = TRANSFER_TYPE_TX,
        .status = 0
    };
    strncpy(tx_cmd.file_path, "/dummy/path/file_tx.txt", sizeof(tx_cmd.file_path));

    xQueueSend(tx_cmd_queue, &tx_cmd, portMAX_DELAY);

    int i = 0;
    while (i < 2)
    {
        transfer_cmd_t status_msg;
        if (xQueueReceive(status_queue, &status_msg, portMAX_DELAY) == pdTRUE)
        {
            const char *type_str = status_msg.transfer_type == TRANSFER_TYPE_TX ? "TX" : "RX";
            if (status_msg.status == 0) {
                printf("[BackupManager] SUCCESS: %s completed for %s\n", type_str, status_msg.file_path);
            } else {
                printf("[BackupManager] FAIL: %s failed for %s [ERR = %d]\n",
                       type_str, status_msg.file_path, status_msg.status);
            }
            i++;
        }
    }
    success_flag = 1;
    vTaskDelete(NULL);
}


/***************************************************************************
 * Function:    append_data
 * Purpose:     Resize the buffer if needed and append new data to it.
 *              The buffer is resized to double its size if it exceeds the initial size.
 * Parameters:  Receiving buffer, buffer length, max buffer size, data to append, and size of the data item.
 * Return:     None
 * Note:       The buffer is null-terminated after appending the new data.
 ***************************************************************************/
void append_data(char **buffer, size_t *buffer_len, size_t *buffer_size, const char *data, size_t item_size) {
    //printf("Buffer length before append: %zu, size: %zu\n", *buffer_len, *buffer_size);
    //printf("Item: %.*s\n", (int)item_size, data);
    //printf("Item size: %zu\n", item_size);
    // printf("Item bytes: ");
    // for (size_t i = 0; i < item_size; ++i) {
    //     printf("%02X ", (unsigned char)data[i]);
    // }
    // printf("\n");
    if (*buffer_len + item_size + 1 > *buffer_size) {

        size_t new_size = (*buffer_size * 2 > *buffer_len + item_size + 1) ? *buffer_size * 2 : *buffer_len + item_size + 1;
        char *new_buffer = realloc(*buffer, new_size);
        if (!new_buffer) {
            printf("Failed to allocate memory for buffer!\n");
            exit(1);
        }

        *buffer = new_buffer;
        *buffer_size = new_size;

        printf("Buffer resized to %zu bytes\n", new_size);
    }
    // Append new data
    memcpy(*buffer + *buffer_len, data, item_size);
    *buffer_len += item_size;
    // (*buffer)[*buffer_len] = '\0'; // Null-terminate
}

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
    xTaskCreate(dummy_bt_task, "dummy_bt_task", 8192, (void *) failure_content, 4, NULL);
    xTaskCreate(dummy_backup_task, "dummy_backup_task", 8192, NULL, 4, NULL);
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
