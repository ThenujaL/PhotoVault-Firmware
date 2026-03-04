

#include "pv_bt_utils.h"
#include "pv_transfer_context.h"


/* Global Variables */
pv_transfer_context_t transfer_context_list[MAX_DEVICE_CONNECTIONS];



/* Function Definitions */

/**
 * @brief Get the transfer context struct for a given device handle.
 * @param handle The device handle for which to retrieve the transfer context.
 * @return A pointer to the transfer context struct if found, otherwise NULL.
 */
pv_transfer_context_t* pv_get_transfer_context_by_handle(uint32_t handle) {
    for (int i = 0; i < MAX_DEVICE_CONNECTIONS; i++) {
        if (transfer_context_list[i].device_handle == handle) {
            return &transfer_context_list[i];
        }
    }
    return NULL;
}