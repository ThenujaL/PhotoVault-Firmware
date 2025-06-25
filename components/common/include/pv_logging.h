#ifndef __LOG__
#define __LOG__

#include "esp_log.h"

#define __FORMAT(FORMAT) "(%s:%d) " FORMAT

#define PV_LOGD(TAG, FORMAT, ...) ESP_LOGD(TAG, __FORMAT(FORMAT), __func__, __LINE__, ##__VA_ARGS__)
#define PV_LOGI(TAG, FORMAT, ...) ESP_LOGI(TAG, __FORMAT(FORMAT), __func__, __LINE__, ##__VA_ARGS__)
#define PV_LOGW(TAG, FORMAT, ...) ESP_LOGW(TAG, __FORMAT(FORMAT), __func__, __LINE__, ##__VA_ARGS__)
#define PV_LOGE(TAG, FORMAT, ...) ESP_LOGE(TAG, __FORMAT(FORMAT), __func__, __LINE__, ##__VA_ARGS__)

#endif