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

#ifdef __cplusplus
}
#endif

#endif // _UART_EASYCON_H_