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
    RX_CLEANUP, 
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
size_t iter_file_size = 0;

#define LEFTOVER_MAX_SIZE 4
uint8_t leftover_buffer[LEFTOVER_MAX_SIZE];

BaseType_t sent = pdTRUE;
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
                        ESP_LOGI(SPP_TAG, "Failed to send chunk to TX ring buffer\n");
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
                    iter_file_size = 0;

                    sent = xRingbufferSend(tx_ringbuf, RX_ENDM_CMD, RX_ENDM_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        ESP_LOGI(SPP_TAG, "Failed to send chunk to TX ring buffer\n");
                        break;
                    }
                }
            }
            else
            {
                process_photo_metadata((char *)data, &cur_file_size);
            }
            break;
        case RX_ACTIVE:
            if(len == RX_END_LEN)
            {
                if(cmd_compare((char *)RX_END_CMD, data, RX_END_LEN))
                {
                    //for now send END as end lol
                    sent = xRingbufferSend(rx_ringbuf, data, len, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        ESP_LOGI(SPP_TAG, "Failed to send chunk to RX ring buffer\n");
                        break;
                    }
                    ESP_LOGI(SPP_TAG, "ARBITER LEAVING RX_ACTIVE MODE");
                    set_state(WAIT);
                    sent = xRingbufferSend(tx_ringbuf, RX_END_CMD, RX_END_LEN, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        ESP_LOGI(SPP_TAG, "Failed to send chunk to TX ring buffer\n");
                        break;
                    }
                }
            }
            else
            {
                if(iter_file_size + len < cur_file_size ){
                    sent = xRingbufferSend(rx_ringbuf, data, len, portMAX_DELAY);
                    iter_file_size += len;
                }
                else
                {
                    size_t left_over =  iter_file_size + len - cur_file_size;
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
                            ESP_LOGI(SPP_TAG, "Failed to send chunk to TX ring buffer\n");
                            break;
                        }
                    }

                }
                if (sent != pdTRUE) {
                    ESP_LOGI(SPP_TAG, "Failed to send chunk to RX ring buffer\n");
                    break;
                }
            }
            break;
        case RX_CLEANUP:
            // pass end data to transfer control
            set_state(WAIT);
            break;
    }
    
}
