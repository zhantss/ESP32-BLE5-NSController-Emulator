#ifndef _UART_SIMPLE_H_
#define _UART_SIMPLE_H_

#include "uart_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIMPLE_PROTOCOL_MAX_FRAME_SIZE 32    // Increased for new protocol frames

// New protocol frame types (replaces old protocol)
#define SIMPLE_PROTOCOL_TYPE_FULL_HID       0x01   // 0x01: Full HID data (buttons + sticks)
#define SIMPLE_PROTOCOL_TYPE_MANAGEMENT     0x02   // 0x02: Management operation
#define SIMPLE_PROTOCOL_TYPE_SENSOR         0x03   // Sensor data (at least 28bit)

// New protocol frame header/footer
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
#define SIMPLE_FRAME_SIZE_FULL_HID    16  // 2+1+9+2+2 = 16 bytes
#define SIMPLE_FRAME_SIZE_MANAGEMENT  8   // 2+1+1+2+2 = 8 bytes
#define SIMPLE_FRAME_SIZE_SENSOR      12  // 2+1+5+2+2 = 12 bytes

// Simple protocol implementation
extern const uart_protocol_impl_t simple_protocol_impl;

/**
 * @brief Calculate XOR checksum for simple protocol
 * @param data Data to calculate checksum for
 * @param len Length of data
 * @return XOR checksum
 */
uint8_t simple_protocol_calculate_checksum(const uint8_t* data, size_t len);

/**
 * @brief Get expected frame size for simple protocol
 * @param data Frame data (at least start byte and type)
 * @param len Available data length
 * @return Expected frame size, or 0 if cannot determine
 */
size_t simple_protocol_get_expected_frame_size(const uint8_t* data, size_t len);

/**
 * @brief Parse simple protocol frame
 * @param data Frame data
 * @param len Frame length
 * @param event Output event
 * @return Event type if valid frame, UART_EVENT_UNKNOWN otherwise
 */
dev_uart_event_type_t simple_protocol_parse_frame(const uint8_t* data, size_t len, dev_uart_event_t* event);

/**
 * @brief Detect if data matches simple protocol
 * @param data Data to check
 * @param len Data length
 * @return true if data matches simple protocol
 */
size_t simple_protocol_detect(const uint8_t* data, size_t len);

/**
 * @brief Calculate CRC-16-CCITT for new protocol frames
 * @param data Data to calculate CRC for
 * @param len Length of data
 * @return CRC-16 value
 */
uint16_t simple_protocol_calculate_crc16(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // _UART_SIMPLE_H_