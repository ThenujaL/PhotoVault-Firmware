#pragma once

#include "esp_err.h"
#include "sdmmc_cmd.h"

/* GLOBAL VARS */
extern 

/* FUNCTION DEFS */
esp_err_t pv_init_sdc(void);
void pv_test_sdc(void);
sdmmc_card_t *card_get(void);