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


#include <sys/types.h>
#include <sys/errno.h>

#define TAG "PV_TRANSFER_CTRL"
char *path_buffer;
char *rx_path_buffer;
struct stat sb;
// 1. Successful transfer to bluetooth by transmitter
// 2. Failure on bluetooth, e.g., disconnected
// 3. Receiver will read from ring buffer that failure occured
// 4. Receiver notifies backup manager of failure
// 5. Backup manager now knows of failure
// 6. Backup manager tries to re-transmit failed file later by talking to tx_cmd_queue
RingbufHandle_t rx_ringbuf; // will be written to by the Bluetooth interface
RingbufHandle_t tx_ringbuf; // will be consumed by the Bluetooth interface
QueueHandle_t tx_cmd_queue; // transmission thread consumes from here, written by backup manager
QueueHandle_t status_queue; // for the backup manager
volatile int success_flag = 0; // used to indicate success or failure of happypath test
#define MAX_LEN 1024

uint32_t int_bt_handle;


/***************************************************************************
 * Function:    process_file_path
 * Purpose:     Take File Path from Metadata Json Then:
 *              1. Store Path of img to write during Reciever Task
 *              2. Create Directories if they do not exist yet
 * Parameters:  None
 ***************************************************************************/

void process_file_path(char * metadata, uint16_t len)
{
    const char* prefix = SD_CARD_MOUNT_POINT;
    size_t prefix_len = strlen(SD_CARD_MOUNT_POINT);
    memcpy(path_buffer, prefix, prefix_len);
    memcpy(path_buffer + prefix_len, metadata, len);
    memcpy(path_buffer + prefix_len + len, "\0", 1);


    int end_of_dir = 0;
    for(int i = prefix_len + len; i>0; i--)
    {
        if(path_buffer[i] == '/'){
            end_of_dir = i;
            i = 0;
        }
    }


    char dir_buffer[end_of_dir + 1];
    //skip first SDCARD '/'
    for(int j = prefix_len + 1; j<end_of_dir+1; j++)
    {
        if(path_buffer[j] == '/'){
            memcpy(dir_buffer, path_buffer, j);
            memcpy(dir_buffer + j, "\0", 1);
            ESP_LOGI(TAG, "Will create Dir %s", dir_buffer);
    
            if(stat(dir_buffer, &sb) != 0)
            {
                if (mkdir(dir_buffer, S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
                    ESP_LOGE(TAG, "Failed to create a new directory: %s", strerror(errno));
                    return;
                }
            }
        }
    }

    // snprintf(path_buffer, sizeof(path_buffer), "%s/%s", MOUNT_POINT, metadata)
    ESP_LOGI(TAG, "Will open file %s", path_buffer);
}

/***************************************************************************
 * Function:    process_photo_metadata
 * Purpose:     Process Json sent from User Stores the file size and sendds file path
 *              to process_file_path
 * Parameters:  None
 ***************************************************************************/
bool process_photo_metadata(const char *json_str, size_t * size_of_image)
{
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        ESP_LOGE(TAG, "❌ Invalid JSON metadata");
        return false;
    }
    
    // cJSON *action = cJSON_GetObjectItem(json, "action");
    cJSON *filepath = cJSON_GetObjectItem(json, "filepath");
    cJSON *size = cJSON_GetObjectItem(json, "filesize");
    // cJSON *index = cJSON_GetObjectItem(json, "index");
    // cJSON *total = cJSON_GetObjectItem(json, "total");
    
    if (!filepath || !size) {
        ESP_LOGE(TAG, "❌ Missing required metadata fields");
        cJSON_Delete(json);
        return false;
    }
    

    int len_path = 0;

    len_path = snprintf(rx_path_buffer, MAX_PATH_SIZE, "%s", cJSON_GetStringValue(filepath));
    
    if(len_path >= MAX_PATH_SIZE)
    {
        ESP_LOGI(TAG, "Did not get string path correctly %s",cJSON_GetStringValue(filepath));
    }

    ESP_LOGI(TAG, "📸 Receiving path: %s with len %d", 
            rx_path_buffer, len_path);

    process_file_path(rx_path_buffer, len_path);

    *size_of_image = (uint32_t)cJSON_GetNumberValue(size);
    
    ESP_LOGI(TAG, "📸 Receiving photo: %s (%.1f KB)", 
             cJSON_GetStringValue(filepath), *size_of_image / 1024.0);
    
    cJSON_Delete(json);
    
    return true;
}

/***************************************************************************
 * Function:    receiver_task
 * Purpose:     Write recieved data to a file on SD card specified by "path_buffer" 
 *              should only be entered after metadata is sent
 * Parameters:  None
 * Send to queue:     PV_ERR_SEND_FAIL or 0 on success
 ***************************************************************************/
