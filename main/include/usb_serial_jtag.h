#ifndef _USB_SERIAL_JTAG_H_
#define _USB_SERIAL_JTAG_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize USB Serial/JTAG driver
 * @return 0 on success, negative on failure
 */
int dev_usb_serial_jtag_init(void);

/**
 * @brief Deinitialize USB Serial/JTAG driver
 */
void dev_usb_serial_jtag_deinit(void);

/**
 * @brief Start USB Serial/JTAG RX task
 * @return 0 on success, negative on failure
 */
int dev_usb_serial_jtag_start_task(void);

/**
 * @brief Stop USB Serial/JTAG RX task
 */
void dev_usb_serial_jtag_stop_task(void);

/**
 * @brief Send data via USB Serial/JTAG
 * @param data Data buffer to send
 * @param len Length of data
 * @return Number of bytes sent, negative on error
 */
int dev_usb_serial_jtag_send_data(const uint8_t* data, size_t len);

/**
 * @brief Check if USB Serial/JTAG is connected to host
 * @return true if connected, false otherwise
 */
bool dev_usb_serial_jtag_is_connected(void);

/**
 * @brief Read data from USB Serial/JTAG
 * @param buf Buffer to store received data
 * @param len Maximum number of bytes to read
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes actually read, negative on error
 */
int dev_usb_serial_jtag_read_bytes(uint8_t* buf, size_t len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // _USB_SERIAL_JTAG_H_
