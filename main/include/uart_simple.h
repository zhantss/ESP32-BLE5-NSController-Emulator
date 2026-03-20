#ifndef _UART_SIMPLE_H_
#define _UART_SIMPLE_H_

#include "uart_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// Simple protocol constants
#define SIMPLE_PROTOCOL_START_BYTE     0xAA
#define SIMPLE_PROTOCOL_TYPE_BUTTON    0x01
#define SIMPLE_PROTOCOL_TYPE_STICK     0x02
#define SIMPLE_PROTOCOL_MAX_FRAME_SIZE 16

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
bool simple_protocol_detect(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // _UART_SIMPLE_H_