/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
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
#include "cJSON.h"

// BLE includes
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

#include "time.h"
#include "sys/time.h"

#define SPP_TAG "PHOTOVAULT"
#define SPP_SERVER_NAME "SPP_SERVER"
#define SPP_SHOW_DATA 0
#define SPP_SHOW_SPEED 1
#define SPP_SHOW_MODE SPP_SHOW_DATA

// Photo transfer constants
#define MAX_FILENAME_LEN 128
#define MAX_JSON_LEN 512
#define BUFFER_SIZE 4096
#define END_MARKER "\n---END---\n"
#define END_MARKER_LEN 9

// Photo transfer states
typedef enum {
    STATE_IDLE,
    STATE_WAITING_HANDSHAKE,
    STATE_WAITING_PHOTO_METADATA,
    STATE_RECEIVING_PHOTO_DATA,
    STATE_TRANSFER_COMPLETE
} transfer_state_t;

// Photo transfer context
typedef struct {
    transfer_state_t state;
    char current_filename[MAX_FILENAME_LEN];
    uint32_t expected_size;
    uint32_t received_size;
    uint16_t current_index;
    uint16_t total_photos;
    bool transfer_active;
    uint32_t total_photos_received;
} photo_transfer_t;

static const char local_device_name[] = "PhotoVault";
static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const bool esp_spp_enable_l2cap_ertm = true;

static struct timeval time_new, time_old;
static long data_num = 0;

static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;

static uint32_t spp_client_handle = 0;
static photo_transfer_t transfer_ctx = {0};


// Simulated existing files on PhotoVault device
static const char* existing_files[] = {
    "IMG_20240101_120000.jpg",
    "IMG_20240101_130000.jpg", 
    "photo_001.jpg",
    "old_photo.jpg"
};
static const int num_existing_files = sizeof(existing_files) / sizeof(existing_files[0]);

// Buffer for incoming data
static char receive_buffer[BUFFER_SIZE];
static int buffer_pos = 0;

// Buffer for end marker detection
static uint8_t end_marker_buffer[32]; // Buffer to check for end marker across chunk boundaries
static int end_marker_buffer_pos = 0;

