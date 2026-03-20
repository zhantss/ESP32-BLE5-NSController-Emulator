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

// Button mapping from UART ID to pro2_btns
extern const pro2_btns button_map[];
extern const size_t BUTTON_MAP_SIZE;
// #define BUTTON_MAP_SIZE (sizeof(button_map) / sizeof(button_map[0]))

// UART Configuration
#define UART_PORT_NUM           UART_NUM_1
#define UART_BAUD_RATE          115200
#define UART_RX_BUFFER_SIZE     1024
#define UART_TX_BUFFER_SIZE     1024
#define UART_RX_PIN             4   // GPIO4 for RX
#define UART_TX_PIN             5   // GPIO5 for TX
#define UART_MAX_FRAME_SIZE     32   // Maximum frame size for all protocols

// Button event for queue communication
typedef struct {
    uint8_t button_id;      // Button ID (mapping to pro2_btns)
    bool pressed;           // Pressed (true) or released (false)
} button_event_t;

// Stick event for queue communication
typedef struct {
    uint8_t stick_id;       // 0 = left stick, 1 = right stick
    uint16_t x;             // X coordinate (12-bit, 0-0xFFF)
    uint16_t y;             // Y coordinate (12-bit, 0-0xFFF)
} stick_event_t;

// HID event for complete controller state
typedef struct {
    uint16_t button_mask;    // Button bitmask (16 bits for non-direction, non-C buttons)
    uint8_t hat_state;       // HAT direction state (0x00-0x08)
    uint16_t left_stick_x;   // Left stick X coordinate (12-bit, 0-0xFFF)
    uint16_t left_stick_y;   // Left stick Y coordinate (12-bit, 0-0xFFF)
    uint16_t right_stick_x;  // Right stick X coordinate (12-bit, 0-0xFFF)
    uint16_t right_stick_y;  // Right stick Y coordinate (12-bit, 0-0xFFF)
} hid_event_t;

// Union for event data
typedef union {
    button_event_t button;
    stick_event_t stick;
    hid_event_t hid;        // Complete HID state event
} dev_uart_event_data_t;

// UART event types
typedef enum {
    UART_EVENT_BUTTON,
    UART_EVENT_STICK,
    UART_EVENT_HID,      // Complete HID state event
    UART_EVENT_UNKNOWN
} dev_uart_event_type_t;

// Complete UART event with type
typedef struct {
    dev_uart_event_type_t type; // Event type
    dev_uart_event_data_t data; // Event data
} dev_uart_event_t;

/**
 * @brief UART protocol types
 */
typedef enum {
    UART_PROTOCOL_SIMPLE = 0,     ///< Simple protocol with XOR checksum
    UART_PROTOCOL_EASYCON = 1,    ///< EasyCon protocol for advanced controllers
    UART_PROTOCOL_AUTO_DETECT = 2 ///< Auto-detect protocol from data
} uart_protocol_t;

/**
 * @brief Protocol interface structure
 *
 * Each protocol implementation must provide these functions
 */
typedef struct {
    uart_protocol_t protocol;      ///< Protocol identifier
    const char* name;              ///< Human-readable protocol name

    /**
     * @brief Parse a frame from raw UART data
     * @param data Raw UART data
     * @param len Length of data
     * @param event Output event if valid frame
     * @return Event type if valid frame, UART_EVENT_UNKNOWN otherwise
     */
    dev_uart_event_type_t (*parse_frame)(const uint8_t* data, size_t len, dev_uart_event_t* event);

    /**
     * @brief Detect if this protocol is being used
     * @param data Raw UART data
     * @param len Length of data
     * @return true if this protocol is detected, false otherwise
     */
    bool (*detect_protocol)(const uint8_t* data, size_t len);

    /**
     * @brief Get expected frame size based on current data
     * @param data Raw UART data (at least start of frame)
     * @param len Length of data available
     * @return Expected total frame size, or 0 if cannot determine
     */
    size_t (*get_expected_frame_size)(const uint8_t* data, size_t len);

    /**
     * @brief Initialize protocol-specific resources
     */
    void (*init_protocol)(void);

    /**
     * @brief Deinitialize protocol-specific resources
     */
    void (*deinit_protocol)(void);

    /**
     * @brief Get protocol version string
     * @return Version string (e.g., "1.0")
     */
    const char* (*get_version)(void);

    /**
     * @brief Set debug mode
     * 
     */
    void (*set_debug)(bool enabled);
} uart_protocol_impl_t;

/**
 * @brief Protocol statistics
 */
typedef struct {
    uint32_t frames_received;      ///< Total frames received
    uint32_t frames_parsed;        ///< Successfully parsed frames
    uint32_t parse_errors;         ///< Parse errors
    uint32_t checksum_errors;      ///< Checksum errors
    uint32_t protocol_detections;  ///< Protocol detection attempts
    uint32_t protocol_switches;    ///< Protocol switches
} uart_protocol_stats_t;

// UART manager structure
typedef struct {
    QueueHandle_t event_queue;      // Queue for UART events
    SemaphoreHandle_t report_mutex; // Mutex for HID report access
    TaskHandle_t uart_task_handle;  // UART task handle
    bool initialized;               // UART initialized flag

    // Protocol management
    uart_protocol_t current_protocol;           // Current protocol type
    const uart_protocol_impl_t* protocol_impl;  // Current protocol implementation
    uart_protocol_stats_t protocol_stats;       // Protocol statistics
} dev_uart_manager_t;

// Global UART manager
extern dev_uart_manager_t g_uart_manager;

#ifdef __cplusplus
}
#endif

#endif // _UART_COMMON_H_