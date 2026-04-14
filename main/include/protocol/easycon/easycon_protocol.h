#ifndef EASYCON_PROTOCOL_H
#define EASYCON_PROTOCOL_H

#include "protocol/protocol.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* EasyCon command codes */
#define EASYCON_CMD_READY               0xA5
#define EASYCON_CMD_DEBUG               0x80
#define EASYCON_CMD_HELLO               0x81
#define EASYCON_CMD_FLASH               0x82
#define EASYCON_CMD_SCRIPT_START        0x83
#define EASYCON_CMD_SCRIPT_STOP         0x84
#define EASYCON_CMD_VERSION             0x85
#define EASYCON_CMD_LED                 0x86
#define EASYCON_CMD_UNPAIR              0x87
#define EASYCON_CMD_CHANGE_CONTROLLER_MODE      0x88
#define EASYCON_CMD_CHANGE_CONTROLLER_COLOR     0x89
#define EASYCON_CMD_SAVE_AMIIBO                 0x90
#define EASYCON_CMD_CHANGE_AMIIBO_INDEX         0x91

/* EasyCon response codes */
#define EASYCON_RPY_ERROR               0x00
#define EASYCON_RPY_BUSY                0xFE
#define EASYCON_RPY_ACK                 0xFF
#define EASYCON_RPY_HELLO               0x80
#define EASYCON_RPY_FLASH_START         0x81
#define EASYCON_RPY_FLASH_END           0x82
#define EASYCON_RPY_SCRIPT_ACK          0x83

/* EasyCon frame sizes */
#define EASYCON_PROTOCOL_ENCODED_SIZE     8
#define EASYCON_PROTOCOL_RAW_SIZE         7
#define EASYCON_PROTOCOL_HELLO_SIZE       3
#define EASYCON_PROTOCOL_SHORT_CMD_SIZE   2
#define EASYCON_PROTOCOL_SIMPLE_CMD_SIZE  3
#define EASYCON_PROTOCOL_SLICE_CMD_SIZE   6
#define EASYCON_PROTOCOL_END_MARKER       0x80
#define EASYCON_PROTOCOL_SLICE_MAX_SIZE   20

/* HAT direction values */
#define HAT_CENTER        0x08
#define HAT_UP            0x00
#define HAT_UP_RIGHT      0x01
#define HAT_RIGHT         0x02
#define HAT_DOWN_RIGHT    0x03
#define HAT_DOWN          0x04
#define HAT_DOWN_LEFT     0x05
#define HAT_LEFT          0x06
#define HAT_UP_LEFT       0x07

/**
 * @brief Slice parser state — carries pending slice info between frames.
 *
 * Stateless parsers (hello, short cmd, simple cmd, HID) do not need a state
 * struct and may pass NULL as their state pointer.
 */
typedef struct {
    uint8_t pending_code;
    uint16_t pending_index;
    uint16_t pending_len;
} easycon_slice_state_t;

/* Parser ops tables */
extern const protocol_parser_ops_t easycon_parser_hello_ops;
extern const protocol_parser_ops_t easycon_parser_short_cmd_ops;
extern const protocol_parser_ops_t easycon_parser_simple_cmd_ops;
extern const protocol_parser_ops_t easycon_parser_slice_ops;
extern const protocol_parser_ops_t easycon_parser_hid_ops;

#ifdef __cplusplus
}
#endif

#endif // EASYCON_PROTOCOL_H
