#ifndef _UART_H_
#define _UART_H_

#include <stdint.h>
#include <stdbool.h>

#include "uart_common.h"
#include "uart_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UART driver and create UART task
 * @return 0 on success, negative error code on failure
 */
int dev_uart_init(void);

/**
 * @brief Deinitialize UART driver and free resources
 */
void dev_uart_deinit(void);

/**
 * @brief Start UART task for receiving data
 * @return 0 on success, negative error code on failure
 */
int dev_uart_start_task(void);

/**
 * @brief Stop UART task
 */
void dev_uart_stop_task(void);

/**
 * @brief Get an event from UART queue (non-blocking)
 * @param event Output event if available
 * @return true if event was retrieved, false if queue is empty
 */
bool dev_uart_get_event(dev_uart_event_t* event);

/**
 * @brief Process events from UART queue and update HID report
 * This function should be called periodically from the HID task
 */
void dev_uart_process_events(void);

/**
 * @brief Send data via UART (for debugging/response)
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent, negative on error
 */
int dev_uart_send_data(const uint8_t* data, size_t len);

/**
 * @brief Set UART protocol
 * @param protocol Protocol to use
 * @return 0 on success, negative error code on failure
 */
int dev_uart_set_protocol(uart_protocol_t protocol);

/**
 * @brief Get current UART protocol
 * @return Current protocol type
 */
uart_protocol_t dev_uart_get_protocol(void);

/**
 * @brief Enable/disable protocol debug logging
 * @param enable true to enable, false to disable
 */
void dev_uart_set_debug_logging(bool enable);

#ifdef __cplusplus
}
#endif

#endif // _UART_H_