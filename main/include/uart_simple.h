#ifndef _UART_SIMPLE_H_
#define _UART_SIMPLE_H_

#include "uart_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// protocol frame types
#define SIMPLE_PROTOCOL_TYPE_HID            0x01   // 0x01: Full HID data (buttons + sticks)
#define SIMPLE_PROTOCOL_TYPE_MANAGEMENT     0x02   // 0x02: Management operation

// protocol frame header/footer
#define SIMPLE_PROTOCOL_NEW_START_BYTE1     0xAA
#define SIMPLE_PROTOCOL_NEW_START_BYTE2     0x55
#define SIMPLE_PROTOCOL_NEW_END_BYTE1       0x55
#define SIMPLE_PROTOCOL_NEW_END_BYTE2       0xAA

// Management command codes (1 byte, same code for request and response)
#define MGMT_CMD_HANDSHAKE        0x01  // Handshake
#define MGMT_CMD_HEARTBEAT        0x02  // Heartbeat
#define MGMT_CMD_REBOOT           0x03  // Reboot command
#define MGMT_CMD_VERSION_QUERY    0x04  // Protocol version query
#define MGMT_CMD_ERROR_REPORT     0x05  // Error report

// Frame sizes for new protocol
#define SIMPLE_FRAME_SIZE_HID         16  // 2+1+9+2+2 = 16 bytes
#define SIMPLE_FRAME_SIZE_MANAGEMENT  8   // 2+1+1+2+2 = 8 bytes

// Simple protocol implementation
extern const uart_protocol_impl_t simple_protocol_impl;

#ifdef __cplusplus
}
#endif

#endif // _UART_SIMPLE_H_