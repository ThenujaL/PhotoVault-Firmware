# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

SET(SOURCES
    src/pv_auth.c
    src/pv_devicelist.c
    src/tests.c
)

SET(INCLUDE_DIRS
    "include"
)


idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES common file_storage_mgr bt bt_arbiter
)

