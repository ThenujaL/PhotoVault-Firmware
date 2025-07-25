#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "unity.h"
#include "sdc_tests.h"
#include "pv_sdc.h"
#include "pv_fs.h"
#include "pv_logging.h"

#define TAG "PV_SDC_TESTS"


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


/***************************************************************************
 * Function:    test_log_writes
 * Purpose:     Tests the log writing functionality by writing a log entry
 *              and checking if it is correctly written to the log file.
 * Parameters:  None
 * Returns:     None
 ***************************************************************************/
void test_log_writes(void) {
    char *file_path = "/path/to/test_file.txt";
    char readline[300];
    int log_file_path_name_length = DEVICE_DIRECTORY_NAME_MAX_LENGTH + 1 + sizeof(LOG_FILE_NAME); // +1 for slash, sizeof includes null terminator
    char log_file_path[log_file_path_name_length];
    char log_dir[DEVICE_DIRECTORY_NAME_MAX_LENGTH];
    char log_entry[LOG_ENTRY_MAX_LENGTH] = {0};

    PV_LOGD(TAG, "Testing log writing functionality");

    // Clear the log file directory if it exists
    snprintf(log_dir, sizeof(log_dir), "%s/%s", SD_CARD_BASE_PATH, TEST_SERIAL_NUMBER);
    pv_delete_dir(log_dir);

    // Call the function to update the backup log
    TEST_ASSERT_EQUAL(ESP_OK, pv_backup_log_append(TEST_SERIAL_NUMBER, file_path));
    PV_LOGD(TAG, "Log entry written successfully");


    // Check if the log file was created and contains the expected data
    snprintf(log_file_path, sizeof(log_file_path), "%s/%s/%s", SD_CARD_BASE_PATH, TEST_SERIAL_NUMBER, LOG_FILE_NAME);
    FILE *log_file = fopen(log_file_path, "r");
    TEST_ASSERT_NOT_NULL(log_file); // Check if log file opened successfully
    PV_LOGD(TAG, "Log file opened successfully: %s", log_file_path);

    // Construct the expected log entry
    snprintf(log_entry, LOG_ENTRY_MAX_LENGTH, "\"%s\"\n", file_path);

    // Read the first line of the log file
    fgets(readline, sizeof(readline), log_file);
    fclose(log_file);

    // Check if the log file contains the expected file path
    TEST_ASSERT_EQUAL_STRING(log_entry, readline);
    PV_LOGD(TAG, "Log entry verified successfully: %s", readline);
}

/***************************************************************************
 * Function:    test_log_checks
 * Purpose:     Tests the log checking functionality by writing a log entry,
 *              and then checking if the entry is correctly identified as backed up.
 *              Check false path also.
 * Parameters:  None
 * Returns:     None
 ***************************************************************************/
void test_log_checks(void) {
    char log_file_path[LOG_FILE_PATH_NAME_LENGTH];
    char log_dir[DEVICE_DIRECTORY_NAME_MAX_LENGTH];
    char *file_path1_v = "/path/to/test_file1_v.txt"; // valid file path
    char *file_path2_m = "/path/to/test_file2_m.txt"; // missing file path
    char *file_path3_d = "/path/to/test_file3_d.txt"; // deleted file path
    struct stat st = {0};

    
    // Clear the log file directory if it exists
    snprintf(log_dir, sizeof(log_dir), "%s/%s", SD_CARD_BASE_PATH, TEST_SERIAL_NUMBER);
    pv_delete_dir(log_dir);

    // Construct full log file path
    snprintf(log_dir, sizeof(log_dir), "%s/%s", SD_CARD_BASE_PATH, TEST_SERIAL_NUMBER);
    snprintf(log_file_path, LOG_FILE_PATH_NAME_LENGTH, "%s/%s", log_dir, LOG_FILE_NAME);

    // Update the backup log with valid file paths
    TEST_ASSERT_EQUAL(ESP_OK, pv_backup_log_append(TEST_SERIAL_NUMBER, file_path1_v));

    // Check if the valid file paths are recognized as backed up
    TEST_ASSERT_TRUE(pv_is_backedUp(TEST_SERIAL_NUMBER, file_path1_v));

    // Check if a missing file path is recognized as not backed up
    TEST_ASSERT_FALSE(pv_is_backedUp(TEST_SERIAL_NUMBER, file_path2_m));

    // Delete test
    size_t file_size_before_delete_test = 0;
    size_t file_size_after_delete_test = 0;
    // Check if the log file exists before deletion
    TEST_ASSERT_EQUAL(ESP_OK, stat(log_file_path, &st));
    file_size_before_delete_test = st.st_size;

    TEST_ASSERT_EQUAL(ESP_OK, pv_backup_log_append(TEST_SERIAL_NUMBER, file_path3_d));
    TEST_ASSERT_TRUE(pv_is_backedUp(TEST_SERIAL_NUMBER, file_path3_d));
    TEST_ASSERT_EQUAL(ESP_OK, pv_delete_log_entry(TEST_SERIAL_NUMBER, file_path3_d));
    TEST_ASSERT_FALSE(pv_is_backedUp(TEST_SERIAL_NUMBER, file_path3_d));

    TEST_ASSERT_EQUAL(ESP_OK, stat(log_file_path, &st));
    file_size_after_delete_test = st.st_size;
    TEST_ASSERT_EQUAL(file_size_before_delete_test, file_size_after_delete_test); // File size should remain the same after deletion

}


