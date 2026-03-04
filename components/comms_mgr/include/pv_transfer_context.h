#pragma once

#include "pv_bt_utils.h"
#include "pv_sdc.h"

typedef struct {
    pv_file_metadata_t file_metadata;
    uint32_t device_handle;
} pv_transfer_context_t;

extern pv_transfer_context_t transfer_context_list[MAX_DEVICE_CONNECTIONS];


pv_transfer_context_t* pv_get_transfer_context_by_handle(uint32_t handle);