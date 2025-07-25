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
#define RX_START_CMD            "RXSTART\n"                       // Start receving file from client
#define RX_START_CMD_LEN        (sizeof(RX_START_CMD) - 1)

#define RX_STARTM_CMD           "RXSTARTM\n"                      // Start receiving metadata from client
#define RX_STARTM_CMD_LEN       (sizeof(RX_STARTM_CMD) - 1)

#define RX_GETFLIST_CMD         "RXGETFLIST\n"                    // Client requests file list
#define RX_GETFLIST_CMD_LEN     (sizeof(RX_GETFLIST_CMD) - 1)

#define RX_GETFILE_CMD          "RXGETFILE\n"                     // Client requests file
#define RX_GETFILE_CMD_LEN      (sizeof(RX_GETFILE_CMD) - 1)

#define RX_OK_MSG               "RXOK\n"                          // Acknowledge message to client
#define RX_OK_MSG_LEN           (sizeof(RX_OK_MSG) - 1)

#define TX_OK_MSG               "TXOK\n"                          // Success message to client
#define TX_OK_MSG_LEN           (sizeof(TX_OK_MSG) - 1)

#define TX_ERR_MSG              "TXERRR\n"                        // Error message to client
#define TX_ERR_MSG_LEN          (sizeof(TX_ERR_MSG) - 1)

#define RX_ENDM_CMD             "ENDM\n"                          // End metadata transaction
#define RX_ENDM_CMD_LEN         (sizeof(RX_ENDM_CMD) - 1)

#define ACK                     "ACK"                             // ACK  - not used?
#define ACK_LEN                 (sizeof(ACK) - 1)

#define END_CMD                 "END\n"                              // End transaction 
#define END_CMD_LEN             (sizeof(END_CMD) - 1)



typedef enum state {
    WAIT, 
    RX_ACTIVEM,
    RX_ACTIVE, 
    RX_ERROR_STATE, 

    TX_SNDFLIST,            // Send file list to client
    TX_RECVACK,             // Check receipt of TX end command
    TX_ERROR_STATE,         // Transfer error state
}BT_ARBITER_STATE;

void bt_arbiter_sm_feedin(uint8_t* data, uint16_t len);