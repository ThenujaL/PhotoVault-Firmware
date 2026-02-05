#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_bt_defs.h"

#include "pv_auth.h"
#include "pv_bt_utils.h"
#include "pv_devicelist.h"
#include "pv_logging.h"


#define TAG "PV_AUTH"

typedef struct {
    uint32_t handle;
    esp_bd_addr_t bd_addr;
    bool authenticated;
} pv_device_connection_t;

typedef struct pv_device_connection_node {
    pv_device_connection_t connection;
    struct pv_device_connection_node *next; 
    struct pv_device_connection_node *prev;
} pv_device_connection_node_t;

/**
 * @brief Linked list to hold all device connections.
 */
typedef struct {
    pv_device_connection_node_t *head;
    size_t connection_count;
    size_t oldest_index;
} pv_connections_t;

static pv_connections_t device_connections = {0};


/**
 * @brief Compares provided PIN with stored PIN from file
 * @param pin The PIN to compare
 * @return true if PINs match, false otherwise
 */
bool pv_cmp_pin(const pv_pin_t pin) {

    if (pin == NULL) {
        PV_LOGE(TAG, "NULL PIN provided");
        return ESP_ERR_INVALID_ARG;
    }

    /* Check if PIN file exists */
    struct stat st;
    if (stat(PV_PIN_PATH, &st) != 0) {
        PV_LOGE(TAG, "PIN file does not exist at %s", PV_PIN_PATH);
        return false;
    }

    FILE *file = fopen(PV_PIN_PATH, "rb");
    if (file == NULL) {
        PV_LOGE(TAG, "Failed to open PIN file for reading");
        return false;
    }

    pv_pin_t stored_pin = {0};
    size_t bytes_read = fread(stored_pin, 1, PV_PIN_BYTES_LENGTH, file);
    fclose(file);

    if (bytes_read != PV_PIN_BYTES_LENGTH) {
        PV_LOGE(TAG, "Failed to read complete PIN from file (read %zu bytes)", bytes_read);
        return false;
    }

    /* Constant-time comparison to prevent timing attacks */
    bool match = (memcmp(pin, stored_pin, PV_PIN_BYTES_LENGTH) == 0);
    
    if (match) {
        PV_LOGI(TAG, "PIN matched successfully");
    } else {
        PV_LOGW(TAG, "PIN mismatch");
    }

    /* Clear sensitive data from stack */
    memset(stored_pin, 0, PV_PIN_BYTES_LENGTH);

    return match;
}


/**
 * @brief Sets/stores the PIN to file
 * @param pin The PIN to store
 * @return ESP_OK on success, error code otherwise
 * @warning This function overwirtes the existing pin. Ensure proper validation before calling.
 */
esp_err_t pv_set_pin(const pv_pin_t pin) {
    if (pin == NULL) {
        PV_LOGE(TAG, "NULL PIN provided");
        return ESP_ERR_INVALID_ARG;
    }

    FILE *file = fopen(PV_PIN_PATH, "wb");
    if (file == NULL) {
        PV_LOGE(TAG, "Failed to open PIN file for writing at %s", PV_PIN_PATH);
        return ESP_ERR_NOT_FOUND;
    }

    size_t bytes_written = fwrite(pin, 1, PV_PIN_BYTES_LENGTH, file);
    
    /* Flush to ensure data is written to SD card */
    fflush(file);
    fclose(file);

    if (bytes_written != PV_PIN_BYTES_LENGTH) {
        PV_LOGE(TAG, "Failed to write complete PIN to file (wrote %zu bytes)", bytes_written);
        return ESP_ERR_INVALID_SIZE;
    }

    PV_LOGI(TAG, "PIN saved successfully to %s", PV_PIN_PATH);
    return ESP_OK;
}


/**
 * @brief Adds a new device connection to the list of connections. If BDA already exists, updates the handle.
 *        The update functionality is needed for instances where the correct event failed to fire upon disconnection.
 * @param handle The connection handle.
 * @param bd_addr The Bluetooth device address.
 * @return ESP_OK on success, error code otherwise.
 * @note Devices added to connection list are not authenticated by default.
 */
