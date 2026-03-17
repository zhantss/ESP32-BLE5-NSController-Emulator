#ifndef _UART_H_
#define _UART_H_

#include <stdbool.h>

#include "freertos/task.h"
#include "freertos/queue.h"
#include "hid.h"

// Log tag for UART module
#define LOG_UART "uart"

// UART configuration
#define UART_PORT_NUM      UART_NUM_0      // Use UART0 (usually connected to USB-to-serial)
#define UART_BAUD_RATE     115200          // Default baud rate
#define UART_RX_BUFFER_SIZE 1024           // Receive buffer size
#define UART_TX_BUFFER_SIZE 1024           // Transmit buffer size
#define UART_TASK_STACK_SIZE 4096          // Task stack size
#define UART_TASK_PRIORITY  5              // Task priority

// Maximum command length (including null terminator)
#define MAX_COMMAND_LENGTH 64

// Button name mapping
typedef enum {
    BTN_A,
    BTN_B,
    BTN_X,
    BTN_Y,
    BTN_R,
    BTN_ZR,
    BTN_L,
    BTN_ZL,
    BTN_GR,
    BTN_GL,
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_LCLICK,
    BTN_RCLICK,
    BTN_PLUS,
    BTN_MINUS,
    BTN_HOME,
    BTN_CAPTURE,
    BTN_C,
    BTN_UNKNOWN
} uart_button_t;

// UART task handle
extern TaskHandle_t uart_task_handle;

/**
 * @brief Initialize UART and start UART task
 *
 * @return true if initialization successful, false otherwise
 */
bool uart_init(void);

/**
 * @brief Stop UART task and deinitialize UART
 */
void uart_deinit(void);

/**
 * @brief Parse command string and enqueue button event
 *
 * @param command Command string (e.g., "PRESS A", "RELEASE B")
 * @return true if command parsed and event enqueued successfully, false otherwise
 */
bool uart_parse_command(const char* command);

/**
 * @brief Convert button name string to button enum
 *
 * @param button_str Button name string (e.g., "A", "B", "UP")
 * @return uart_button_t Corresponding button enum, BTN_UNKNOWN if not recognized
 */
uart_button_t button_str_to_enum(const char* button_str);

/**
 * @brief Convert uart_button_t to pro2_btns
 *
 * @param button UART button enum
 * @return pro2_btns Corresponding Pro2 button enum
 */
pro2_btns uart_button_to_pro2_button(uart_button_t button);

#endif // _UART_H_