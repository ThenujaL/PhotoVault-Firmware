#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_bt_defs.h"

#include "pv_auth.h"
#include "pv_bt_utils.h"
#include "pv_devicelist.h"
#include "pv_logging.h"
#include "pv_bt_commands.h"


#define TAG "PV_AUTH"

typedef struct {
    uint32_t handle;
    esp_bd_addr_t bd_addr;
    bool authenticated;
    pv_android_device_id_t android_id;
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

static pv_connections_t device_connections = {
    .head = NULL,
    .connection_count = 0,
    .oldest_index = 0
};


/**
 * @brief Initializes the PIN by checking if the PIN file exists and creating it with a default PIN if it does not.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t pv_pin_init(void){
    /* Check if PIN file exists, if not create it with default pin 0000 */
    struct stat st;
    if (stat(PV_PIN_PATH, &st) != 0) {
        PV_LOGW(TAG, "PIN file does not exist at %s, creating new file with default PIN", PV_PIN_PATH);
        pv_pin_t default_pin = {0}; // Default PIN is 0000 in bytes
        return pv_set_pin(default_pin);
    }

    PV_LOGI(TAG, "PIN file already exists at %s", PV_PIN_PATH);
    return ESP_OK;
}


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
    size_t bytes_read = fread(stored_pin, 1, PV_PIN_LENGTH, file);
    fclose(file);

    if (bytes_read != PV_PIN_LENGTH) {
        PV_LOGE(TAG, "Failed to read complete PIN from file (read %zu bytes)", bytes_read);
        return false;
    }

    /* Constant-time comparison to prevent timing attacks */
    bool match = (memcmp(pin, stored_pin, PV_PIN_LENGTH) == 0);
    
    if (match) {
        PV_LOGI(TAG, "PIN matched successfully");
    } else {
        PV_LOGW(TAG, "PIN mismatch");
    }

    /* Clear sensitive data from stack */
    memset(stored_pin, 0, PV_PIN_LENGTH);

    return match;
}


/**
 * @brief Sets/stores the PIN to file
 * @param pin The PIN to store
 * @return ESP_OK on success, error code otherwise
 * @warning This function overwirtes the existing pin. Ensure proper authentication before calling.
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

    size_t bytes_written = fwrite(pin, 1, PV_PIN_LENGTH, file);
    
    /* Flush to ensure data is written to SD card */
    fflush(file);
    fclose(file);

    if (bytes_written != PV_PIN_LENGTH) {
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

    PV_LOGI(TAG, "Adding new device connection with handle %lu", handle);

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

    /* Check if device is already in the list */
    if(pv_device_list_id_exists(bd_addr)) {
        PV_LOGI(TAG, "Device with same BDA already exists in device list, marking as authenticated");
        pv_android_device_id_t android_id;
        char bda_str[BD_ADDR_STR_LENGTH];
        bda2str(bd_addr, bda_str, sizeof(bda_str));
        if (ESP_OK == pv_device_list_get_android_id_by_bda(bd_addr, &android_id)) {
            new_node->connection.android_id = android_id;
        } else {
            PV_LOGE(TAG, "Failed to get android_id for device with bda %s. Device will be added to connection list but not marked as authenticated.", bda_str);
        }

        new_node->connection.authenticated = true;
    }


    return ESP_OK;
}

/**
 * @brief Removes a node from the device connection list and frees its memory.
 * @param node The node to remove.
 */
static inline void remove_node(pv_device_connection_node_t *node) {

    if (node == NULL) {
        return;
    }

    /* Remove node from LL */
    if (node->prev) {
        node->prev->next = node->next;
    } 
    else {
        /* Removing head */
        device_connections.head = node->next;
    }

    free(node);
}


/**
 * @brief Removes a device connection from the list based on its android_id.
 * @param android_id The android_id of the device to remove.
 * @return None
 */
void pv_remove_connection_by_id(pv_android_device_id_t android_id) {

    pv_device_connection_node_t *curr_node = device_connections.head;

    /* Look for existing nodes with same android_id */
    while (curr_node) {
        if (curr_node->connection.android_id == android_id) {
            PV_LOGW(TAG, "Removing device connection with android_id %llu from connection list", android_id);
            remove_node(curr_node);
            return;
        }

        if (curr_node->next == NULL) {
            /* Device not in LL */
            PV_LOGW(TAG, "Device with android_id %llu not found in connection list. Nothing to remove.", android_id);
            return;
        }
        
        curr_node = curr_node->next; 
    }

}


