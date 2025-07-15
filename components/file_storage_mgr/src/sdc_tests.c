#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "unity.h"
#include "sdc_tests.h"
#include "pv_sdc.h"


/***************************************************************************
 * Function:    test_sdcWriteFile
 * Purpose:     Writes a test file to the SD card, reads it back, and checks the content.
 * Parameters:  None
 * Returns:     None
 ***************************************************************************/
void test_sdcWriteFile(void){
    const char *test_file_path = TEST_DIR "/test_sdcWriteFile.txt";
    const char *test_data = "This is a test data for SD card write operation.";
    char readBuff[strlen(test_data) + 1]; // +1 for null terminator
    FILE *f = NULL;
    struct stat st = {0};

    // Check if the test directory exists, if not create it
    if (stat(TEST_DIR, &st) != 0) {
        mkdir(TEST_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
        if (stat(TEST_DIR, &st) != 0) {
            TEST_FAIL_MESSAGE("Failed to create test directory");
            return;
        }
    }

    f = fopen(test_file_path, "w");
    TEST_ASSERT_NOT_NULL(f); // Check if file opened successfully

    // Write test data to the file
    fprintf(f, "%s", test_data);
    fclose(f);

    // Open the file for reading
    f = fopen(test_file_path, "r");
    TEST_ASSERT_NOT_NULL(f); // Check if file opened successfully

    // Read the content of the file
    fgets(readBuff, sizeof(readBuff), f);
    fclose(f);

    // Check if the read content matches the written content
    TEST_ASSERT_EQUAL_STRING(test_data, readBuff);
}