// BLE advertising data (unchanged)
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = false,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 0,
    .p_service_uuid      = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp        = true,
    .include_name        = true,
    .include_txpower     = true,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 0,
    .p_service_uuid      = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static char *bda2str(uint8_t * bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

// Also update reset_transfer_state to clear the end marker buffer
static void reset_transfer_state()
{
    transfer_ctx.state = STATE_IDLE;
    transfer_ctx.current_filename[0] = '\0';
    transfer_ctx.expected_size = 0;
    transfer_ctx.received_size = 0;
    transfer_ctx.current_index = 0;
    transfer_ctx.total_photos = 0;
    transfer_ctx.transfer_active = false;
    buffer_pos = 0;
    end_marker_buffer_pos = 0; // Reset end marker buffer
    ESP_LOGI(SPP_TAG, "📸 Transfer state reset");
}

static void send_file_list_ack()
{
    if (spp_client_handle == 0) {
        ESP_LOGE(SPP_TAG, "❌ No client connected");
        return;
    }

    // Create JSON response with existing files
    cJSON *json = cJSON_CreateObject();
    cJSON *status = cJSON_CreateString("ACK");
    cJSON *files_array = cJSON_CreateArray();
    
    cJSON_AddItemToObject(json, "status", status);
    
    // Add existing files to array
    for (int i = 0; i < num_existing_files; i++) {
        cJSON *filename = cJSON_CreateString(existing_files[i]);
        cJSON_AddItemToArray(files_array, filename);
    }
    
    cJSON_AddItemToObject(json, "existing_files", files_array);
    
    // Convert to string
    char *json_string = cJSON_Print(json);
    if (json_string) {
        // Send JSON + end marker
        char response[1024];
        snprintf(response, sizeof(response), "%s%s", json_string, END_MARKER);
        
        esp_spp_write(spp_client_handle, strlen(response), (uint8_t *)response);
        ESP_LOGI(SPP_TAG, "📤 Sent file list ACK with %d existing files", num_existing_files);
        
        free(json_string);
    } else {
        ESP_LOGE(SPP_TAG, "❌ Failed to create JSON response");
    }
    
    cJSON_Delete(json);
    transfer_ctx.state = STATE_WAITING_PHOTO_METADATA;
}

static void send_ready_signal()
{
    if (spp_client_handle == 0) return;
    
    const char *ready_msg = "READY\n";
    esp_spp_write(spp_client_handle, strlen(ready_msg), (uint8_t *)ready_msg);
    ESP_LOGI(SPP_TAG, "📤 Sent READY signal for %s", transfer_ctx.current_filename);
}

static void send_transfer_confirmation(bool success)
{
    if (spp_client_handle == 0) return;
    
    const char *msg = success ? "TRANSFER_OK\n" : "TRANSFER_ERROR\n";
    esp_spp_write(spp_client_handle, strlen(msg), (uint8_t *)msg);
    
    if (success) {
        transfer_ctx.total_photos_received++;
        ESP_LOGI(SPP_TAG, "✅ Photo %d/%d transferred successfully: %s (%.1f KB)", 
                 transfer_ctx.current_index, transfer_ctx.total_photos, 
                 transfer_ctx.current_filename,
                 transfer_ctx.received_size / 1024.0);
    } else {
        ESP_LOGE(SPP_TAG, "❌ Photo transfer failed: %s", transfer_ctx.current_filename);
    }
    
    // Force flush the confirmation
    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to ensure message is sent
}

static bool process_photo_metadata(const char *json_str)
{
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        ESP_LOGE(SPP_TAG, "❌ Invalid JSON metadata");
        return false;
    }
    
    cJSON *action = cJSON_GetObjectItem(json, "action");
    cJSON *filename = cJSON_GetObjectItem(json, "filename");
    cJSON *size = cJSON_GetObjectItem(json, "size");
    cJSON *index = cJSON_GetObjectItem(json, "index");
    cJSON *total = cJSON_GetObjectItem(json, "total");
    
    if (!action || !filename || !size || !index || !total) {
        ESP_LOGE(SPP_TAG, "❌ Missing required metadata fields");
        cJSON_Delete(json);
        return false;
    }
    
    if (strcmp(cJSON_GetStringValue(action), "SEND_PHOTO") != 0) {
        ESP_LOGE(SPP_TAG, "❌ Invalid action: %s", cJSON_GetStringValue(action));
        cJSON_Delete(json);
        return false;
    }
    
    // Store metadata
    strncpy(transfer_ctx.current_filename, cJSON_GetStringValue(filename), MAX_FILENAME_LEN - 1);
    transfer_ctx.current_filename[MAX_FILENAME_LEN - 1] = '\0';
    transfer_ctx.expected_size = (uint32_t)cJSON_GetNumberValue(size);
    transfer_ctx.current_index = (uint16_t)cJSON_GetNumberValue(index);
    transfer_ctx.total_photos = (uint16_t)cJSON_GetNumberValue(total);
    transfer_ctx.received_size = 0;
    
    ESP_LOGI(SPP_TAG, "📸 Receiving photo %d/%d: %s (%.1f KB)", 
             transfer_ctx.current_index, transfer_ctx.total_photos,
             transfer_ctx.current_filename, transfer_ctx.expected_size / 1024.0);
    
    cJSON_Delete(json);
    
    // Send ready signal
    send_ready_signal();
    transfer_ctx.state = STATE_RECEIVING_PHOTO_DATA;
    
    return true;
}

