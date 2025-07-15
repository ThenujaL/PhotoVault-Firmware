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
#include <inttypes.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "bt_arbiter_sm.h"
#include "sd_card.h"

static const char *TAG = "example";
char *path_buffer;
// 1. Successful transfer to bluetooth by transmitter
// 2. Failure on bluetooth, e.g., disconnected
// 3. Receiver will read from ring buffer that failure occured
// 4. Receiver notifies backup manager of failure
// 5. Backup manager now knows of failure
// 6. Backup manager tries to re-transmit failed file later by talking to tx_cmd_queue
#define SPP_TAG "SPP_ACCEPTOR_DEMO"
RingbufHandle_t rx_ringbuf; // will be written to by the Bluetooth interface
RingbufHandle_t tx_ringbuf; // will be consumed by the Bluetooth interface
QueueHandle_t tx_cmd_queue; // transmission thread consumes from here, written by backup manager
QueueHandle_t status_queue; // for the backup manager
volatile int success_flag = 0; // used to indicate success or failure of happypath test
#define MAX_LEN 1024
#define  END_RX_LEN 5
const uint8_t END_RX_CMD[END_RX_LEN] = {69, 78, 68, 13, 10}; //END + charge return

bool cmd_compare_no_len(uint8_t * CMD, uint8_t * DATA)
{
    for(int i = 0; i<END_RX_LEN; i++)
    {
        if(CMD[i] != DATA[i])
        {
            return false;
        }
    }
    return true;
}

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

/***************************************************************************
 * Function:    receiver_task
 * Purpose:     Handle the receiving of files to be backed up by reading the ring buffer.
 *              Comes in fixed chunks, we need to read until we find the end of file pattern "00000000".
 * Parameters:  None
 * Sends to queue: PV_ERR_RECV_FAIL or 0 on success
 ***************************************************************************/

 void process_meta_data(char * metadata, uint16_t len)
 {
        const char* prefix = MOUNT_POINT"/";
        size_t prefix_len = strlen(prefix);
        memcpy(path_buffer, prefix, prefix_len);
        memcpy(path_buffer + prefix_len, metadata, strlen(metadata));
        

        ESP_LOGI(TAG, "Will open file %s", path_buffer);
 }


 void receiver_task()
{
    esp_err_t ret;
    char *buffer = malloc(INITIAL_BUFFER_SIZE); 
    size_t buffer_len = 0;
    size_t buffer_size = INITIAL_BUFFER_SIZE;
    ret = ESP_OK;

    // const char *file_hello = MOUNT_POINT"/test_5.png";
    // ret = s_example_write_file(file_hello, buffer);

    while (1) {
        size_t item_size;
        uint8_t *data = (uint8_t *)xRingbufferReceive(rx_ringbuf, &item_size, portMAX_DELAY);

        // if (strstr((char *)data, FAILURE_PATTERN) != NULL) {
        //     // Handle receive failure
        //     vRingbufferReturnItem(rx_ringbuf, data);
        //     transfer_cmd_t status_msg = {
        //         .transfer_type = TRANSFER_TYPE_RX,
        //         .status = PV_ERR_RECV_FAIL
        //     };
        //     printf("Receiver notified storage manager the failure status\n");
        //     xQueueSend(status_queue, &status_msg, portMAX_DELAY);
        //     buffer_len = 0;
        //     memset(buffer, 0, buffer_size);
        //     continue;
        // }
        //printf("Receiver got chunk %.*s\n", (int)item_size, data);
 
       //printf("Keen before check %.*s\n", (int)buffer_len, buffer);
        if (cmd_compare_no_len(END_RX_CMD,data)) {
            // End of file pattern found, handle completion
            // TODO: Write the buffer to a file system (still need to figure out how we are doing this)
            
            ESP_LOGI(TAG, "File written");


           
            ESP_LOGI(SPP_TAG, "Finished writing to file!\n");


            transfer_cmd_t status_msg = {
                .transfer_type = TRANSFER_TYPE_RX,
                .status = 0
            };
            // strncpy(status_msg.file_path, "/dummy/path/file_tx.txt", sizeof(status_msg.file_path));
            ESP_LOGI(SPP_TAG, "Receiver notified storage manager the success status\n");
            xQueueSend(status_queue, &status_msg, portMAX_DELAY);

            // Reset buffer for next reception
            buffer_len = 0;
        }
        else
        {
            // if (item_size == 0) {
            //     //Received empty data, skipping write. SUPER RARE
            //     buffer_len = 0;
            //     memset(buffer, 0, 0);
            //     continue;
            // }
            ESP_LOGI(TAG, "Attempting to open %s", path_buffer);
            FILE *f = fopen(path_buffer, "a");
            if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open file for writing");
                    ret = ESP_FAIL;
            }
            memcpy(buffer, data, item_size);

            fwrite(buffer,1,item_size, f);

            if (ret != ESP_OK) {
                ESP_LOGI(SPP_TAG, "Failed to write to file\n");
                return;
            }
            // Return space in ring buffer
            vRingbufferReturnItem(rx_ringbuf, data);
            fclose(f);

        }
    }
}



