/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

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
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>

// BLE includes
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

#include <string.h>
#include <stdio.h>
#include "transfer_control.h"
#include "cJSON.h"
#include "bt_arbiter_sm.h"
#include "bluetooth_mgr.h"
#include "pv_auth.h"
#include "pv_devicelist.h"
#include "pv_bt_utils.h"

#define TAG "PV_ARBITER"


struct spp_data_ind_evt_param cur_data;




BT_ARBITER_STATE cur_state = WAIT;
BT_ARBITER_STATE_ACTION cur_state_action = BT_ARBITER_STATE_ACTION_NONE;
RingbufHandle_t bt_ringbuf;

struct bt_arbiter_sm_cmd_line {
    uint16_t            len;            /*!< The length of data */
    uint8_t             *data;          /*!< The data received */
} cmd_line;     

bool cmd_compare(char * CMD, uint8_t * DATA, uint16_t len)
{
    for(int i = 0; i<len; i++)
    {
        if(CMD[i] != DATA[i])
        {
            return false;
        }
    }
    return true;
}

static void set_state(BT_ARBITER_STATE new_state)
{
    cur_state = new_state;
}

static void set_state_action(BT_ARBITER_STATE_ACTION new_state_action)
{
    cur_state_action = new_state_action;
}


#define LEFTOVER_MAX_SIZE 4
// uint8_t leftover_buffer[LEFTOVER_MAX_SIZE]; TODO: Remove this if not needed


/**
 * @brief Bluetooth Arbiter State Machine
 * 
 * @param data Pointer to received data
 * @param len Length of received data
 */