/**
 * @brief Removes a device connection from the list based on its handle and BDA.
 * @param handle The connection handle.
 * @param bd_addr The Bluetooth device address.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t pv_remove_connection(uint32_t handle) {

    pv_device_connection_node_t *curr_node = device_connections.head;

    if (curr_node == NULL) {
        PV_LOGW(TAG, "No connected devices found in connection list to be removed for handle %lu", handle);
        return false;
    }

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

    if (curr_node == NULL) {
        PV_LOGW(TAG, "No connected devices found in connection list");
        return false;
    }

     /* Look for existing nodes with same handle */
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
        PV_LOGD(TAG, "Device with handle %lu is already authenticated (found from connection list)", handle);
        return true;
    }

    /* If device is in the device list */
    if (pv_device_list_id_exists(curr_node->connection.bd_addr)) {
        PV_LOGW(TAG, "Device with handle %lu is already authenticated (found from device list)", handle);
        curr_node->connection.authenticated = true;
        return true;
    }    

    char bda_str[BD_ADDR_STR_LENGTH];
    bda2str(curr_node->connection.bd_addr, bda_str, sizeof(bda_str));

    PV_LOGW(TAG, "Device with handle %lu (bd_addr: %s) and android_id %llu is not found in device list", handle, bda_str, curr_node->connection.android_id);

    return false;
}



/**
 * @brief Sets the authentication status of a device connection.
 * @param handle The connection handle.
 * @param authenticated The authentication status to set.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t pv_set_authenticated(uint32_t handle, pv_android_device_id_t android_id, bool authenticated){
    pv_device_connection_node_t *curr_node = device_connections.head;

    /* Look for existing nodes with same handle */
    while (curr_node) {
        if (curr_node->connection.handle == handle) {
            curr_node->connection.android_id = android_id;
            curr_node->connection.authenticated = authenticated;
            PV_LOGI(TAG, "Set device with handle %luandroid_id %llu authentication status to %d", handle, android_id, authenticated);
            return ESP_OK;
        }

        curr_node = curr_node->next; 
    }

    PV_LOGE(TAG, "Device with handle %lu not found in connection list", handle);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Gets the android_id associated with a connection handle.
 * @param device_handle The connection handle.
 * @param out_android_id Output parameter to store the retrieved android_id.
 */