/***************************************************************************
 * Function:    transmitter_task
 * Purpose:     Concurrently iterate over all the files needing to be transmitted to the 
 *              mobile device and send data in 1024-byte chunks
 * Parameters:  None
 * Send to queue:     PV_ERR_SEND_FAIL or 0 on success
 ***************************************************************************/
void transmitter_task()
{
    transfer_cmd_t cmd;
    while (1)
    {
        // will block forever
        // get file name here
        if (xQueueReceive(tx_cmd_queue, &cmd, portMAX_DELAY) == pdPASS)
        {
            printf("Transmitter received command: %s, type: %d\n", cmd.file_path, cmd.transfer_type);

            // Mimic reading file contents
            //TODO: Remove dummy content
            /*
            FILE *f = fopen(cmd.file_path, "rb");
            if (f == NULL) {
                status_msg.status = PV_ERR_SEND_FAIL;
            } else {
                uint8_t buffer[1024];
                size_t read_len;
                while ((read_len = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                    esp_spp_write(bt_handle, read_len, buffer);
                    // optionally add delay or flow control here
                }
                fclose(f);
            }
            */

            transfer_cmd_t status_msg = {
                .transfer_type = TRANSFER_TYPE_TX,
                .status = 0
            };
            strncpy(status_msg.file_path, cmd.file_path, sizeof(cmd.file_path));

            const char *mock_file_content = "DylanMichaelAndrewKeen";
            size_t total_len = strlen(mock_file_content);
            size_t chunk_size = 8;  // for example, send in 8-byte chunks
            BaseType_t sent = pdTRUE;
            size_t offset = 0;
            while (offset < total_len) {
                size_t remaining = total_len - offset;
                size_t send_len = (remaining < chunk_size) ? remaining : chunk_size;

                sent = xRingbufferSend(tx_ringbuf, mock_file_content + offset, send_len, portMAX_DELAY);
                if (sent != pdTRUE) {
                    printf("Failed to send chunk to TX ring buffer\n");
                    break;
                }

                //printf("Sent chunk: %.*s\n", (int)send_len, mock_file_content + offset);

                offset += send_len;
            }
            
            if (sent != pdTRUE) {
                printf("Transmitter failed to send data");
                status_msg.status = PV_ERR_SEND_FAIL;
            }
            strncpy(status_msg.file_path, cmd.file_path, sizeof(status_msg.file_path));
            xQueueSend(status_queue, &status_msg, portMAX_DELAY);
        }
    }
}
/***************************************************************************
 * Function:    transfer_control_init
 * Purpose:     Init ring buffers, create tasks and queues
 * Parameters:  None
 * Return:     None
 ***************************************************************************/
void transfer_control_init()
{
    // All data is stored as a sequence of byte and do not maintain separate items
    rx_ringbuf = xRingbufferCreate(RX_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF); 
    tx_ringbuf = xRingbufferCreate(TX_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);

    // TODO: Change size
    tx_cmd_queue = xQueueCreate(10, sizeof(transfer_cmd_t));
    status_queue = xQueueCreate(10, sizeof(transfer_cmd_t));

    xTaskCreate(receiver_task, "receiver_task", 8192, NULL, 5, NULL);
    xTaskCreate(transmitter_task, "transmitter_task", 8192, NULL, 5, NULL);

    path_buffer = malloc(MAX_PATH_SIZE); 
}

// void start_transfer_control_tests() {
//     printf("start_transfer_control_tests\n");
//     UNITY_BEGIN();
//     transfer_control_init();
//     RUN_TEST(failure_path);
//     RUN_TEST(happy_path);
//     RUN_TEST(overflow_path);
//     UNITY_END();  
// }