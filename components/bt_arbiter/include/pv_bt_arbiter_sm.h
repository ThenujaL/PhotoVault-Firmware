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
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>


// BLE includes
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "pv_logging.h"

#define PV_MDATA_BUFFER_SIZE 128 // Size of metadata character buffer (number digits in file size)
#define BT_RINGBUF_SIZE 8192

typedef enum state {
    WAIT, 
    RX_ACTIVEM,
    RX_ACTIVE,

    TX_SNDFLIST,            // Send file list to client
    TX_ACTIVE,              // Active state for sending file to client
    TX_RECVACK,             // Check receipt of TX end command
} BT_ARBITER_STATE;

// Internal action state machine
/*
    These are for when we use the common json metadata format and ACTIVEM state for multiple 
    purposes; these action states determine what to do after receiving metadata and is 
    set during the WAIT state based on the command received.
*/
// "nested state mchine go brrrrrr" (Abraham Lincoln, 1832)
typedef enum state_action {
    BT_ARBITER_STATE_ACTION_NONE, // No action
    BT_ARBITER_STATE_ACTION_RX_FILE, // Receive file from client
    BT_ARBITER_STATE_ACTION_TX_FILE, // Transmit file to client
    BT_ARBITER_STATE_ACTION_DEL_FILE, // Delete file on client request
    BT_ARBITER_STATE_ACTION_RENAME_FILE // Rename file on client request
} BT_ARBITER_STATE_ACTION;

void init_bt_arbiter_sm();

extern RingbufHandle_t bt_ringbuf;