void receiver_task()
{
    esp_err_t ret;
    ret = ESP_OK;
    size_t written;
    // const char *file_hello = MOUNT_POINT"/test_5.png";
    // ret = s_example_write_file(file_hello, buffer);

    while (1) {
        size_t item_size;
        uint8_t *data = (uint8_t *)xRingbufferReceive(rx_ringbuf, &item_size, portMAX_DELAY);

        if (item_size != 0) {
            ESP_LOGI(TAG, "Attempting to open %s", path_buffer);
            FILE *f = fopen(path_buffer, "a");
            if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open file for writing");
                 ret = ESP_FAIL;
            }
            else {
                written = fwrite((char *)data,1,item_size, f);
                if (written != item_size) {
                    ESP_LOGE(TAG, "Failed to write all data to file");
                }
            }

            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write to file\n");
            }


            fclose(f);
        }

        // Return space in ring buffer
        vRingbufferReturnItem(rx_ringbuf, data);


        
    }
}



/***************************************************************************
 * Function:    transmitter_task
 * Purpose:     Possibly will be split into two functions or use cmd queue.
 *              Current function sends Data that was placed on the ring buffer.
 *              Concurrently iterate over all the files needing to be transmitted to the 
 *              mobile device and send data in 1024-byte chunks (TO BE IMPLEMENTED)
 *              
 * Parameters:  None
 * Send to queue:     PV_ERR_SEND_FAIL or 0 on success
 ***************************************************************************/
void transmitter_task()
{
    // transfer_cmd_t cmd;
    char *buffer_tx = malloc(INITIAL_BUFFER_SIZE); 
    while (1)
    {
        // will block forever
        // get file name here
        size_t item_size;
        uint8_t *data = (uint8_t *)xRingbufferReceive(tx_ringbuf, &item_size, portMAX_DELAY);
        memcpy(buffer_tx, data, item_size);

        ESP_LOGI(TAG, "Attempting to send on handle: [%lu]", int_bt_handle);
        esp_spp_write(int_bt_handle, item_size, (uint8_t *)buffer_tx);
        memcpy(buffer_tx + item_size, "\0", 1);
        ESP_LOGI(TAG, "Sent: %s", buffer_tx);

        


        // if (xQueueReceive(tx_cmd_queue, &cmd, portMAX_DELAY) == pdPASS)
        // {
        //     printf("Transmitter received command: %s, type: %d\n", cmd.file_path, cmd.transfer_type);

        //     // Mimic reading file contents
        //     //TODO: Remove dummy content
        //     /*
        //     FILE *f = fopen(cmd.file_path, "rb");
        //     if (f == NULL) {
        //         status_msg.status = PV_ERR_SEND_FAIL;
        //     } else {
        //         uint8_t buffer[1024];
        //         size_t read_len;
        //         while ((read_len = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        //             esp_spp_write(bt_handle, read_len, buffer);
        //             // optionally add delay or flow control here
        //         }
        //         fclose(f);
        //     }
        //     */

        //     // transfer_cmd_t status_msg = {
        //     //     .transfer_type = TRANSFER_TYPE_TX,
        //     //     .status = 0
        //     // };
        //     // strncpy(status_msg.file_path, cmd.file_path, sizeof(cmd.file_path));

        //     // const char *mock_file_content = "DylanMichaelAndrewKeen";
        //     // size_t total_len = strlen(mock_file_content);
        //     // size_t chunk_size = 8;  // for example, send in 8-byte chunks
        //     // BaseType_t sent = pdTRUE;
        //     // size_t offset = 0;
        //     // while (offset < total_len) {
        //     //     size_t remaining = total_len - offset;
        //     //     size_t send_len = (remaining < chunk_size) ? remaining : chunk_size;

        //     //     sent = xRingbufferSend(tx_ringbuf, mock_file_content + offset, send_len, portMAX_DELAY);
        //     //     if (sent != pdTRUE) {
        //     //         printf("Failed to send chunk to TX ring buffer\n");
        //     //         break;
        //     //     }

        //     //     //printf("Sent chunk: %.*s\n", (int)send_len, mock_file_content + offset);

        //     //     offset += send_len;
        //     // }
            
        //     // if (sent != pdTRUE) {
        //     //     printf("Transmitter failed to send data");
        //     //     status_msg.status = PV_ERR_SEND_FAIL;
        //     // }
        //     // strncpy(status_msg.file_path, cmd.file_path, sizeof(status_msg.file_path));
        //     xQueueSend(status_queue, &status_msg, portMAX_DELAY);
        // }
        vRingbufferReturnItem(tx_ringbuf, data);
    }
}
/***************************************************************************
 * Function:    transfer_control_init
 * Purpose:     Init ring buffers, create tasks and queues
 * Parameters:  None
 * Return:     None
 ***************************************************************************/
void transfer_control_init(uint32_t bt_handle)
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
    rx_path_buffer = malloc(MAX_PATH_SIZE); 

    int_bt_handle = bt_handle;
    // start_transfer_control_tests();
}

// void start_transfer_control_tests() {
//     printf("start_transfer_control_tests\n");
//     UNITY_BEGIN();
//     // transfer_control_init(0);
//     RUN_TEST(failure_path);
//     RUN_TEST(happy_path);
//     RUN_TEST(overflow_path);
//     UNITY_END();  
// }