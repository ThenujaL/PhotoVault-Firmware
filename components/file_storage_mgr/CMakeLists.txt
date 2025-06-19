SET(SOURCES
    src/pv_sdc.c
    src/bdev_tests.c
)

SET(INCLUDE_DIRS
    "include"
)


idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    PRIV_REQUIRES common esp_driver_sdspi sdmmc unity 
)
