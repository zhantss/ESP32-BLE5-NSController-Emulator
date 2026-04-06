#ifndef _UART_COMMON_H_
#define _UART_COMMON_H_

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "hid.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define BUTTON_MAP_SIZE (sizeof(button_map) / sizeof(button_map[0]))

// Transport layer selection
#if CONFIG_TRANSPORT_LAYER_USB_SERIAL_JTAG
    #define TRANSPORT_USE_USB_SERIAL_JTAG   1
#else
    #define TRANSPORT_USE_USB_SERIAL_JTAG   0
#endif

// UART Configuration (used when TRANSPORT_USE_USB_SERIAL_JTAG == 0)
#define UART_PORT_NUM           UART_NUM_1
#define UART_BAUD_RATE          115200
#define UART_RX_BUFFER_SIZE     1024
#define UART_TX_BUFFER_SIZE     1024
#define UART_RX_PIN             CONFIG_UART_RX_PIN
#define UART_TX_PIN             CONFIG_UART_TX_PIN
#define UART_MAX_FRAME_SIZE     32   // Maximum frame size for all protocols


// Full HID event - contains all button bytes and stick data
typedef struct {
    uint8_t button_bytes[3];    // 3 bytes button state (corresponding to pro2_btn_bits_t)
    uint16_t left_stick_x;      // Left stick X coordinate (12-bit, 0-0xFFF, unpacked)
    uint16_t left_stick_y;      // Left stick Y coordinate (12-bit, 0-0xFFF, unpacked)
    uint16_t right_stick_x;     // Right stick X coordinate (12-bit, 0-0xFFF, unpacked)
    uint16_t right_stick_y;     // Right stick Y coordinate (12-bit, 0-0xFFF, unpacked)
} simple_hid_event_t;

// Management event 
typedef struct {
    uint8_t command;            // Command code
} simple_management_event_t;

typedef struct {
    uint8_t code;
    uint8_t data;
} ec_cmd_event_t;

// EasyCon protocol event
typedef struct {
    uint16_t button_mask;    // Button bitmask (16 bits for non-direction, non-C buttons)
    uint8_t hat_state;       // HAT direction state (0x00-0x08)
    uint16_t left_stick_x;   // Left stick X coordinate (12-bit, 0-0xFFF)
    uint16_t left_stick_y;   // Left stick Y coordinate (12-bit, 0-0xFFF)
    uint16_t right_stick_x;  // Right stick X coordinate (12-bit, 0-0xFFF)
    uint16_t right_stick_y;  // Right stick Y coordinate (12-bit, 0-0xFFF)
} ec_hid_event_t;

// Union for event data
typedef union {
    // Simple Protocol
    simple_management_event_t management;       // Management operation
    simple_hid_event_t simple_hid;              // Simple HID event
    // EasyCon Protocol
    ec_cmd_event_t ec_cmd;                      // EasyCon command
    ec_hid_event_t ec_hid;                      // EasyCon HID Event
} dev_uart_event_data_t;

// UART event types
typedef enum {
    // Simple 
    UART_EVENT_SIMPLE_MANAGEMENT, // Management operation
    UART_EVENT_SIMPLE_HID,        // Simple HID data (buttons + sticks)
    // EasyCon
    UART_EVENT_EC_CMD,            // EasyCon command
    UART_EVENT_EC_HID,            // EasyCon HID data (buttons + sticks)
    // Unknown
    UART_EVENT_UNKNOWN
} dev_uart_event_type_t;

// Complete UART event with type
typedef struct {
    dev_uart_event_type_t type; // Event type
    dev_uart_event_data_t data; // Event data
} dev_uart_event_t;

typedef struct {
    uint8_t* data;
    size_t len;
} dev_uart_event_rsp_t;

/**
 * @brief UART protocol types
 */
typedef enum {
    UART_PROTOCOL_SIMPLE = 0,     ///< Simple protocol with XOR checksum
    UART_PROTOCOL_EASYCON = 1,    ///< EasyCon protocol for advanced controllers
} uart_protocol_t;

/**
 * @brief Protocol interface structure
 *
 * Each protocol implementation must provide these functions
 */
typedef struct {
    uart_protocol_t protocol;
    size_t (*get_frame_header_size)(void);
    size_t (*get_frame_size)(const uint8_t* header, size_t len);
    dev_uart_event_type_t (*parse_frame)(const uint8_t* frame_data, size_t len, dev_uart_event_t* event);
    int (*process_event)(hid_device_report_t* buffer, dev_uart_event_t* event, dev_uart_event_rsp_t* rsp);
    void (*set_debug)(bool enable);
} uart_protocol_impl_t;

// UART manager structure
typedef struct {
    QueueHandle_t event_queue;          // Queue for UART events
    TaskHandle_t uart_rx_task_handle;   // UART RX task handle
    TaskHandle_t uart_evt_task_handle;  // UART event processing task handle
    bool initialized;                   // UART initialized flag

    // Protocol management
    uart_protocol_t current_protocol;           // Current protocol type
    const uart_protocol_impl_t* protocol_impl;  // Current protocol implementation
} dev_uart_manager_t;

// Global UART manager
extern dev_uart_manager_t g_uart_manager;

#ifdef __cplusplus
}
#endif

#endif // _UART_COMMON_H_