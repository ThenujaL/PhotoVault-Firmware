#include "pv_sdc.h"
#include "pv_fs.h"

#define TEST_DIR            SD_CARD_BASE_PATH "/startup_tests"

/* FUNCTION DEFS */
void test_sdcWriteFile(void);
void test_log_writes(void);
void test_log_checks(void);