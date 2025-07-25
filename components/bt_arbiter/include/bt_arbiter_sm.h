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


// BLE includes
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "pv_logging.h"

#define TAG "PV_ARBITER"

#define PV_MDATA_BUFFER_SIZE 128 // Size of metadata character buffer (number digits in file size)

/* BT COMMANDS */
#define RESET_CMD               "RESET\n"                         // Reset command
#define RESET_CMD_LEN           (sizeof(RESET_CMD) - 1)  

#define RX_START_CMD            "RXSTART\n"                       // Start receving file from client
#define RX_START_CMD_LEN        (sizeof(RX_START_CMD) - 1)

#define RX_STARTM_CMD           "RXSTARTM\n"                      // Start receiving metadata from client
#define RX_STARTM_CMD_LEN       (sizeof(RX_STARTM_CMD) - 1)

#define RX_GETFLIST_CMD         "RXGETFLIST\n"                    // Client requests file list
#define RX_GETFLIST_CMD_LEN     (sizeof(RX_GETFLIST_CMD) - 1)

#define RX_GETFILE_CMD          "RXGETFILE\n"                     // Client requests file
#define RX_GETFILE_CMD_LEN      (sizeof(RX_GETFILE_CMD) - 1)

#define RX_OK_MSG               "RXOK\n"                          // Success message TO client after RECEIVING file
#define RX_OK_MSG_LEN           (sizeof(RX_OK_MSG) - 1)

#define TX_OK_MSG               "TXOK\n"                          // Success message FROM client after SENDING file
#define TX_OK_MSG_LEN           (sizeof(TX_OK_MSG) - 1)

#define TX_ERR_MSG              "TXERRR\n"                        // Error message FROM client after SENDING file
#define TX_ERR_MSG_LEN          (sizeof(TX_ERR_MSG) - 1)

#define DEL_CMD                 "DEL\n"                           // Delete file command from client
#define DEL_CMD_LEN             (sizeof(DEL_CMD) - 1)

#define DELOK_MSG               "DELOK\n"                         // Delete file success message to client
#define DELOK_MSG_LEN           (sizeof(DELOK_MSG) - 1)

#define DELERR_MSG              "DELERR\n"                        // Delete file error message to client
#define DELERR_MSG_LEN          (sizeof(DELERR_MSG) - 1)

#define RX_ENDM_CMD             "ENDM\n"                          // End metadata transaction
#define RX_ENDM_CMD_LEN         (sizeof(RX_ENDM_CMD) - 1)

#define ACK                     "ACK"                             // ACK  - not used?
#define ACK_LEN                 (sizeof(ACK) - 1)

#define END_CMD                 "END\n"                           // End transaction 
#define END_CMD_LEN             (sizeof(END_CMD) - 1)



typedef enum state {
    WAIT, 
    RX_ACTIVEM,
    RX_ACTIVE, 
    RX_ERROR_STATE, 

    TX_SNDFLIST,            // Send file list to client
    TX_ACTIVE,              // Active state for sending file to client
    TX_RECVACK,             // Check receipt of TX end command
    TX_ERROR_STATE,         // Transfer error state
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
} BT_ARBITER_STATE_ACTION;

void bt_arbiter_sm_feedin(uint8_t* data, uint16_t len);