#ifndef TRANSFER_CONTROL_H
#define TRANSFER_CONTROL_H

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "cJSON.h"
#include "pv_fs.h"
#include "pv_sdc.h"

#define RX_RINGBUF_SIZE                             4096
#define TX_RINGBUF_SIZE                             4096
#define INITIAL_BUFFER_SIZE                         4096
#define MAX_PATH_SIZE                               BACKUP_PATH_MAX_LENGTH


#define PV_TX_CHUNK_SIZE                            800 /* Size of each chunk sent by transmitter task */


typedef struct {
    bool send_file;
} file_send_cmd_t;

// declare variables whose definitions are present in c file
extern RingbufHandle_t rx_ringbuf;
extern RingbufHandle_t tx_ringbuf;
extern QueueHandle_t ctx_file_send_queue;


void transfer_control_init();
void transfer_control_set_bt(uint32_t bt_handle);
void receiver_task();
void transmitter_task();
esp_err_t process_photo_metadata(const char *json_str, pv_file_metadata_t *metadata_out, pv_android_device_id_t *android_id);
void pv_ctx_setup_recv_dirs(void);
esp_err_t pv_ctx_delete_file(const char *serial_number);
esp_err_t pv_ctx_send_file(uint32_t *bytes_sent);
esp_err_t pv_ctx_get_local_fsize(uint32_t *file_size);
esp_err_t pv_ctx_create_file(void);
esp_err_t pv_ctx_rename_file(const char* serial_number);
void pv_ctx_get_mdata_fsize(uint32_t *file_size);
esp_err_t pv_send_file(const char *file_path, uint32_t *bytes_sent);
esp_err_t pv_log_rx_file(void);
extern char *ctx_abs_path_buffer;
extern volatile bool g_spp_congested;

#endif