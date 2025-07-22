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

#define RX_START_LEN 8 //exclude null terminator
const char RX_START_CMD[RX_START_LEN] = "RXSTART\n"; //RXSTART

#define RX_STARTM_LEN 9
const char RX_STARTM_CMD[RX_STARTM_LEN] = "RXSTARTM\n"; //RXSTARTM

#define RX_END_LEN 4 //exclude null terminator
const char RX_END_CMD[RX_END_LEN] = "END\n"; //END
#define RX_ENDM_LEN 5
const char RX_ENDM_CMD[RX_ENDM_LEN] = "ENDM\n"; //ENDM

#define SPP_TAG "SPP_ACCEPTOR_DEMO"
#define ACK_LEN 3
const char ACK[ACK_LEN] = "ACK"; //ACK 


typedef enum state {
    WAIT, 
    RX_ACTIVEM,
    RX_ACTIVE, 
    RX_ERROR_STATE, 
}BT_ARBITER_STATE;


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

size_t cur_file_size = 0;
size_t bytes_sent_so_far = 0;

#define LEFTOVER_MAX_SIZE 4
uint8_t leftover_buffer[LEFTOVER_MAX_SIZE];

BaseType_t sent = pdTRUE;

/***************************************************************************
 * Function:    bt_arbiter_sm_feedin
 * Purpose:     Manage Communications with the Phone. Tells Transfer Controller
 *              What to recieve and what to send
 * Parameters:  Data in Bluetooth Packet, Amount of Bytes of Data in Bluetooth Packet
 * Return:     None
 * Note:       Will run on callback whenever data is recieved on bluetooth
 *             Should be the only function processing data from bluetooth
 ***************************************************************************/
void bt_arbiter_sm_feedin(uint8_t* data, uint16_t len)
{
    switch(cur_state)
    {
        case WAIT:
            if(len == RX_STARTM_LEN)
            {
                if(cmd_compare((char *)RX_STARTM_CMD, data, RX_STARTM_LEN))
                {
                    ESP_LOGI(SPP_TAG, "ARBITER ENTERING RX_ACTIVEM MODE");
                    set_state(RX_ACTIVEM);

                    sent = xRingbufferSend(tx_ringbuf, RX_STARTM_CMD, RX_STARTM_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        ESP_LOGE(SPP_TAG, "Failed to send chunk to TX ring buffer");
                        break;
                    }
                }
            }
            else
            {
                // not recognized
            }
            break;
        case RX_ACTIVEM:
            if(len == RX_ENDM_LEN)
            {   
                if(cmd_compare((char *)RX_ENDM_CMD, data, RX_ENDM_LEN))
                {
                    ESP_LOGI(SPP_TAG, "ARBITER ENTERING RX_ACTIVE MODE");
                    set_state(RX_ACTIVE);

                    // Start tracking bytes sent
                    bytes_sent_so_far = 0;

                    sent = xRingbufferSend(tx_ringbuf, RX_ENDM_CMD, RX_ENDM_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send chunk to TX ring buffer\n");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                }
            }
            else
            {
                // Assume whole sent packet is a JSON string (might not be true)
                process_photo_metadata((char *)data, &cur_file_size);
            }
            break;
        case RX_ACTIVE:
            if(bytes_sent_so_far + len < cur_file_size ){
                sent = xRingbufferSend(rx_ringbuf, data, len, portMAX_DELAY);
                bytes_sent_so_far += len;
            }
            else
            {
                size_t left_over =  bytes_sent_so_far + len - cur_file_size;
                sent = xRingbufferSend(rx_ringbuf, data, len - left_over, portMAX_DELAY);
                for(int i = 0; i<left_over; i++)
                {
                    leftover_buffer[i] = data[len - left_over + i];
                }
                if(cmd_compare((char *)RX_END_CMD, leftover_buffer, RX_END_LEN))
                {
                    ESP_LOGI(SPP_TAG, "ARBITER LEAVING RX_ACTIVE MODE");
                    set_state(WAIT);
                    sent = xRingbufferSend(tx_ringbuf, RX_END_CMD, RX_END_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        PV_LOGE(TAG, "Failed to send chunk to TX ring buffer\n");
                        set_state(RX_ERROR_STATE);
                        break;
                    }
                }
                else
                {
                    PV_LOGE(TAG, "ERROR! DID NOT RECIEVE VALID END OF FILE CMD");
                    set_state(RX_ERROR_STATE);
                }

            }
            if (sent != pdTRUE) {
                PV_LOGE(TAG, "Failed to send chunk to RX ring buffer\n");
                break;
            }
            break;
        case RX_ERROR_STATE:
            // pass end data to transfer control
            ESP_LOGI(SPP_TAG, "IN ERROR STATE NOT PROCESSED\n");
            break;
    }
    
}
