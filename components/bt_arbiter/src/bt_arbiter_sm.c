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



struct spp_data_ind_evt_param cur_data;




BT_ARBITER_STATE cur_state = WAIT;

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

void set_state(BT_ARBITER_STATE new_state)
{
    cur_state = new_state;
}


#define LEFTOVER_MAX_SIZE 4
// uint8_t leftover_buffer[LEFTOVER_MAX_SIZE]; TODO: Remove this if not needed


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
void bt_arbiter_sm_feedin(uint8_t* data, uint16_t len)
{
    esp_err_t err = ESP_OK;

    static size_t cur_file_size = 0;
    static size_t bytes_sent_so_far = 0;
    static uint32_t sent_mdata = 0;
    static uint32_t abs_path_len = 0;
    static char abs_path_buffer[MAX_PATH_SIZE] = {0};

    static bool getfile = false; // Flag to indicate if we are in the process of getting a file (until we figure out a better way)

    uint32_t recv_mdata = 0;
    BaseType_t sent = pdTRUE;

    


    switch(cur_state)
    {
        case WAIT:
            getfile = false; // Reset getfile flag
            PV_LOGI(TAG, "ARBITER IN WAIT STATE");
            if(len == RX_STARTM_CMD_LEN || len == RX_GETFLIST_CMD_LEN || len == RX_GETFILE_CMD_LEN)
            {
                if(cmd_compare((char *)RX_STARTM_CMD, data, RX_STARTM_CMD_LEN))
                {
                    ESP_LOGI(TAG, "ARBITER ENTERING RX_ACTIVEM MODE");
                    set_state(RX_ACTIVEM);

                    sent = xRingbufferSend(tx_ringbuf, RX_STARTM_CMD, RX_STARTM_CMD_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        ESP_LOGE(TAG, "Failed to send chunk to TX ring buffer");
                        break;
                    }
                }
                else if(cmd_compare((char *)RX_GETFILE_CMD, data, RX_GETFILE_CMD_LEN))
                {
                    getfile = true; // Set flag to indicate we are getting a file
                    ESP_LOGI(TAG, "ARBITER ENTERING RX_ACTIVEM MODE");

                    // Reset bytes sent so far
                    bytes_sent_so_far = 0;

                    // Send RX_STARTM_CMD to client
                    sent = xRingbufferSend(tx_ringbuf, RX_STARTM_CMD, RX_STARTM_CMD_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send chunk to TX ring buffer");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                    set_state(RX_ACTIVEM);
                }                
                else if(cmd_compare((char *)RX_GETFLIST_CMD, data, RX_GETFLIST_CMD_LEN))
                {
                    // Get log file length and send it
                    uint32_t log_file_length = 0;
                    pv_get_log_file_length(DEFAULT_CLIENT_SERIAL_NUMBER, &log_file_length);

                    sent = xRingbufferSend(tx_ringbuf, &log_file_length, sizeof(uint32_t), portMAX_DELAY);
                    if (sent != pdTRUE) {
                        ESP_LOGE(TAG, "Failed to file length send chunk to TX ring buffer");
                        break;
                    }
                    sent_mdata = log_file_length;
                    ESP_LOGI(TAG, "Sent logfile length %ld to client", log_file_length);
                    set_state(TX_SNDFLIST);
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
            if(len == RX_ENDM_CMD_LEN && !getfile)
            {   
                if(cmd_compare((char *)RX_ENDM_CMD, data, RX_ENDM_CMD_LEN))
                {
                    ESP_LOGI(TAG, "ARBITER ENTERING RX_ACTIVE MODE");
                    set_state(RX_ACTIVE);

                    // Start tracking bytes sent
                    bytes_sent_so_far = 0;

                    sent = xRingbufferSend(tx_ringbuf, RX_ENDM_CMD, RX_ENDM_CMD_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send chunk to TX ring buffer\n");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                }
            }
            else if(getfile) // Metadata handling if file is being requested by client
            {
                uint32_t path_len = 0;
                process_photo_metadata((char *)data, &cur_file_size, &path_len);
                pv_get_cxt_file_path(abs_path_buffer, path_len, &abs_path_len);
                PV_LOGI(TAG, "Received file metadata, file size: %zu, abs path length: %lu", cur_file_size, abs_path_len);
                PV_LOGI(TAG, "Absolute file path: %s", abs_path_buffer);

                // Send file size to client
                uint32_t fbytes_sent = 0; 
                err = pv_send_file(abs_path_buffer, &fbytes_sent);
                if (err != ESP_OK) {
                    PV_LOGE(TAG, "Failed to send file %s", abs_path_buffer);
                    set_state(WAIT);
                    break;
                }

                if (fbytes_sent != cur_file_size) {
                    PV_LOGE(TAG, "Sent file size %lu does not match requested size %zu", fbytes_sent, cur_file_size);
                    set_state(WAIT);
                    break;
                }

                set_state(TX_RECVACK);
            }
            else // Metadata handling if client is sending a file
            {
                // Assume whole sent packet is a JSON string (might not be true)
                uint32_t path_len = 0;
                process_photo_metadata((char *)data, &cur_file_size, &path_len);
                process_file_path(path_len);
            }
            break;
        case RX_ACTIVE:
            PV_LOGI(TAG, "ARBITER IN RX_ACTIVE STATE");
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

                err = pv_log_rx_file();
                if (err != ESP_OK) {
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
                
                // for(int i = 0; i<left_over; i++)
                // {
                //     leftover_buffer[i] = data[len - left_over + i];
                // }
                // if(cmd_compare((char *)END_CMD, leftover_buffer, END_CMD_LEN))
                // {
                //     ESP_LOGI(TAG, "ARBITER LEAVING RX_ACTIVE MODE");
                //     set_state(WAIT);
                //     sent = xRingbufferSend(tx_ringbuf, END_CMD, END_CMD_LEN, portMAX_DELAY);
                //     if (sent != pdTRUE) {
                //         PV_LOGE(TAG, "Failed to send chunk to TX ring buffer\n");
                //         set_state(RX_ERROR_STATE);
                //         break;
                //     }
                // }
                // else
                // {
                //     PV_LOGE(TAG, "ERROR! DID NOT RECIEVE VALID END OF FILE CMD");
                //     set_state(RX_ERROR_STATE);
                // }

            }
            break;

        case RX_ERROR_STATE:
            PV_LOGI(TAG, "ARBITER IN RX_ERROR_STATE");
            // pass end data to transfer control
            ESP_LOGI(TAG, "IN ERROR STATE NOT PROCESSED\n");
            break;

        case TX_SNDFLIST:
            PV_LOGI(TAG, "ARBITER IN TX_SNDFLIST STATE");
            /* For MVP log comparison, we send the entire log file.
            May want to have a mechanism to send only the last n logs in future iterations.
            But this works fine as long as longs files remain small enough. */

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
                    uint32_t fbytes_sent = 0; // Dummber variable to match function signature - not used as client does not tell us file length
                    err = pv_send_file(log_file_path, &fbytes_sent);
                    if (err != ESP_OK) {
                        PV_LOGE(TAG, "Failed to send file %s", abs_path_buffer);
                        set_state(WAIT);
                        break;
                    }

                    set_state(TX_RECVACK);


                } else {
                    PV_LOGE(TAG, "Received file length does not match sent length");
                    PV_LOGE(TAG, "Received %ld, expected %ld", recv_mdata, sent_mdata);
                    // set_state(WAIT);
                }
            } else {
                PV_LOGE(TAG, "Received unexpected data length for log file length echo in TX_SNDFLIST state");
                PV_LOGE(TAG, "Received %d, expected %zu", len, sizeof(sent_mdata));
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
