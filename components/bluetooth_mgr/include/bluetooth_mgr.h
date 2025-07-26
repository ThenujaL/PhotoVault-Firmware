#include "pv_logging.h"

#define CONG_RETRY_DELAY_MS 10 // Delay in milliseconds to retry sending data when congested

extern volatile uint32_t g_spp_congested; // Congestion flag

void register_bluetooth_callbacks(void);