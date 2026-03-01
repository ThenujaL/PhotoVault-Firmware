/* BT COMMANDS */
#define AUTH_CMD                "AUTH\n"                          // Authentication command
#define AUTH_CMD_LEN            (sizeof(AUTH_CMD) - 1)

#define AUTH_OK_MSG            "AUTHOK\n"                        // Authentication success message to client
#define AUTH_OK_MSG_LEN        (sizeof(AUTH_OK_MSG) - 1)

#define AUTH_ERR_MSG           "AUTHERR\n"                       // Authentication error message to client
#define AUTH_ERR_MSG_LEN       (sizeof(AUTH_ERR_MSG) - 1)

#define AUTH_SETUP_CMD           "AUTHSETUP\n"                     // Authentication setup command for first device to set PIN and device name
#define AUTH_SETUP_CMD_LEN       (sizeof(AUTH_SETUP_CMD) - 1)

#define RESET_CMD               "RESET\n"                         // Reset command
#define RESET_CMD_LEN           (sizeof(RESET_CMD) - 1)  

#define RX_START_CMD            "RXSTART\n"                       // Start receving file from client
#define RX_START_CMD_LEN        (sizeof(RX_START_CMD) - 1)

#define RX_STARTM_CMD           "RXSTARTM\n"                      // Start receiving metadata from client
#define RX_STARTM_CMD_LEN       (sizeof(RX_STARTM_CMD) - 1)

#define RX_GETFLIST_CMD         "RXGETFLIST\n"                    // Client requests file list
#define RX_GETFLIST_CMD_LEN     (sizeof(RX_GETFLIST_CMD) - 1)

#define RX_GETFILE_CMD          "RXGETFILE\n"                     // Client requests file
#define RX_GETFILE_CMD_LEN      (sizeof(RX_GETFILE_CMD) - 1)

#define RX_OK_MSG               "RXOK\n"                          // Success message TO client after RECEIVING file
#define RX_OK_MSG_LEN           (sizeof(RX_OK_MSG) - 1)

#define TX_OK_MSG               "TXOK\n"                          // Success message FROM client after SENDING file
#define TX_OK_MSG_LEN           (sizeof(TX_OK_MSG) - 1)

#define TX_ERR_MSG              "TXERRR\n"                        // Error message FROM client after SENDING file
#define TX_ERR_MSG_LEN          (sizeof(TX_ERR_MSG) - 1)

#define DEL_CMD                 "DEL\n"                           // Delete file command from client
#define DEL_CMD_LEN             (sizeof(DEL_CMD) - 1)

#define DELOK_MSG               "DELOK\n"                         // Delete file or device from devicelist success message to client
#define DELOK_MSG_LEN           (sizeof(DELOK_MSG) - 1)

#define DELERR_MSG              "DELERR\n"                        // Delete file error message to client
#define DELERR_MSG_LEN          (sizeof(DELERR_MSG) - 1)

#define RENAME_CMD              "RENAME\n"                        // Rename file command
#define RENAME_CMD_LEN          (sizeof(RENAME_CMD) - 1)

#define RENAMEOK_MSG            "RENAMEOK\n"                      // Rename file/device success message to client
#define RENAMEOK_CMD_LEN        (sizeof(RENAMEOK_MSG) - 1)

#define RENAMEERR_MSG           "RENAMEERR\n"                      // Rename file/device error message to client
#define RENAMEERR_CMD_LEN       (sizeof(RENAMEERR_MSG) - 1)

#define RX_ENDM_CMD             "ENDM\n"                          // End metadata transaction
#define RX_ENDM_CMD_LEN         (sizeof(RX_ENDM_CMD) - 1)

#define ACK                     "ACK"                             // ACK  - not used?
#define ACK_LEN                 (sizeof(ACK) - 1)

#define END_CMD                 "END\n"                           // End transaction 
#define END_CMD_LEN             (sizeof(END_CMD) - 1)

/* Device List Commands */
#define DEVLIST_DEL_CMD         "DEVLIST_DEL\n"                   // Delete device from device list command
#define DEVLIST_DEL_CMD_LEN     (sizeof(DEVLIST_DEL_CMD) - 1)

#define DEVLIST_MOD_CMD         "DEVLIST_MOD\n"                   // Modify device in device list command
#define DEVLIST_MOD_CMD_LEN     (sizeof(DEVLIST_MOD_CMD) - 1)