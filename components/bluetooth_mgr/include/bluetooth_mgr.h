#include "pv_logging.h"

#define CONG_RETRY_DELAY_MS 2 // Delay in milliseconds to retry sending data when congested

extern volatile bool g_spp_congested; // Congestion flag


/**
 * @brief Structure for data to be sent via ring buffer.
 */
typedef struct {
    uint32_t handle;            /*!< The connection handle */
    size_t data_len;            /*!< The length of data */
    uint8_t data[];             /*!< The data received */
} btRingBufferData_t;

void register_bluetooth_callbacks(void);