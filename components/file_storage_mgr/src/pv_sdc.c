#include "driver/sdspi_host.h"

#include "unity.h"
#include "bdev_tests.h"

#include "pv_bdev.h"
#include "pv_errors.h"
/***************************************************************************
 * Function:    pv_init_sdc
 * Purpose:     Initializes the SD card device using the provided device
 *              configuration structure. Sets up the device handle and
 *              calls the necessary initialization routines for SPI SD card
 *              communication.
 * Parameters:  None
 * Returns:     PV_ERR_CODE_SUCCESS on success, PV_ERR_CODE_UNKNOWN else.
 * Notes:       This function assumes that the SPI host has already been
 *              initialized (by calling spi_bus_initialize()) prior to calling this function.
 ***************************************************************************/
pv_error_code_t pv_init_sdc(void){

}


/***************************************************************************
 * Function:    pv_test_sdc
 * Purpose:     Run Unity Test Framework tests for SDC
 * Parameters:  None
 * Returns:     None
 ***************************************************************************/
void pv_test_sdc(void){
    UNITY_BEGIN();
    RUN_TEST(test_sdcReadWrite);
    UNITY_END();  
}