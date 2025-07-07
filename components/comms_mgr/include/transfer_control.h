#ifndef TRANSFER_CONTROL_H
#define TRANSFER_CONTROL_H

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>


#define RX_RINGBUF_SIZE 4096
#define TX_RINGBUF_SIZE 4096
#define DUMMY_FILE_BUF_SIZE 8192

#define TRANSFER_TYPE_RX 0
#define TRANSFER_TYPE_TX 1

#define PV_ERR_SEND_FAIL 1
#define PV_ERR_RECV_FAIL 2


typedef struct
{
    char file_path[128];
    uint8_t transfer_type; // TRANSFER_TYPE_TX or TRANSFER_TYPE_RX
    uint8_t status;        // PV_ERR_SEND_FAIL, PV_ERR_RECV_FAIL, or 0 on success
} transfer_cmd_t;

// declare variables whose definitions are present in c file
extern QueueHandle_t tx_cmd_queue;
extern QueueHandle_t status_queue;
extern RingbufHandle_t rx_ringbuf;
extern RingbufHandle_t tx_ringbuf;

void transfer_control_init();
void receiver_task();
void transmitter_task();

//testing
extern volatile int success_flag;
void dummy_bt_task();
void dummy_backup_task();
void start_transfer_control_tests();


#endif
