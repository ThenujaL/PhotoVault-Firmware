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

// BLE includes
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

#define RX_START_LEN 7 //exclude null terminator
#define RX_START_CMD "RXSTART"

#define RX_END_LEN 3 //exclude null terminator
#define RX_END_CMD "END"



typedef enum state {
    WAIT, 
    RX_ACTIVE, 
    RX_CLEANUP, 
}BT_ARBITER_STATE;


struct spp_data_ind_evt_param cur_data;




BT_ARBITER_STATE cur_state = WAIT;

struct bt_arbiter_sm_cmd_line {
    uint16_t            len;            /*!< The length of data */
    uint8_t             *data;          /*!< The data received */
} cmd_line;     

bool cmd_compare(char * CMD, char * DATA, uint16_t len)
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

void bt_arbiter_sm_feedin(data_ind* cur_data)
{
    switch(cur_state)
    {
        case WAIT:
            if(cur_data->len == RX_START_LEN)
            {
                if(cmd_compare(RX_START_CMD, cur_data->data, RX_START_LEN))
                {
                    set_state(RX_ACTIVE);
                }
            }
            else
            {
                // not recognized
            }
            break;
        case RX_ACTIVE:
            if(cur_data->len == RX_END_LEN)
            {
                if(cmd_compare(RX_END_CMD, cur_data->data, RX_END_LEN))
                {
                    set_state(RX_CLEANUP);
                }
            }
            else
            {
                // pass data to transfer control
            }
            break;
        case RXEND:
            // pass end data to transfer control
            set_state(WAIT);
            break;
    }
    
}