void bt_arbiter_sm(uint8_t *data, uint16_t len)
{
    static uint32_t cur_file_size = 0;
    static uint32_t bytes_sent_so_far = 0;
    static uint32_t sent_mdata = 0;
    file_send_cmd_t file_tx_cmd;

    uint32_t recv_mdata = 0;
    BaseType_t sent = pdTRUE;

    // Reset state if RESET command is received
    if (len == RESET_CMD_LEN && cmd_compare((char *)RESET_CMD, data, RESET_CMD_LEN)) {
        ESP_LOGI(TAG, "Received RESET command, resetting state machine");

        // Clean up if RESET happened in the middle of a backup
        if (cur_state == RX_ACTIVE){
            if (ESP_OK != pv_ctx_delete_file(DEFAULT_CLIENT_SERIAL_NUMBER)) {
                PV_LOGE(TAG, "Failed to clean up RX file during reset");
            }
        }
        set_state(WAIT);
        return;
    }

    switch(cur_state)
    {
        case WAIT:
            PV_LOGI(TAG, "ARBITER IN WAIT STATE");
            set_state_action(BT_ARBITER_STATE_ACTION_NONE);
            if(len == RX_STARTM_CMD_LEN || len == RX_GETFLIST_CMD_LEN || 
                len == RX_GETFILE_CMD_LEN || len == DEL_CMD_LEN || 
                len == RENAME_CMD_LEN || len == DEVLIST_DEL_CMD_LEN || 
                len == DEVLIST_MOD_CMD_LEN)
            {
                if(cmd_compare((char *)RX_STARTM_CMD, data, RX_STARTM_CMD_LEN))
                {
                    sent = xRingbufferSend(tx_ringbuf, RX_STARTM_CMD, RX_STARTM_CMD_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        ESP_LOGE(TAG, "Failed to send chunk to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }

                    ESP_LOGI(TAG, "ARBITER ENTERING RX_ACTIVEM MODE");
                    set_state(RX_ACTIVEM);
                    set_state_action(BT_ARBITER_STATE_ACTION_RX_FILE);

                }
                else if(cmd_compare((char *)RX_GETFILE_CMD, data, RX_GETFILE_CMD_LEN))
                {
                    // Reset bytes sent so far
                    bytes_sent_so_far = 0;

                    // Send RX_STARTM_CMD to client
                    sent = xRingbufferSend(tx_ringbuf, RX_STARTM_CMD, RX_STARTM_CMD_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send chunk to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                    ESP_LOGI(TAG, "ARBITER ENTERING RX_ACTIVEM MODE");
                    set_state(RX_ACTIVEM);
                    set_state_action(BT_ARBITER_STATE_ACTION_TX_FILE);
                }                
                else if(cmd_compare((char *)RX_GETFLIST_CMD, data, RX_GETFLIST_CMD_LEN))
                {
                    // Get log file length and send it
                    uint32_t log_file_length = 0;
                    pv_get_log_file_length(DEFAULT_CLIENT_SERIAL_NUMBER, &log_file_length);

                    sent = xRingbufferSend(tx_ringbuf, &log_file_length, sizeof(uint32_t), portMAX_DELAY);
                    if (sent != pdTRUE) {
                        ESP_LOGE(TAG, "Failed to send file length send chunk to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                    ESP_LOGI(TAG, "Sent logfile length %ld to client", log_file_length);
                    sent_mdata = log_file_length;
                    if (!log_file_length){ // Empty log file -> go straight to wait
                        set_state(WAIT);
                    }
                    else{
                        set_state(TX_SNDFLIST);
                    }
                    
                }
                else if (cmd_compare((char*)RENAME_CMD, data, RENAME_CMD_LEN))
                {
                    ESP_LOGI(TAG, "Received rename command from client");

                    // Send RX_STARTM_CMD to client
                    sent = xRingbufferSend(tx_ringbuf, RX_STARTM_CMD, RX_STARTM_CMD_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send chunk to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                    ESP_LOGI(TAG, "ARBITER ENTERING RX_ACTIVEM MODE");
                    set_state(RX_ACTIVEM);
                    set_state_action(BT_ARBITER_STATE_ACTION_RENAME_FILE);
                }
                else if (cmd_compare((char *)DEL_CMD, data, DEL_CMD_LEN))
                {
                    // Delete file command received
                    ESP_LOGI(TAG, "Received delete command from client");
                    
                    // Send RX_STARTM_CMD to client
                    sent = xRingbufferSend(tx_ringbuf, RX_STARTM_CMD, RX_STARTM_CMD_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send chunk to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                    ESP_LOGI(TAG, "ARBITER ENTERING RX_ACTIVEM MODE");
                    set_state(RX_ACTIVEM);
                    set_state_action(BT_ARBITER_STATE_ACTION_DEL_FILE);
                    
                }
                else if (cmd_compare((char *)DEVLIST_DEL_CMD, data, DEVLIST_DEL_CMD_LEN))
                {
                    // Device list delete command received
                    ESP_LOGI(TAG, "Received device list delete command from client");

                    // Get the bluetooth device address from the data (assuming it's right after the command)
                    esp_bd_addr_t bd_addr = {0};
                    char bda_str[BD_ADDR_STR_LENGTH] = {0};

                    memcpy(&bd_addr, data + DEVLIST_DEL_CMD_LEN, sizeof(bd_addr));
                    bda2str(bd_addr, bda_str, BD_ADDR_STR_LENGTH);
                    ESP_LOGI(TAG, "Device address to delete: %s", bda_str);

                    // Delete device from device list
                    esp_err_t err = pv_device_list_delete_device(bd_addr);

                    // Send delete status to client
                    char* response = (err == ESP_OK) ? DELOK_MSG : DELERR_MSG;
                    size_t response_len = (err == ESP_OK) ? DELOK_MSG_LEN : DELERR_MSG_LEN;
                    sent = xRingbufferSend(tx_ringbuf, response, response_len, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send chunk to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                    ESP_LOGI(TAG, "ARBITER ENTERING WAIT MODE");
                    set_state(WAIT);
                    set_state_action(BT_ARBITER_STATE_ACTION_NONE);
                }
                else if (cmd_compare((char *)DEVLIST_MOD_CMD, data, DEVLIST_MOD_CMD_LEN))
                {
                    // Device list modify command received
                    ESP_LOGI(TAG, "Received device list modify command from client");

                    // Get the bluetooth device address from the data (assuming it's right after the command)
                    esp_bd_addr_t bd_addr = {0};
                    char bda_str[BD_ADDR_STR_LENGTH] = {0};
                    char device_name[PV_DEVICE_NAME_MAX_LENGTH] = {0};

                    memcpy(&bd_addr, data + DEVLIST_MOD_CMD_LEN, ESP_BD_ADDR_LEN);
                    bda2str(bd_addr, bda_str, BD_ADDR_STR_LENGTH);
                    ESP_LOGI(TAG, "Device address to modify: %s", bda_str);

                    /* Get name length */
                    uint8_t name_len = *(data + DEVLIST_MOD_CMD_LEN + ESP_BD_ADDR_LEN);
                    if (name_len > PV_DEVICE_NAME_MAX_LENGTH) {
                        PV_LOGE(TAG, "Device name length %d exceeds maximum %d", name_len, PV_DEVICE_NAME_MAX_LENGTH - 1);
                        name_len = PV_DEVICE_NAME_MAX_LENGTH - 1; // Truncate to max length
                    }

                    /* Get device name from command */
                    strncpy(device_name, (char *)(data + DEVLIST_MOD_CMD_LEN + ESP_BD_ADDR_LEN + 1), name_len);
                    device_name[name_len] = '\0'; // Ensure null termination


                    ESP_LOGI(TAG, "Renaming device %s to %s", bda_str, device_name);

                    // Modify device in device list
                    esp_err_t err = pv_device_list_add_device(bd_addr, device_name);

                    // Send rename status to client
                    char* response = (err == ESP_OK) ? RENAMEOK_MSG : RENAMEERR_MSG;
                    size_t response_len = (err == ESP_OK) ? RENAMEOK_CMD_LEN : RENAMEERR_CMD_LEN;
                    sent = xRingbufferSend(tx_ringbuf, response, response_len, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send chunk to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                    ESP_LOGI(TAG, "ARBITER ENTERING WAIT MODE");
                    set_state(WAIT);
                    set_state_action(BT_ARBITER_STATE_ACTION_NONE);
                }
                else
                {
                    PV_LOGE(TAG, "Received unexpected command in WAIT state");
                }
            }
            else
            {
                PV_LOGE(TAG, "Received unexpected data length in WAIT state");
            }
            break;

        case RX_ACTIVEM:
            PV_LOGI(TAG, "ARBITER IN RX_ACTIVEM STATE");

            switch (cur_state_action)
            {
                case BT_ARBITER_STATE_ACTION_RX_FILE:
                    if(len == RX_ENDM_CMD_LEN)
                    {   
                        if(cmd_compare((char *)RX_ENDM_CMD, data, RX_ENDM_CMD_LEN))
                        {
                            ESP_LOGI(TAG, "ARBITER ENTERING RX_ACTIVE MODE");

                            // Start tracking bytes sent
                            bytes_sent_so_far = 0;
                            
                            if (ESP_OK != pv_ctx_create_file()) { // This is needed so we don't keep appending to the same file if it exists (useful for file updates)
                                PV_LOGE(TAG, "Failed to create file for receiving data");
                                set_state(RX_ERROR_STATE);
                                break;
                            }

                            sent = xRingbufferSend(tx_ringbuf, RX_ENDM_CMD, RX_ENDM_CMD_LEN, portMAX_DELAY);
                            if (sent != pdTRUE) {
                                PV_LOGE(TAG, "Failed to send chunk to TX ring buffer\n");
                                set_state(RX_ERROR_STATE);
                                break;
                            }
                            PV_LOGI(TAG, "Ready to receive file size %lu", cur_file_size);
                            set_state(RX_ACTIVE);
                        }
                    }
                    else // Metadata handling if client is sending a file
                    {
                        // Assume whole sent packet is a JSON string (might not be true)
                        process_photo_metadata((char *)data);
                        pv_ctx_get_mdata_fsize(&cur_file_size);
                        pv_ctx_setup_recv_dirs();
                    }
                    // Stay in this state until RX_ENDM_CMD is received
                    break;
                
                case BT_ARBITER_STATE_ACTION_TX_FILE:
                    process_photo_metadata((char *)data);
                    if (ESP_OK != pv_ctx_get_local_fsize(&cur_file_size)) {
                        PV_LOGE(TAG, "Failed to get local file size");
                        set_state(TX_ERROR_STATE);
                        break;
                    }

                    // Send file size to client
                    sent = xRingbufferSend(tx_ringbuf, &cur_file_size, sizeof(uint32_t), portMAX_DELAY);
                    if (sent != pdTRUE) {
                        ESP_LOGE(TAG, "Failed to send file length send chunk to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                    sent_mdata = cur_file_size;
                    PV_LOGI(TAG, "Sent file length %ld to client. Waiting for echo...", cur_file_size);
                    set_state(TX_ACTIVE);

                break;

            case BT_ARBITER_STATE_ACTION_RENAME_FILE:
                PV_LOGI(TAG, "Processing rename metadata");

                process_photo_metadata((char *)data);
                // Rename file
                if (ESP_OK != pv_ctx_rename_file(DEFAULT_CLIENT_SERIAL_NUMBER)) {
                    sent = xRingbufferSend(tx_ringbuf, RENAMEERR_MSG, RENAMEERR_CMD_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send RENAMEERR_MSG to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                } else {
                    sent = xRingbufferSend(tx_ringbuf, RENAMEOK_MSG, RENAMEOK_CMD_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send RENAMEOK_MSG to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                }
                set_state(WAIT);
                break;
                

            case BT_ARBITER_STATE_ACTION_DEL_FILE:
                PV_LOGI(TAG, "Processing delete metadata");

                process_photo_metadata((char *)data);

                // Delete file
                if (ESP_OK != pv_ctx_delete_file(DEFAULT_CLIENT_SERIAL_NUMBER)) {
                    sent = xRingbufferSend(tx_ringbuf, DELERR_MSG, DELERR_MSG_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send DELERR_MSG to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                } else {
                    sent = xRingbufferSend(tx_ringbuf, DELOK_MSG, DELOK_MSG_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send DELOK_MSG to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                    }
                    set_state(WAIT);
                    break;

                case BT_ARBITER_STATE_ACTION_NONE:
                    PV_LOGI(TAG, "No action set in RX_ACTIVEM state");
                    set_state(WAIT);
                    break;

                default:
                    set_state(WAIT);
                    break;
            }

            break;

        case RX_ACTIVE:
            if(bytes_sent_so_far + len < cur_file_size ){
                sent = xRingbufferSend(rx_ringbuf, data, len, portMAX_DELAY);
                if (sent != pdTRUE) {
                    PV_LOGE(TAG, "Failed to send chunk to RX ring buffer\n");
                    set_state(RX_ERROR_STATE);
                    break;
                }
                bytes_sent_so_far += len;
            }
            else
            {
                size_t left_over =  bytes_sent_so_far + len - cur_file_size;
                sent = xRingbufferSend(rx_ringbuf, data, len - left_over, portMAX_DELAY);

                if (ESP_OK != pv_log_rx_file()) {
                    PV_LOGE(TAG, "Failed to log received file");
                    set_state(RX_ERROR_STATE);
                    break;
                }

                // Send RX_OK_MSG to client after receiving all data and finished with logging
                sent = xRingbufferSend(tx_ringbuf, RX_OK_MSG, RX_OK_MSG_LEN, portMAX_DELAY);
                if (sent != pdTRUE) {
                    PV_LOGE(TAG, "Failed to send RX_OK_MSG to TX ring buffer\n");
                    set_state(RX_ERROR_STATE);
                    break;
                }

                PV_LOGI(TAG, "ARBITER LEAVING RX_ACTIVE MODE and going back to WAIT");
                set_state(WAIT);

            }

            break;

        case RX_ERROR_STATE:
            PV_LOGI(TAG, "ARBITER IN RX_ERROR_STATE");
            // pass end data to transfer control
            ESP_LOGI(TAG, "IN ERROR STATE NOT PROCESSED\n");
            break;

        case TX_ACTIVE:
            // Check correct file size echo
            memcpy(&recv_mdata, data, sizeof(recv_mdata));
            if (len == sizeof(sent_mdata)) {
                // Compare received data with sent metadata
                if (memcmp(data, &sent_mdata, sizeof(sent_mdata)) == 0) {
                    PV_LOGI(TAG, "Received log file size %ld echo from client", sent_mdata);
                    PV_LOGI(TAG, "Sending log file to client");

                    // Send file to client
                    file_tx_cmd.send_file = true;
                    xQueueSend(ctx_file_send_queue, &file_tx_cmd, portMAX_DELAY);
                    // uint32_t fbytes_sent = 0; 
                    // if (ESP_OK != pv_ctx_send_file(&fbytes_sent)) {
                    //     set_state(WAIT);
                    //     break;
                    // }

                    // if (fbytes_sent != cur_file_size) {
                    //     PV_LOGE(TAG, "Sent file size %lu does not match requested size %lu", fbytes_sent, cur_file_size);
                    //     set_state(WAIT);
                    //     break;
                    // }

                    set_state(TX_RECVACK);
                } else {
                    PV_LOGE(TAG, "Received file length does not match sent length");
                    PV_LOGE(TAG, "Received %lu, expected %lu", recv_mdata, sent_mdata);
                    // set_state(WAIT);
                }
            } else {
                    PV_LOGE(TAG, "Received unexpected data length for log file length echo in TX_SNDFLIST state");
                    PV_LOGE(TAG, "Received %u, expected %zu", len, sizeof(sent_mdata));
                    // set_state(WAIT);
            }
            break;


        case TX_SNDFLIST:
            PV_LOGI(TAG, "ARBITER IN TX_SNDFLIST STATE");
            /* For MVP log comparison, we send the entire log file.
            May want to have a mechanism to send only the last n logs in future iterations.
            But this works fine as long as log files remain small enough. */

            // Check correct file size echo
            memcpy(&recv_mdata, data, sizeof(recv_mdata));
            if (len == sizeof(sent_mdata)) {
                // Compare received data with sent metadata
                if (memcmp(data, &sent_mdata, sizeof(sent_mdata)) == 0) {
                    PV_LOGI(TAG, "Received log file size %ld echo from client", sent_mdata);
                    PV_LOGI(TAG, "Sending log file to client");

                    // Construct full log file path
                    int log_file_path_name_length = DEVICE_DIRECTORY_NAME_MAX_LENGTH + 1 + sizeof(LOG_FILE_NAME); // +1 for slash, sizeof includes null terminator
                    char log_file_path[log_file_path_name_length];
                    snprintf(log_file_path, log_file_path_name_length, "%s/%s/%s", SD_CARD_BASE_PATH, DEFAULT_CLIENT_SERIAL_NUMBER, LOG_FILE_NAME);

                    // Send log file
                    snprintf(ctx_abs_path_buffer, MAX_PATH_SIZE, "%s", log_file_path);
                    file_tx_cmd.send_file = true;
                    xQueueSend(ctx_file_send_queue, &file_tx_cmd, portMAX_DELAY);
                    // if (ESP_OK != pv_send_file(log_file_path, &fbytes_sent)) {
                    //     PV_LOGE(TAG, "Failed to send file %s", log_file_path);
                    //     set_state(WAIT);
                    //     break;
                    // }

                    set_state(TX_RECVACK);


                } else {
                    PV_LOGE(TAG, "Received file length does not match sent length");
                    PV_LOGE(TAG, "Received %lu, expected %lu", recv_mdata, sent_mdata);
                    // set_state(WAIT);
                }
            } else {
                PV_LOGE(TAG, "Received unexpected data length for log file length echo in TX_SNDFLIST state");
                PV_LOGE(TAG, "Received %u, expected %zu", len, sizeof(sent_mdata));
                // set_state(WAIT);
            }
            break;

        case TX_RECVACK:
            PV_LOGI(TAG, "ARBITER IN TX_RECVACK STATE");
            if (len == TX_OK_MSG_LEN) {
                if (memcmp(data, TX_OK_MSG, TX_OK_MSG_LEN) == 0) {
                    PV_LOGI(TAG, "Received TXOK ack for file transfer");
                    set_state(WAIT);
                } else {
                    PV_LOGE(TAG, "Did not receive expected TXOK ack, received: %.*s", len, data);
                    set_state(TX_ERROR_STATE);
                }
            }
            else {
                PV_LOGE(TAG, "Received unexpected data length in TX_RECVACK state after sending file");
                set_state(TX_ERROR_STATE);
            }

            break;

        case TX_ERROR_STATE:
            PV_LOGI(TAG, "ARBITER IN TX_ERROR_STATE");
            // pass end data to transfer control
            PV_LOGE(TAG, "IN TX ERROR STATE NOT PROCESSED\n");
            break;

    }


}


/***************************************************************************
 * Function:    bt_arbiter_sm_feedin
 * Purpose:     Manage Communications with the Phone. Tells Transfer Controller
 *              What to recieve and what to send
 * Parameters:  uint8_t* data - Ptr to data byte array
 *              uint16_t len - Length of data byte array
 * Return:     None
 * Note:       Will run on callback whenever data is recieved on bluetooth
 *             Should be the only function processing data from bluetooth
 ***************************************************************************/
void bt_arbiter_sm_feedin()
{

    esp_err_t err = ESP_OK;
    size_t pck_len;
    while (1)
    {
        btRingBufferData_t *rb_item = (btRingBufferData_t *)xRingbufferReceive(bt_ringbuf, &pck_len, portMAX_DELAY); // will block forever
        if (rb_item == NULL) {
            ESP_LOGE(TAG, "Failed to receive item from BT ring buffer");
            continue;
        }
        uint8_t *data = rb_item->data;
        uint16_t len = rb_item->data_len;

        
        if (pv_is_device_authorized(rb_item->handle)) { /* Check that the handle is authenticated */
            bt_arbiter_sm(data, len);
        }
        else if(cmd_compare((char *)AUTH_CMD, data, AUTH_CMD_LEN)) { /* Check for auth CMD and handle it */

            /**
             * AUTH CMD format:
             * AUTHCMD/n<pin(4 bytes)><name_length(1 byte)><device_name(variable length, max 128 bytes including null terminator)>
             */

            esp_bd_addr_t bd_addr;
            char device_name[PV_DEVICE_NAME_MAX_LENGTH] = {0};
            pv_pin_t pin = {0};
            uint8_t name_len = 0;

            /* Get the BD_ADDR for this handle */
            pv_get_bda_from_handle(rb_item->handle, bd_addr);

            /* Get new pin from command */
            memcpy(pin, data + AUTH_CMD_LEN, PV_PIN_BYTES_LENGTH);

            /* Get name length */
            name_len = *(data + AUTH_CMD_LEN + PV_PIN_BYTES_LENGTH);

            if (name_len > PV_DEVICE_NAME_MAX_LENGTH) {
                PV_LOGE(TAG, "Device name length %d exceeds maximum %d", name_len, PV_DEVICE_NAME_MAX_LENGTH - 1);
                name_len = PV_DEVICE_NAME_MAX_LENGTH - 1; // Truncate to max length
            }

            /* Get device name from command */
            strncpy(device_name, (char *)(data + AUTH_CMD_LEN + PV_PIN_BYTES_LENGTH + 1), name_len);

            /* If this is the first device being validated, add to devicelist, set the new pin, and mark as authorized */
            if (pv_device_list_get_count() == 0) {
                
                /* Add device to device list */
                err = pv_device_list_add_device(bd_addr, device_name);
                if (err != ESP_OK) {
                    PV_LOGE(TAG, "Failed to add device to device list");
                    xRingbufferSend(tx_ringbuf, AUTH_ERR_MSG, AUTH_ERR_MSG_LEN, portMAX_DELAY);
                    vRingbufferReturnItem(bt_ringbuf, data);
                    continue;
                }

                /* Set the new pin */
                err = pv_set_pin(pin);
                if (err != ESP_OK) {
                    PV_LOGE(TAG, "Failed to set new pin");
                    xRingbufferSend(tx_ringbuf, AUTH_ERR_MSG, AUTH_ERR_MSG_LEN, portMAX_DELAY);
                    vRingbufferReturnItem(bt_ringbuf, data);
                    continue;
                }

                /* Mark device as authenticated */
                err = pv_set_authenticated(rb_item->handle, true);
                if (err != ESP_OK) {
                    PV_LOGE(TAG, "Failed to set device as authenticated");
                    xRingbufferSend(tx_ringbuf, AUTH_ERR_MSG, AUTH_ERR_MSG_LEN, portMAX_DELAY);
                    vRingbufferReturnItem(bt_ringbuf, data);
                    continue;
                }

                /* Send AUTH_OK message */
                xRingbufferSend(tx_ringbuf, AUTH_OK_MSG, AUTH_OK_MSG_LEN, portMAX_DELAY);
                ESP_LOGI(TAG, "Device authenticated and added to device list");

            }
            else { /* This is a new phone but pin has already been configured, check pin, add it to the device list, and mark as authorized */
                
                /* Check if the pin is correct */
                if (pv_cmp_pin(pin)) {
                    /* Add device to device list */
                    err = pv_device_list_add_device(bd_addr, device_name);
                    if (err != ESP_OK) {
                        PV_LOGE(TAG, "Failed to add device to device list");
                        xRingbufferSend(tx_ringbuf, AUTH_ERR_MSG, AUTH_ERR_MSG_LEN, portMAX_DELAY);
                        vRingbufferReturnItem(bt_ringbuf, data);
                        continue;
                    }

                    /* Mark device as authenticated */
                    err = pv_set_authenticated(rb_item->handle, true);
                    if (err != ESP_OK) {
                        PV_LOGE(TAG, "Failed to set device as authenticated");
                        xRingbufferSend(tx_ringbuf, AUTH_ERR_MSG, AUTH_ERR_MSG_LEN, portMAX_DELAY);
                        vRingbufferReturnItem(bt_ringbuf, data);
                        continue;
                    }

                    /* Send AUTH_OK message */
                    xRingbufferSend(tx_ringbuf, AUTH_OK_MSG, AUTH_OK_MSG_LEN, portMAX_DELAY);
                    ESP_LOGI(TAG, "Device authenticated and added to device list");
                }
                else {
                    PV_LOGE(TAG, "Incorrect pin received for authentication");
                    xRingbufferSend(tx_ringbuf, AUTH_ERR_MSG, AUTH_ERR_MSG_LEN, portMAX_DELAY);
                    vRingbufferReturnItem(bt_ringbuf, data);
                    continue;                    
                }
            }
        }
        else {
            PV_LOGE(TAG, "Received data from unauthorized device, rejecting");
            xRingbufferSend(tx_ringbuf, AUTH_ERR_MSG, AUTH_ERR_MSG_LEN, portMAX_DELAY);
        }
        vRingbufferReturnItem(bt_ringbuf, data);
    }

    
}

void init_bt_arbiter_sm()
{
    bt_ringbuf = xRingbufferCreate(BT_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);

    xTaskCreate(bt_arbiter_sm_feedin, "bt_arbiter_sm_feedin", 8192, NULL, 3, NULL);
}
