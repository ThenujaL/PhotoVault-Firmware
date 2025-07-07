# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

SET(SOURCES
    src/transfer_control.c
    src/transfer_control_tests.c
)

SET(INCLUDE_DIRS
    "include"
)


idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    PRIV_REQUIRES common esp_driver_sdspi sdmmc unity esp_ringbuf
)