esp_err_t pv_get_android_id_by_handle(uint32_t device_handle, pv_android_device_id_t *out_android_id) {

    pv_device_connection_node_t *curr_node = device_connections.head;

    /* Look for existing nodes with same android_id */
    while (curr_node) {
        if (curr_node->connection.handle == device_handle) {
            *out_android_id = curr_node->connection.android_id;
            PV_LOGI(TAG, "Found android_id %llu for device handle %lu", *out_android_id, device_handle);
            return ESP_OK;
        }

        if (curr_node->next == NULL) {
            /* Device not in LL */
            PV_LOGW(TAG, "Device with handle %lu not found in connection list. Nothing to remove.", device_handle);
            return ESP_ERR_NOT_FOUND;
        }
        
        curr_node = curr_node->next; 
    }

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


/**
 * @brief Handles the authentication command received from a device, validating the provided PIN and device information, updating the device list, and setting the authentication status accordingly.
 * @param data The data received from the device, expected to contain the authentication command, PIN
 * @param len The length of the received data.
 * @param handle The connection handle of the device sending the command.
 * @param bda The Bluetooth device address of the device sending the command.
 * @return PV_AUTH_SUCCESS if authentication is successful, PV_AUTH_ERR if authentication fails, PV_AUTH_SET_UP_REQUIRED if the first device is trying to authenticate without providing necessary info.
 */
pv_auth_err_t pv_auth_cmd_handler(uint8_t *data, uint16_t len, uint32_t handle) {
    
    esp_err_t err;

    /* Get bda from handle */
    esp_bd_addr_t bda;
    err = pv_get_bda_from_handle(handle, bda);
    if (err != ESP_OK) {
        PV_LOGE(TAG, "Failed to get BDA from handle %lu. Verify that BDA set for handle during ESP_SPP_SRV_OPEN_EVT", handle);
        return PV_AUTH_ERR;
    }

    /**
     * AUTH CMD format:
     * AUTHCMD/n<pin(4 bytes)><name_length(1 byte)><device_name(variable length, max 128 bytes including null terminator)>
     */    

    if(!cmd_compare((char *)AUTH_CMD, data, AUTH_CMD_LEN)) {
        PV_LOGE(TAG, "Received data from unauthorized device, rejecting");
        return PV_AUTH_ERR;
    }

    /* If simply checking auth status */
    if (len < AUTH_CMD_LEN + PV_PIN_LENGTH + 1) {
        /* If first device */
        if (pv_device_list_get_count() == 0) {
            /** 
            @note If this is the first device, entering here means the mobile app needs to be
            told that that this the first device and the user must go through the PIN setup
            UI 
            */
            return PV_AUTH_SET_UP_REQUIRED;
        }
        else {
            /* If not first device, entering here means an unauthorized device is trying to authenticate without providing necessary info, so reject */
            PV_LOGE(TAG, "Received AUTH command with no pin or device name, rejecting");
            return PV_AUTH_ERR;
        }
    }

    char device_name[PV_DEVICE_NAME_MAX_LENGTH] = {0};
    pv_pin_t pin = {0};
    uint8_t name_len = 0;
    pv_android_device_id_t android_id = {0};

    /* Get pin from command */
    memcpy(pin, data + AUTH_CMD_LEN, PV_PIN_LENGTH);

    /* Get name length */
    name_len = *(data + AUTH_CMD_LEN + PV_PIN_LENGTH);

    if (name_len > PV_DEVICE_NAME_MAX_LENGTH) {
        PV_LOGE(TAG, "Device name length %d exceeds maximum %d", name_len, PV_DEVICE_NAME_MAX_LENGTH - 1);
        name_len = PV_DEVICE_NAME_MAX_LENGTH - 1; // Truncate to max length
    }

    /* Check if all characters of name and device_id received received */
    if (len < AUTH_CMD_LEN + PV_PIN_LENGTH + 1 + name_len + sizeof(pv_android_device_id_t)) {
        PV_LOGE(TAG, "Received AUTH command with incomplete device name or android_id");
        return PV_AUTH_ERR;
    }

    /* Get device name from command */
    memcpy(device_name, (char *)(data + AUTH_CMD_LEN + PV_PIN_LENGTH + 1), name_len);

    /* Get android device ID from command */
    memcpy(&android_id, (char *)(data + AUTH_CMD_LEN + PV_PIN_LENGTH + 1 + name_len), sizeof(pv_android_device_id_t));

    /* If this is the first device being validated, add to devicelist, set the new pin, and mark as authorized */
    if (pv_device_list_get_count() == 0) {
        
        /* Add device to device list */
        err = pv_device_list_add_device(bda, android_id, device_name);
        if (err != ESP_OK) {
            PV_LOGE(TAG, "Failed to add device to device list");
            return PV_AUTH_ERR;
        }

        /* Set the new pin */
        err = pv_set_pin(pin);
        if (err != ESP_OK) {
            PV_LOGE(TAG, "Failed to set new pin");
            return PV_AUTH_ERR;
        }

        /* Mark device as authenticated */
        err = pv_set_authenticated(handle, android_id, true);
        if (err != ESP_OK) {
            PV_LOGE(TAG, "Failed to set device as authenticated");
            return PV_AUTH_ERR;
        }

        ESP_LOGI(TAG, "Device authenticated and added to device list");
    }
    else { /* This is a new phone but pin has already been configured, check pin, add it to the device list, and mark as authorized */
        
        /* Check if the pin is correct */
        if (pv_cmp_pin(pin)) {
            /* Add device to device list */
            err = pv_device_list_add_device(bda, android_id, device_name);
            if (err != ESP_OK) {
                PV_LOGE(TAG, "Failed to add device to device list");
                return PV_AUTH_ERR;
            }

            /* Mark device as authenticated */
            err = pv_set_authenticated(handle, android_id, true);
            if (err != ESP_OK) {
                PV_LOGE(TAG, "Failed to set device as authenticated");
                return PV_AUTH_ERR;
            }

            ESP_LOGI(TAG, "Device authenticated and added to device list");
        }
        else {
            PV_LOGE(TAG, "Incorrect pin received for authentication");
            return PV_AUTH_ERR;                    
        }
    }
    
    return PV_AUTH_SUCCESS;
}

