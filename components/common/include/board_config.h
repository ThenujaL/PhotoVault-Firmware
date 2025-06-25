#pragma once

#include "driver/sdspi_host.h"


/* SPI Config */

/* 
    Note: Any GPIO pin can be used, but these are dedicated IO_MUX pins that are hardwired 
    to SPI peripheral signal via IO_MUX and allow for faster speeds
*/
#define PV_CONFIG_PIN_MISO 12U
#define PV_CONFIG_PIN_MOSI 13U
#define PV_CONFIG_PIN_SCLK 14U
#define PV_CONFIG_PIN_CS 15U

extern const spi_bus_config_t pv_config_spi2_bus_cfg;
