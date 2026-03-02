#include "unity.h"

#include "pv_devicelist.h"


#define TAG "PV_DEVICELIST_TESTS"

void test_devicelist_functions(void);

void pv_test_devicelist(void){
    // UNITY_BEGIN();
    // RUN_TEST(test_devicelist_functions);
    // UNITY_END();  
}


void test_devicelist_functions(void) {
    // /* Test if device list created */
    // TEST_ASSERT_EQUAL(ESP_OK, pv_device_list_create());

    // /* Read file and see if headers were created */
    // FILE *fp = fopen(DEVICE_LIST_PATH_INTERNAL, "r");
    // TEST_ASSERT_NOT_NULL(fp);
    // char line[256];
    // fgets(line, sizeof(line), fp);
    // TEST_ASSERT_EQUAL_STRING("bda,device_name\n", line);
    // fclose(fp);

    // /* Test adding a device */
    // const char* id_1 = "00:11:22:33:44:55";
    // const char* id_2 = "66:77:88:99:AA:BB";
    // TEST_ASSERT_EQUAL(ESP_OK, pv_device_list_add_name(id_1, "Test Device 1"));
    // TEST_ASSERT_EQUAL(ESP_OK, pv_device_list_add_name(id_2, "Test Device 2"));

    // /* Test if devices are correctly added */
    // TEST_ASSERT_TRUE(pv_device_list_id_exists(id_1));
    // TEST_ASSERT_TRUE(pv_device_list_id_exists(id_2));
    // TEST_ASSERT_FALSE(pv_device_list_id_exists("00:00:00:00:00:00"));

    // /* Test updating device name */
    // char name_buffer[sizeof("Updated Test Device 1")];
    // TEST_ASSERT_EQUAL(ESP_OK, pv_device_list_add_name(id_1, "Updated Test Device 1"));
    // TEST_ASSERT_EQUAL(ESP_OK, pv_device_list_get_name_by_id(id_1, name_buffer, sizeof(name_buffer)));
    // TEST_ASSERT_EQUAL_STRING("Updated Test Device 1", name_buffer);
    
    // /* Test deleting device */
    // TEST_ASSERT_EQUAL(ESP_OK, pv_device_list_delete_device(id_2));
    // TEST_ASSERT_FALSE(pv_device_list_id_exists(id_2));
}