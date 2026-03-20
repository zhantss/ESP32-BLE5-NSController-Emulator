#ifndef _UART_PROTOCOL_H_
#define _UART_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "uart_common.h"

#ifdef __cplusplus
extern "C" {
#endif


// Protocol implementations (defined in respective .c files)
extern const uart_protocol_impl_t simple_protocol_impl;
extern const uart_protocol_impl_t easycon_protocol_impl;

// Protocol management functions

/**
 * @brief Get protocol implementation by type
 * @param protocol Protocol type
 * @return Pointer to protocol implementation, or NULL if not found
 */
const uart_protocol_impl_t* uart_protocol_get_impl(uart_protocol_t protocol);

/**
 * @brief Get current protocol from configuration
 * @return Configured protocol type
 */
uart_protocol_t uart_protocol_get_configured(void);

/**
 * @brief Auto-detect protocol from data
 * @param data Raw UART data
 * @param len Length of data
 * @return Detected protocol type, or UART_PROTOCOL_SIMPLE if uncertain
 */
uart_protocol_t uart_protocol_auto_detect(const uint8_t* data, size_t len);

/**
 * @brief Get protocol statistics
 * @param stats Output statistics structure
 */
void uart_protocol_get_stats(uart_protocol_stats_t* stats);

/**
 * @brief Reset protocol statistics
 */
void uart_protocol_reset_stats(void);

// Debug utilities

/**
 * @brief Hex dump utility for debugging
 * @param tag Log tag
 * @param data Data to dump
 * @param len Length of data
 */
void uart_protocol_hex_dump(const char* tag, const uint8_t* data, size_t len);

/**
 * @brief Get protocol name string
 * @param protocol Protocol type
 * @return Protocol name string
 */
const char* uart_protocol_get_name(uart_protocol_t protocol);

/**
 * @brief Parse frame using current protocol
 */
dev_uart_event_type_t uart_protocol_parse_frame(const uart_protocol_impl_t* impl,
                                                const uint8_t* data, size_t len,
                                                dev_uart_event_t* event);
/**
 * @brief Initialize all protocols
 */
void uart_protocol_init_all(void);

/**
 * @brief Deinitialize all protocols
 */
void uart_protocol_deinit_all(void);

void uart_protocol_update_stats_frame_received(void);
void uart_protocol_update_stats_frame_parsed(void);
void uart_protocol_update_stats_parse_error(void);
void uart_protocol_update_stats_checksum_error(void);
void uart_protocol_update_stats_protocol_switch(void);
void uart_protocol_set_debug(bool enable);

#ifdef __cplusplus
}
#endif

#endif // _UART_PROTOCOL_H_