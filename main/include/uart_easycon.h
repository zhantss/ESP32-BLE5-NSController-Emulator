#ifndef _UART_EASYCON_H_
#define _UART_EASYCON_H_

#include "uart_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// EasyCon command code
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

// EasyCon command response code
#define EASYCON_RPY_ERROR               0x0
#define EASYCON_RPY_BUSY                0xFE
#define EASYCON_RPY_ACK                 0xFF
#define EASYCON_RPY_HELLO               0x80
#define EASYCON_RPY_FLASH_START         0x81
#define EASYCON_RPY_FLASH_END           0x82
#define EASYCON_RPY_SCRIPT_ACK          0x83

// EasyCon protocol constants
#define EASYCON_PROTOCOL_ENCODED_SIZE     8     // Encoded frame size (7-bit packed)
#define EASYCON_PROTOCOL_RAW_SIZE         7     // Raw data size before encoding
#define EASYCON_PROTOCOL_HELLO_SIZE       3     // hello and heartbeat frame size
#define EASYCON_PROTOCOL_SHORT_CMD_SIZE   2     // Short command frame size
#define EASYCON_PROTOCOL_SIMPLE_CMD_SIZE  3     // Simple command frame size
#define EASYCON_PROTOCOL_SLICE_CMD_SIZE   6     // Slice command frame size
#define EASYCON_PROTOCOL_END_MARKER       0x80  // Last byte bit7=1 as end marker
#define EASYCON_PROTOCOL_SLICE_MAX_SIZE   20    // Max slice data size

// HAT direction values
#define HAT_CENTER        0x08
#define HAT_UP            0x00
#define HAT_UP_RIGHT      0x01
#define HAT_RIGHT         0x02
#define HAT_DOWN_RIGHT    0x03
#define HAT_DOWN          0x04
#define HAT_DOWN_LEFT     0x05
#define HAT_LEFT          0x06
#define HAT_UP_LEFT       0x07

typedef enum {
  EC_IDLE,
  EC_HANDSHAKE,
  EC_HID,
  EC_ERROR
} easycon_protocol_state_t;

extern easycon_protocol_state_t ec_state;

// EasyCon protocol implementation
extern const uart_protocol_impl_t easycon_protocol_impl;

extern ec_cmd_slice_event_t ec_current_slice_event;

#ifdef __cplusplus
}
#endif

#endif // _UART_EASYCON_H_