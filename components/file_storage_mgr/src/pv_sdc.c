#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "pv_logging.h"

#include "unity.h"
#include "sdc_tests.h"

#include "pv_sdc.h"
#include "board_config.h"

#define TAG "PV_SDC"

/* STATIC VARIABLES */
static sdmmc_card_t *s_card = NULL;
static sdmmc_host_t host = SDSPI_HOST_DEFAULT();
static sdspi_device_config_t devConfig = SDSPI_DEVICE_CONFIG_DEFAULT();

/* TODO: Move spi bus functions to separete PV file */
const spi_bus_config_t pv_config_spi2_bus_cfg = {
    .mosi_io_num = PV_CONFIG_PIN_MOSI,
    .miso_io_num = PV_CONFIG_PIN_MISO,
    .sclk_io_num = PV_CONFIG_PIN_SCLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4000,
};

/***************************************************************************
 * Function:    pv_card_get
 * Purpose:     Helper function to get the sd card pointer that was initialized
 * Parameters:  sdmmc_card_t *out_card Pointer to store the card information
 *              after initialization.
 * Returns:     None
 ***************************************************************************/
void pv_card_get(sdmmc_card_t **out_card) {
    *out_card = s_card;
}

/***************************************************************************
 * Function:    pv_init_sdc
 * Purpose:     Initializes the SD card device using the provided device
 *              configuration structure. Sets up the device handle and
 *              calls the necessary initialization routines for SPI SD card
 *              communication.
 * Parameters:  None
 * Returns:     PV_ERR_CODE_SUCCESS on success, PV_ERR_CODE_UNKNOWN else.
 * Notes:       This function is NOT thread safe
 ***************************************************************************/
esp_err_t pv_init_sdc(void){
    esp_err_t ret;
    sdspi_dev_handle_t sdcDevhandle;
    s_card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    if (s_card == NULL) { 
        PV_LOGE(TAG, "Failed to allocate memory for sdmmc_card_t.");
        return ESP_ERR_NO_MEM; // Memory allocation failed
    }

    /* Reduce host freq. from default */
    host.max_freq_khz = 4000;

    ret = spi_bus_initialize(host.slot, &pv_config_spi2_bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        PV_LOGE(TAG, "Failed to initialize SPI bus.");
        return ret;
    }

    /* Modify defaults */
    devConfig.gpio_cs = PV_CONFIG_PIN_CS;
    devConfig.host_id = host.slot;

    ret = sdspi_host_init_device(&devConfig, &sdcDevhandle);
    if (ret != ESP_OK) {
        PV_LOGE(TAG, "Failed to initialize the SD SPI device and attach to SPI bus.");
        return ret;
    }

    ret = sdmmc_card_init(&host, s_card);
    if (ret != ESP_OK) {
        PV_LOGE(TAG, "Failed to init SDC using given host.");
        return ret;
    }
    sdmmc_card_print_info(stdout, s_card);

    return ESP_OK;
}


/***************************************************************************
 * Function:    pv_test_sdc
 * Purpose:     Run Unity Test Framework tests for SDC
 * Parameters:  None
 * Returns:     None
 ***************************************************************************/
void pv_test_sdc(void){
    UNITY_BEGIN();
    RUN_TEST(test_sdcWriteFile);
    RUN_TEST(test_log_writes);
    RUN_TEST(test_log_checks);
    UNITY_END();  
}