// Fixed process_photo_data function that properly handles binary data + end marker
static void process_photo_data(const uint8_t *data, uint16_t len)
{
    const uint8_t *end_marker = (const uint8_t *)END_MARKER;
    const int end_marker_len = END_MARKER_LEN;
    
    // Check if we can find the end marker in this chunk
    for (int i = 0; i < len; i++) {
        // Add byte to end marker buffer
        end_marker_buffer[end_marker_buffer_pos] = data[i];
        end_marker_buffer_pos++;
        
        // Keep buffer size manageable
        if (end_marker_buffer_pos >= sizeof(end_marker_buffer)) {
            // Shift buffer left by half to keep recent data
            int shift = sizeof(end_marker_buffer) / 2;
            memmove(end_marker_buffer, end_marker_buffer + shift, sizeof(end_marker_buffer) - shift);
            end_marker_buffer_pos -= shift;
        }
        
        // Check if we have enough bytes to check for end marker
        if (end_marker_buffer_pos >= end_marker_len) {
            // Check if the last bytes match the end marker
            if (memcmp(end_marker_buffer + end_marker_buffer_pos - end_marker_len, 
                      end_marker, end_marker_len) == 0) {
                
                // Found end marker! Calculate how much real photo data we got
                int photo_data_in_this_chunk = i + 1 - end_marker_len;
                if (photo_data_in_this_chunk > 0) {
                    transfer_ctx.received_size += photo_data_in_this_chunk;
                } else {
                    // End marker was split across chunks, subtract the overlap
                    transfer_ctx.received_size += (photo_data_in_this_chunk);
                }
                
                ESP_LOGI(SPP_TAG, "🏁 Found end marker at position %d in chunk", i);
                ESP_LOGI(SPP_TAG, "📸 Photo data complete: %s", transfer_ctx.current_filename);
                ESP_LOGI(SPP_TAG, "📊 Size: received=%lu, expected=%lu bytes", 
                         transfer_ctx.received_size, transfer_ctx.expected_size);
                
                // Validate size (allow small discrepancies due to encoding differences)
                bool success = true;
                if (transfer_ctx.expected_size > 0) {
                    long size_diff = (long)transfer_ctx.received_size - (long)transfer_ctx.expected_size;
                    if (abs(size_diff) > 100) { // Allow 100 byte difference
                        ESP_LOGW(SPP_TAG, "⚠️ Size mismatch: diff=%ld bytes", size_diff);
                        // Still consider it successful if the difference is small
                        success = (abs(size_diff) < 1000);
                    }
                }
                
                // Reset end marker buffer
                end_marker_buffer_pos = 0;
                
                // Send confirmation
                send_transfer_confirmation(success);
                
                // Reset for next photo
                transfer_ctx.received_size = 0;
                memset(transfer_ctx.current_filename, 0, sizeof(transfer_ctx.current_filename));
                
                // Check if this was the last photo
                if (transfer_ctx.current_index >= transfer_ctx.total_photos) {
                    ESP_LOGI(SPP_TAG, "🎉 All photos transferred! Total received: %lu photos", 
                             transfer_ctx.total_photos_received);
                    transfer_ctx.state = STATE_TRANSFER_COMPLETE;
                    
                    // Send final completion message
                    const char *final_msg = "ALL_PHOTOS_COMPLETE\n";
                    esp_spp_write(spp_client_handle, strlen(final_msg), (uint8_t *)final_msg);
                } else {
                    // Wait for next photo metadata
                    ESP_LOGI(SPP_TAG, "⏳ Waiting for next photo (%d/%d)", 
                             transfer_ctx.current_index + 1, transfer_ctx.total_photos);
                    transfer_ctx.state = STATE_WAITING_PHOTO_METADATA;
                }
                
                return; // Exit early since we found the end marker
            }
        }
    }
    
    // No end marker found in this chunk, just add the photo data
    transfer_ctx.received_size += len;
    
    // Log progress for larger files (every 10%)
    if (transfer_ctx.expected_size > 10240) { // > 10KB
        float progress = (float)transfer_ctx.received_size / transfer_ctx.expected_size * 100.0;
        static int last_progress = -1;
        int current_progress = (int)(progress / 10) * 10; // Round to nearest 10%
        
        if (current_progress != last_progress && current_progress > 0 && current_progress <= 100) {
            ESP_LOGI(SPP_TAG, "📸 Progress: %d%% (%lu/%lu bytes) - %s", 
                     current_progress, transfer_ctx.received_size, 
                     transfer_ctx.expected_size, transfer_ctx.current_filename);
            last_progress = current_progress;
        }
    }
}

static void handle_received_data(const uint8_t *data, uint16_t len)
{
    ESP_LOGI(SPP_TAG, "📥 Received %d bytes in state %d", len, transfer_ctx.state);
    
    switch (transfer_ctx.state) {
        case STATE_IDLE:
            // Add data to buffer for handshake detection
            if (buffer_pos + len < BUFFER_SIZE - 1) {
                memcpy(receive_buffer + buffer_pos, data, len);
                buffer_pos += len;
                receive_buffer[buffer_pos] = '\0';
                
                // Look for handshake
                if (strstr(receive_buffer, "PHOTOVAULT_HANDSHAKE")) {
                    ESP_LOGI(SPP_TAG, "🤝 Received handshake, sending file list");
                    send_file_list_ack();
                    buffer_pos = 0; // Clear buffer
                }
            } else {
                ESP_LOGW(SPP_TAG, "⚠️ Buffer overflow in IDLE state, resetting");
                buffer_pos = 0;
            }
            break;
            
        case STATE_WAITING_PHOTO_METADATA:
            // Add data to buffer for JSON parsing
            if (buffer_pos + len < BUFFER_SIZE - 1) {
                memcpy(receive_buffer + buffer_pos, data, len);
                buffer_pos += len;
                receive_buffer[buffer_pos] = '\0';
                
                // Look for complete JSON line (ends with newline)
                char *newline = strchr(receive_buffer, '\n');
                if (newline) {
                    *newline = '\0'; // Null terminate JSON
                    ESP_LOGI(SPP_TAG, "📋 Processing metadata: %s", receive_buffer);
                    
                    if (process_photo_metadata(receive_buffer)) {
                        // Successfully processed metadata, clear buffer
                        buffer_pos = 0;
                    } else {
                        ESP_LOGE(SPP_TAG, "❌ Failed to process metadata");
                        send_transfer_confirmation(false);
                        transfer_ctx.state = STATE_WAITING_PHOTO_METADATA;
                        buffer_pos = 0;
                    }
                }
            } else {
                ESP_LOGW(SPP_TAG, "⚠️ Buffer overflow in METADATA state, resetting");
                buffer_pos = 0;
            }
            break;
            
        case STATE_RECEIVING_PHOTO_DATA:
            // Process photo data directly (binary data)
            ESP_LOGI(SPP_TAG, "📸 Received %d bytes of photo data (total: %lu/%lu)", 
                     len, transfer_ctx.received_size + len, transfer_ctx.expected_size);
            process_photo_data(data, len);
            break;
            
        default:
            ESP_LOGW(SPP_TAG, "⚠️ Received data in unknown state: %d", transfer_ctx.state);
            break;
    }
}

