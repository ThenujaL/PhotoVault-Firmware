#pragma once

#include "esp_err.h"
#include "sdmmc_cmd.h"
#include <sys/stat.h>
#include "esp_vfs_fat.h"

/* FUNCTION DEFS */
esp_err_t pv_init_sdc(void);
void pv_test_sdc(void);
void pv_card_get(sdmmc_card_t **out_card);