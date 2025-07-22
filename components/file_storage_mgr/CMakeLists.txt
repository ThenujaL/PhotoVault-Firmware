SET(SOURCES
    src/pv_sdc.c
    src/pv_fs.c
    src/sdc_tests.c
    src/pv_backup_log.c
)

SET(INCLUDE_DIRS
    "include"
)


idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    PRIV_REQUIRES common esp_driver_sdspi sdmmc fatfs unity 
)
