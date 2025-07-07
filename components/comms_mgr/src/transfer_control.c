#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>
#include <string.h>
#include <stdio.h>
#include "transfer_control.h"
#include <projdefs.h>

static char dummy_file_buffer[DUMMY_FILE_BUF_SIZE];
static RingbufHandle_t rx_ringbuf; // will be written to by the Bluetooth interface
static RingbufHandle_t tx_ringbuf; // will be consumed by the Bluetooth interface
static QueueHandle_t tx_cmd_queue; // transmission thread consumes from here, written by backup manager
static QueueHandle_t status_queue; // for the backup manager

void dummy_bt_writer_task(void)
{
    while (1)
    {
        const char *sample_data = "IncomingBluetoothData";
        vRingbufferSend(rx_ringbuf, sample_data, strlen(sample_data), portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void dummy_bt_consumer_task(void)
{
    while (1)
    {
        size_t item_size;
        char *data = (char *)xRingbufferReceive(tx_ringbuf, &item_size, portMAX_DELAY);
        if (data)
        {
            printf("Dummy BT consumer received: %.*s\n", (int)item_size, data);
            vRingbufferReturnItem(tx_ringbuf, data);
        }
    }
}
/***************************************************************************
 * Function:    receiver_task
 * Purpose:     Handle the receiving of files to be backed up by reading the ring buffer
 * Parameters:  None
 * Sends to queue: PV_ERR_RECV_FAIL or 0 on success
 ***************************************************************************/
void receiver_task(void)
{
    while (1)
    {
        size_t item_size;
        char *data = (char *)xRingbufferReceive(rx_ringbuf, &item_size, portMAX_DELAY);
        if (data)
        {
            // Mimic file system write by writing to dummy buffer
            strncat(dummy_file_buffer, data, item_size); // appends
            printf("Receiver wrote %.*s to dummy file buffer\n", (int)item_size, data);

            transfer_cmd_t status_msg = {
                .transfer_type = TRANSFER_TYPE_RX,
                .status = 0,
            };

            if (data == NULL) {
                status_msg.status = PV_ERR_RECV_FAIL;
            } else {
                // mimic file system write
                strncat(dummy_file_buffer, data, item_size);
                vRingbufferReturnItem(rx_ringbuf, data);
            }
            //TODO: Remove dummy
            //strncpy(status_msg.file_path, actual_filename_from_bluetooth_or_temp, sizeof(status_msg.file_path));

            strncpy(status_msg.file_path, "/dummy/path/file_rx.txt", sizeof(status_msg.file_path));
            xQueueSend(status_queue, &status_msg, portMAX_DELAY);

            vRingbufferReturnItem(rx_ringbuf, data);
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
void transmitter_task(void)
{
    transfer_cmd_t cmd;
    while (1)
    {
        if (xQueueReceive(tx_cmd_queue, &cmd, portMAX_DELAY) == pdPASS)
        {
            printf("Transmitter received command: %s, type: %d\n", cmd.file_path, cmd.transfer_type);

            // Mimic reading file contents
            const char *mock_file_content = "DylanMichaelAndrewKeen";
            BaseType_t sent = vRingbufferSend(tx_ringbuf, mock_file_content, strlen(mock_file_content), portMAX_DELAY);

            transfer_cmd_t status_msg = {
                .transfer_type = TRANSFER_TYPE_TX,
                .status = 0,
            };
            if (sent != pdTRUE) {
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
 * Send to queue:     PV_ERR_SEND_FAIL or 0 on success
 ***************************************************************************/
void transfer_control_init()
{
    // create ring buffers
    //All data is stored as a sequence of byte and do not maintain separate items
    rx_ringbuf = xRingbufferCreate(RX_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF); 
    tx_ringbuf = xRingbufferCreate(TX_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);

    /*
     QueueHandle_t xQueueCreate( 
                            UBaseType_t uxQueueLength,
                             UBaseType_t uxItemSize );

    */
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
    xTaskCreate(dummy_bt_writer_task, "dummy_bt_writer", 2048, NULL, 4, NULL);
    xTaskCreate(dummy_bt_consumer_task, "dummy_bt_consumer", 2048, NULL, 4, NULL);
}
