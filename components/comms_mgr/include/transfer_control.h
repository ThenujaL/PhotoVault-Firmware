#ifndef TRANSFER_CONTROL_H
#define TRANSFER_CONTROL_H

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "cJSON.h"
#include "pv_fs.h"
#include "pv_sdc.h"

#define RX_RINGBUF_SIZE 4096
#define TX_RINGBUF_SIZE 4096
#define INITIAL_BUFFER_SIZE 4096
#define MAX_PATH_SIZE 256

#define TRANSFER_TYPE_RX 0
#define TRANSFER_TYPE_TX 1

#define PV_ERR_SEND_FAIL 1
#define PV_ERR_RECV_FAIL 2

#define FAILURE_PATTERN "69696969"


typedef struct
{
    char file_path[128];
    uint8_t transfer_type; // TRANSFER_TYPE_TX or TRANSFER_TYPE_RX
    uint8_t status;        // PV_ERR_SEND_FAIL, PV_ERR_RECV_FAIL, or 0 on success
} transfer_cmd_t;

// declare variables whose definitions are present in c file
extern QueueHandle_t tx_cmd_queue;
extern QueueHandle_t status_queue;
extern RingbufHandle_t rx_ringbuf;
extern RingbufHandle_t tx_ringbuf;

void transfer_control_init(uint32_t bt_handle);
void receiver_task();
void transmitter_task();
void append_data(char **buffer, size_t *buffer_len, size_t *buffer_size, const char *data, size_t item_size);
// void process_meta_data(uint8_t * metadata, uint16_t len);
//testing
extern volatile int success_flag;
void dummy_bt_task(void* param);
void dummy_backup_task();
void start_transfer_control_tests();
bool process_photo_metadata(const char *json_str, size_t * size_of_image);

#endif