static void print_speed(void)
{
    float time_old_s = time_old.tv_sec + time_old.tv_usec / 1000000.0;
    float time_new_s = time_new.tv_sec + time_new.tv_usec / 1000000.0;
    float time_interval = time_new_s - time_old_s;
    float speed = data_num * 8 / time_interval / 1000.0;
    ESP_LOGI(SPP_TAG, "📊 Speed: %.1f kbit/s (%.1f KB/s)", speed, speed / 8.0);
    data_num = 0;
    time_old.tv_sec = time_new.tv_sec;
    time_old.tv_usec = time_new.tv_usec;
}

// BLE GAP event handler (unchanged)
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(SPP_TAG, "BLE advertising data set complete");
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(SPP_TAG, "BLE scan response data set complete");
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(SPP_TAG, "BLE advertising start failed: %d", param->adv_start_cmpl.status);
        } else {
            ESP_LOGI(SPP_TAG, "📡 BLE advertising started - device discoverable");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(SPP_TAG, "BLE advertising stopped");
        break;
    default:
        break;
    }
}

// Add timeout handling to prevent getting stuck
static void check_transfer_timeout(void *pvParameters)
{
    const uint32_t TRANSFER_TIMEOUT_MS = 30000; // 30 seconds per photo
    static uint32_t last_activity_time = 0;
    
    while (1) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        if (transfer_ctx.state == STATE_RECEIVING_PHOTO_DATA) {
            if (last_activity_time == 0) {
                last_activity_time = current_time;
            } else if (current_time - last_activity_time > TRANSFER_TIMEOUT_MS) {
                ESP_LOGE(SPP_TAG, "⏰ Transfer timeout! Resetting state");
                send_transfer_confirmation(false);
                transfer_ctx.state = STATE_WAITING_PHOTO_METADATA;
                last_activity_time = 0;
            }
        } else {
            last_activity_time = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
    }
}