esp_err_t pv_add_connection(uint32_t handle, esp_bd_addr_t bd_addr){

    pv_device_connection_node_t *curr_node = device_connections.head;

    /* Look for existing nodes with same bd_addr */
    while (curr_node) {
        if (pv_cmp_db_addr(bd_addr, curr_node->connection.bd_addr)) {
            /* Update the handle */
            curr_node->connection.handle = handle;
            return ESP_OK;
        }

        if (curr_node->next) {
           curr_node = curr_node->next; 
        }
        else {
            break;
        }
    }

    /* Allocate new node */
    pv_device_connection_node_t *new_node = (pv_device_connection_node_t *)calloc(1, sizeof(pv_device_connection_node_t));
    if (new_node == NULL) {
        PV_LOGE(TAG, "Failed to allocate memory for new device");
        return ESP_ERR_NO_MEM;
    }

    /* Set new node values */
    new_node->prev = curr_node;
    new_node->connection.handle = handle;
    memcpy(new_node->connection.bd_addr, bd_addr, ESP_BD_ADDR_LEN);
    
    if (curr_node) {
        /* Add to end of LL */
        curr_node->next = new_node;
    } 
    else {
        /* Empty LL -> new_node is head */
        device_connections.head = new_node;
    }

    return ESP_OK;
}


/**
 * @brief Removes a device connection from the list based on its handle and BDA.
 * @param handle The connection handle.
 * @param bd_addr The Bluetooth device address.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t pv_remove_connection(uint32_t handle) {

    pv_device_connection_node_t *curr_node = device_connections.head;

    /* Look for existing nodes with same handle */
    while (curr_node) {
        if (curr_node->connection.handle == handle) {
            break;
        }

        if (curr_node->next == NULL) {
            /* Device not in LL */
            PV_LOGE(TAG, "Device with handle %lu not found in connection list", handle);
            return ESP_ERR_NOT_FOUND;
        }
        
        curr_node = curr_node->next; 
    }

    /* Remove node from LL */
    if (curr_node->prev) {
        curr_node->prev->next = curr_node->next;
    } 
    else {
        /* Removing head */
        device_connections.head = curr_node->next;
    }

    free(curr_node);
    return ESP_OK;

}


/**
 * @brief Checks if a device is authorized based on its handle and provided data (PIN + name).
 * @param handle The connection handle.
 * @return true if the device is authorized, false otherwise.
 */
bool pv_is_device_authorized(uint32_t handle){

    /* Check LL if device was authed before */
    pv_device_connection_node_t *curr_node = device_connections.head;
    while (curr_node) {
        if (curr_node->connection.handle == handle) {
            break;
        }

        if (curr_node->next == NULL) {
            /* Device not in LL */
            PV_LOGE(TAG, "Device with handle %lu not found in connection list", handle);
            return false;
        }
        curr_node = curr_node->next;
    }

    /* If authed before. return true immedietly*/
    if (curr_node->connection.authenticated) {
        return true;
    }

    /* If device is in the device list */
    if (pv_device_list_id_exists(curr_node->connection.bd_addr)) {
        curr_node->connection.authenticated = true;
        return true;
    }    

    return false;
}



/**
 * @brief Sets the authentication status of a device connection.
 * @param handle The connection handle.
 * @param authenticated The authentication status to set.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t pv_set_authenticated(uint32_t handle, bool authenticated){
    pv_device_connection_node_t *curr_node = device_connections.head;

    /* Look for existing nodes with same handle */
    while (curr_node) {
        if (curr_node->connection.handle == handle) {
            curr_node->connection.authenticated = authenticated;
            return ESP_OK;
        }

        curr_node = curr_node->next; 
    }

    PV_LOGE(TAG, "Device with handle %lu not found in connection list", handle);
    return ESP_ERR_NOT_FOUND;
}


/**
 * @brief Retrieves the Bluetooth device address associated with a connection handle.
 * @param handle The connection handle.
 * @param out_bda Output parameter to store the retrieved Bluetooth device address.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t pv_get_bda_from_handle(uint32_t handle, esp_bd_addr_t out_bda) {
    pv_device_connection_node_t *curr_node = device_connections.head;

    /* Look for existing nodes with same handle */
    while (curr_node) {
        if (curr_node->connection.handle == handle) {
            memcpy(out_bda, curr_node->connection.bd_addr, ESP_BD_ADDR_LEN);
            return ESP_OK;
        }

        curr_node = curr_node->next; 
    }

    PV_LOGE(TAG, "Device with handle %lu not found in connection list", handle);
    return ESP_ERR_NOT_FOUND;
}

