#ifndef _UART_EASYCON_H_
#define _UART_EASYCON_H_

#include "uart_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// EasyCon protocol constants
#define EASYCON_PROTOCOL_ENCODED_SIZE   8   // Encoded frame size (7-bit packed)
#define EASYCON_PROTOCOL_RAW_SIZE       7   // Raw data size before encoding
#define EASYCON_PROTOCOL_END_MARKER     0x80 // Last byte bit7=1 as end marker

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

// EasyCon protocol implementation
extern const uart_protocol_impl_t easycon_protocol_impl;

/**
 * @brief Decode 7-bit packed data to raw bytes
 * @param encoded Encoded 7-bit data (8 bytes)
 * @param decoded Output decoded raw data (7 bytes)
 * @param decoded_len Length of decoded data (should be 7)
 */
void easycon_decode_7bit_packed(const uint8_t* encoded, uint8_t* decoded, size_t decoded_len);

/**
 * @brief Encode raw bytes to 7-bit packed data
 * @param decoded Raw data to encode (7 bytes)
 * @param encoded Output encoded data (8 bytes)
 * @param decoded_len Length of data to encode (should be 7)
 */
void easycon_encode_7bit_packed(const uint8_t* decoded, uint8_t* encoded, size_t decoded_len);

/**
 * @brief Scale EasyCon stick value (0-255) to HID 12-bit value (0-4095)
 * @param easycon_value EasyCon stick value (0-255, center=128)
 * @return Scaled HID value (0-4095, center=2048)
 */
uint16_t easycon_scale_stick_value(uint8_t easycon_value);

/**
 * @brief Get expected frame size for EasyCon protocol
 * @param data Frame data (at least start of frame)
 * @param len Available data length
 * @return Expected frame size (always 8 for EasyCon), or 0 if cannot determine
 */
size_t easycon_protocol_get_expected_frame_size(const uint8_t* data, size_t len);

/**
 * @brief Parse EasyCon protocol frame
 * @param data Frame data (8 bytes encoded)
 * @param len Frame length
 * @param event Output event
 * @return Event type if valid frame, UART_EVENT_UNKNOWN otherwise
 */
dev_uart_event_type_t easycon_protocol_parse_frame(const uint8_t* data, size_t len, dev_uart_event_t* event);

/**
 * @brief Detect if data matches EasyCon protocol
 * @param data Data to check
 * @param len Data length
 * @return true if data matches EasyCon protocol
 */
size_t easycon_protocol_detect(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // _UART_EASYCON_H_