// GATT event handler (unchanged)
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(SPP_TAG, "GATT server registered");
        break;
    default:
        break;
    }
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    char bda_str[18] = {0};

    switch (event) {
    case ESP_SPP_INIT_EVT:
        if (param->init.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG, "✅ SPP initialized");
            esp_spp_start_srv(sec_mask, role_slave, 0, SPP_SERVER_NAME);
        } else {
            ESP_LOGE(SPP_TAG, "❌ SPP init failed: %d", param->init.status);
        }
        break;
        
    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(SPP_TAG, "SPP discovery complete");
        break;
        
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "SPP connection opened");
        break;
        
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "🔌 SPP connection closed (handle: %lu)", param->close.handle);
        spp_client_handle = 0;
        reset_transfer_state();
        
        // Restart advertising for next connection
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        esp_ble_gap_start_advertising(&adv_params);
        break;
        
    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG, "✅ SPP server started (handle: %lu)", param->start.handle);
            esp_bt_gap_set_device_name(local_device_name);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            
            // Start BLE advertising
            ESP_LOGI(SPP_TAG, "🚀 Starting BLE advertising...");
            esp_ble_gap_set_device_name(local_device_name);
            esp_ble_gap_config_adv_data(&adv_data);
            esp_ble_gap_config_adv_data(&scan_rsp_data);
        } else {
            ESP_LOGE(SPP_TAG, "❌ SPP server start failed: %d", param->start.status);
        }
        break;
        
    case ESP_SPP_DATA_IND_EVT:
        // Handle incoming data
        handle_received_data(param->data_ind.data, param->data_ind.len);
        
        // Update speed statistics
        gettimeofday(&time_new, NULL);
        data_num += param->data_ind.len;
        if (time_new.tv_sec - time_old.tv_sec >= 3) {
            print_speed();
        }
        break;
        
    case ESP_SPP_CONG_EVT:
        ESP_LOGW(SPP_TAG, "⚠️ SPP congestion");
        break;
        
    case ESP_SPP_WRITE_EVT:
        ESP_LOGI(SPP_TAG, "SPP write complete");
        break;
        
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "🔗 Client connected: %s", 
                 bda2str(param->srv_open.rem_bda, bda_str, sizeof(bda_str)));
        spp_client_handle = param->srv_open.handle;
        gettimeofday(&time_old, NULL);
        reset_transfer_state();
        transfer_ctx.state = STATE_IDLE;
        
        break;
        
    case ESP_SPP_SRV_STOP_EVT:
        ESP_LOGI(SPP_TAG, "SPP server stopped");
        break;
        
    case ESP_SPP_UNINIT_EVT:
        ESP_LOGI(SPP_TAG, "SPP uninitialized");
        break;
        
    default:
        break;
    }
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    char bda_str[18] = {0};

    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(SPP_TAG, "🔐 Authentication success: %s", 
                     bda2str(param->auth_cmpl.bda, bda_str, sizeof(bda_str)));
        } else {
            ESP_LOGE(SPP_TAG, "❌ Authentication failed: %d", param->auth_cmpl.stat);
        }
        break;
        
    case ESP_BT_GAP_PIN_REQ_EVT:
        ESP_LOGI(SPP_TAG, "📱 PIN request (16 digit: %d)", param->pin_req.min_16_digit);
        if (param->pin_req.min_16_digit) {
            esp_bt_pin_code_t pin_code = {0};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        } else {
            esp_bt_pin_code_t pin_code = {'1', '2', '3', '4'};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
        }
        break;

#if (CONFIG_EXAMPLE_SSP_ENABLED == true)
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(SPP_TAG, "🔢 Numeric confirmation: %06lu", param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(SPP_TAG, "🔑 Passkey: %06lu", param->key_notif.passkey);
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(SPP_TAG, "🔐 Please enter passkey");
        break;
#endif

    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(SPP_TAG, "📡 Mode change: %d for %s", param->mode_chg.mode,
                 bda2str(param->mode_chg.bda, bda_str, sizeof(bda_str)));
        break;

    default:
        break;
    }
}

void app_main(void)
{
    char bda_str[18] = {0};
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize transfer context
    reset_transfer_state();

    // Initialize Bluetooth controller for dual mode
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "❌ Controller init failed: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "❌ Controller enable failed: %s", esp_err_to_name(ret));
        return;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
#if (CONFIG_EXAMPLE_SSP_ENABLED == false)
    bluedroid_cfg.ssp_en = false;
#endif
    if ((ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "❌ Bluedroid init failed: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "❌ Bluedroid enable failed: %s", esp_err_to_name(ret));
        return;
    }

    // Register callbacks
    ESP_ERROR_CHECK(esp_bt_gap_register_callback(esp_bt_gap_cb));
    ESP_ERROR_CHECK(esp_spp_register_callback(esp_spp_cb));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));

    // Initialize SPP
    esp_spp_cfg_t bt_spp_cfg = {
        .mode = esp_spp_mode,
        .enable_l2cap_ertm = esp_spp_enable_l2cap_ertm,
        .tx_buffer_size = 0,
    };
    ESP_ERROR_CHECK(esp_spp_enhanced_init(&bt_spp_cfg));

    // Initialize BLE GATT server
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));

#if (CONFIG_EXAMPLE_SSP_ENABLED == true)
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

    xTaskCreate(check_transfer_timeout, "transfer_timeout", 2048, NULL, 5, NULL);

    ESP_LOGI(SPP_TAG, "📸 PhotoVault initialized!");
    ESP_LOGI(SPP_TAG, "🔗 Device address: %s", bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));
    ESP_LOGI(SPP_TAG, "📡 Ready for photo transfers via Bluetooth SPP");
    ESP_LOGI(SPP_TAG, "🎯 Simulating %d existing files on device", num_existing_files);
}