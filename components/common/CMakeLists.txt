SET(SOURCES

)

SET(INCLUDE_DIRS
    "include"
)


idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    PRIV_REQUIRES esp_driver_spi
    )
