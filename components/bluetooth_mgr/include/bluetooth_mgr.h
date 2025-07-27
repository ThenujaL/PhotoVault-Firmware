#include "pv_logging.h"

#define CONG_RETRY_DELAY_MS 20 // Delay in milliseconds to retry sending data when congested

extern volatile bool g_spp_congested; // Congestion flag

void register_bluetooth_callbacks(void);