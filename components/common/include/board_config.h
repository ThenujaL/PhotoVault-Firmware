#pragma once

#include "driver/sdspi_host.h"


/* SPI Config */

/* 
    Note: Any GPIO pin can be used, but these are dedicated IO_MUX pins that are hardwired 
    to SPI peripheral signal via IO_MUX and allow for faster speeds
*/
#define PV_CONFIG_PIN_MISO 19U
#define PV_CONFIG_PIN_MOSI 23U
#define PV_CONFIG_PIN_SCLK 18U
#define PV_CONFIG_PIN_CS 5U

extern const spi_bus_config_t pv_config_spi2_bus_cfg;
