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

#define RX_START_LEN 9 //exclude null terminator
const uint8_t RX_START_CMD[RX_START_LEN] = {82, 88, 83, 84, 65, 82, 84, 13, 10}; //RXSTART + charg return (will change)

#define RX_STARTM_LEN 10
const uint8_t RX_STARTM_CMD[RX_STARTM_LEN] = {82, 88, 83, 84, 65, 82, 84, 77, 13, 10}; //RXSTARTM + charg return (will change)

#define RX_END_LEN 5 //exclude null terminator
const uint8_t RX_END_CMD[RX_END_LEN] = {69, 78, 68, 13, 10}; //END + charge return
#define RX_ENDM_LEN 6 
const uint8_t RX_ENDM_CMD[RX_ENDM_LEN] = {69, 78, 68, 77, 13, 10}; //ENDM + charge return

#define SPP_TAG "SPP_ACCEPTOR_DEMO"


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

bool cmd_compare(uint8_t * CMD, uint8_t * DATA, uint16_t len)
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

BaseType_t sent = pdTRUE;
void bt_arbiter_sm_feedin(uint8_t* data, uint16_t len)
{
    switch(cur_state)
    {
        case WAIT:
            if(len == RX_STARTM_LEN)
            {
                if(cmd_compare(RX_STARTM_CMD, data, RX_STARTM_LEN))
                {
                    ESP_LOGI(SPP_TAG, "ARBITER ENTERING RX_ACTIVEM MODE");
                    set_state(RX_ACTIVEM);
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
                if(cmd_compare(RX_ENDM_CMD, data, RX_ENDM_LEN))
                {
                    ESP_LOGI(SPP_TAG, "ARBITER ENTERING RX_ACTIVE MODE");
                    set_state(RX_ACTIVE);
                }
            }
            else
            {
                process_meta_data((char *)data, len);
            }
            break;
        case RX_ACTIVE:
            if(len == RX_END_LEN)
            {
                if(cmd_compare(RX_END_CMD, data, RX_END_LEN))
                {
                    //for now send END as end lol
                    sent = xRingbufferSend(rx_ringbuf, data, len, portMAX_DELAY);
                    if (sent != pdTRUE) {
                        ESP_LOGI(SPP_TAG, "Failed to send chunk to RX ring buffer\n");
                        break;
                    }
                    ESP_LOGI(SPP_TAG, "ARBITER LEAVING RX_ACTIVE MODE");
                    set_state(RX_CLEANUP);
                }
            }
            else
            {
                sent = xRingbufferSend(rx_ringbuf, data, len, portMAX_DELAY);
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
