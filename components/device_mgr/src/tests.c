#include "unity.h"

#include "pv_devicelist.h"


#define TAG "PV_DEVICELIST_TESTS"

void pv_test_devicelist(void){
    UNITY_BEGIN();
    RUN_TEST(test_devicelist_functions);
    UNITY_END();  
}


void test_devicelist_functions(void) {
    /* Test if device list created */
    TEST_ASSERT_EQUAL(ESP_OK, pv_device_list_create());

    /* Read file and see if headers were created */
    FILE *fp = fopen(DEVICE_LIST_PATH, "r");
    TEST_ASSERT_NOT_NULL(fp);
    char line[256];
    fgets(line, sizeof(line), fp);
    TEST_ASSERT_EQUAL_STRING("device_id,device_name\n", line);
    fclose(fp);

    /* Test adding a device */
    int id;
    TEST_ASSERT_EQUAL(ESP_OK, pv_device_list_add_device("Test Device 1", &id));
    TEST_ASSERT_EQUAL(0, id);
    TEST_ASSERT_EQUAL(ESP_OK, pv_device_list_add_device("Test Device 2", &id));
    TEST_ASSERT_EQUAL(1, id);

    /* Test if devices are correctly added */
    TEST_ASSERT_TRUE(pv_device_list_name_exists("Test Device 1"));
    TEST_ASSERT_TRUE(pv_device_list_name_exists("Test Device 2"));
    TEST_ASSERT_FALSE(pv_device_list_name_exists("Nonexistent Device"));

    /* Test updating device name */
    TEST_ASSERT_EQUAL(ESP_OK, pv_device_list_update_name(0, "Updated Test Device 1"));
    TEST_ASSERT_FALSE(pv_device_list_name_exists("Test Device 1"));
    TEST_ASSERT_TRUE(pv_device_list_name_exists("Updated Test Device 1"));

    /* Test deleting device */
    TEST_ASSERT_EQUAL(ESP_OK, pv_device_list_delete_device(1));
    TEST_ASSERT_FALSE(pv_device_list_name_exists("Test Device 2"));
}