#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>
#include <string.h>
#include <stdio.h>
#include "transfer_control.h"
#include "unity.h"
#include "transfer_control_tests.h"


// 1. Successful transfer to bluetooth by transmitter
// 2. Failure on bluetooth, e.g., disconnected
// 3. Receiver will read from ring buffer that failure occured
// 4. Receiver notifies backup manager of failure
// 5. Backup manager now knows of failure
// 6. Backup manager tries to re-transmit failed file later by talking to tx_cmd_queue

char dummy_file_buffer[DUMMY_FILE_BUF_SIZE];
RingbufHandle_t rx_ringbuf; // will be written to by the Bluetooth interface
RingbufHandle_t tx_ringbuf; // will be consumed by the Bluetooth interface
QueueHandle_t tx_cmd_queue; // transmission thread consumes from here, written by backup manager
QueueHandle_t status_queue; // for the backup manager
volatile int success_flag = 0; // used to indicate success or failure of happypath test


void dummy_bt_task()
{
    int i = 0;
    while (i<2)
    {
        size_t item_size;
        char *data = (char *)xRingbufferReceive(tx_ringbuf, &item_size, portMAX_DELAY);
        if (data)
        {
            printf("Dummy Bluetooth consumer received: %.*s\n", (int)item_size, data);
            vRingbufferReturnItem(tx_ringbuf, data);
            i++;
        }
    }
    const char *sample_data = "StatusIsGood";
    printf("Dummy Bluetooth writer task sending data: StatusIsGood\n");
    xRingbufferSend(rx_ringbuf, sample_data, strlen(sample_data), portMAX_DELAY);
    vTaskDelete(NULL);
}

void dummy_backup_task()
{
    transfer_cmd_t tx_cmd = {
        .transfer_type = TRANSFER_TYPE_TX,
        .status = 0
    };
    strncpy(tx_cmd.file_path, "/dummy/path/file_tx.txt", sizeof(tx_cmd.file_path));

    printf("BackupManager Sending TX command: %s\n", tx_cmd.file_path);
    xQueueSend(tx_cmd_queue, &tx_cmd, portMAX_DELAY);

    int i = 0;
    while (i < 2)
    {
        transfer_cmd_t status_msg;
        if (xQueueReceive(status_queue, &status_msg, portMAX_DELAY) == pdTRUE)
        {
            const char *type_str = status_msg.transfer_type == TRANSFER_TYPE_TX ? "TX" : "RX";
            printf("[BackupManager] status: %d\n", status_msg.status);
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
 * Function:    receiver_task
 * Purpose:     Handle the receiving of files to be backed up by reading the ring buffer
 * Parameters:  None
 * Sends to queue: PV_ERR_RECV_FAIL or 0 on success
 ***************************************************************************/
void receiver_task()
{
    while (1)
    {
        size_t item_size;
        // blocks forever
        char *data = (char *)xRingbufferReceive(rx_ringbuf, &item_size, portMAX_DELAY);

        transfer_cmd_t status_msg = {
            .transfer_type = TRANSFER_TYPE_RX,
            .status = 0
        };
        strncpy(status_msg.file_path, "/dummy/path/file_tx.txt", sizeof("/dummy/path/file_tx.txt"));
        // data == pattern, then indicate what failed
        if (data == NULL) { //TODO see if there is a failure on bluetooth receive, like a dedicated byte pattern to indicate failure
            status_msg.status = PV_ERR_RECV_FAIL;
        } else {
            // Mimic file system write by writing to dummy buffer
            strncat(dummy_file_buffer, data, item_size);
            printf("Receiver wrote %.*s to dummy file buffer\n", (int)item_size, data);

            // Need to return space in the ring buffer
            vRingbufferReturnItem(rx_ringbuf, data);
        }

        //TODO: Remove dummy
        //strncpy(status_msg.file_path, actual_filename_from_bluetooth_or_temp, sizeof(status_msg.file_path));
        //strncpy(status_msg.file_path, "/dummy/path/file_rx.txt", sizeof(status_msg.file_path));
        xQueueSend(status_queue, &status_msg, portMAX_DELAY);
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

                printf("Sent chunk: %.*s\n", (int)send_len, mock_file_content + offset);

                offset += send_len;
            }
            
            if (sent != pdTRUE) {
                status_msg.status = PV_ERR_SEND_FAIL;
            }
            strncpy(status_msg.file_path, cmd.file_path, sizeof(status_msg.file_path));
            // queue, data, wait forever
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
    // create ring buffers
    // All data is stored as a sequence of byte and do not maintain separate items
    rx_ringbuf = xRingbufferCreate(RX_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF); 
    tx_ringbuf = xRingbufferCreate(TX_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);

    /*
     QueueHandle_t xQueueCreate( 
                            UBaseType_t uxQueueLength,
                             UBaseType_t uxItemSize );

    */
    // TODO: Change size
    tx_cmd_queue = xQueueCreate(10, sizeof(transfer_cmd_t));
    status_queue = xQueueCreate(10, sizeof(transfer_cmd_t));

    /*
         BaseType_t xTaskCreate( 
                         TaskFunction_t pvTaskCode, // function
                         const char * const pcName, // name of task for desc
                         const configSTACK_DEPTH_TYPE uxStackDepth, // size (in words) of task's stack
                         void *pvParameters,
                         UBaseType_t uxPriority,
                         TaskHandle_t *pxCreatedTask
                       );
    */
    xTaskCreate(receiver_task, "receiver_task", 4096, NULL, 5, NULL);
    xTaskCreate(transmitter_task, "transmitter_task", 4096, NULL, 5, NULL);
}

void start_transfer_control_tests() {
    printf("start_transfer_control_tests\n");
    UNITY_BEGIN();
    RUN_TEST(happy_path);
    UNITY_END();  
}

