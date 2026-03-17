#include "uart.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

// Task handle
TaskHandle_t uart_task_handle = NULL;

// UART task function
static void uart_task(void *arg) {
    ESP_LOGI(LOG_UART, "UART task started");

    uint8_t data[128];
    char command_buffer[MAX_COMMAND_LENGTH] = {0};
    int buffer_index = 0;

    while (1) {
        // Read data from UART
        int len = uart_read_bytes(UART_PORT_NUM, data, sizeof(data) - 1, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = '\0';  // Null terminate

            // Process each character
            for (int i = 0; i < len; i++) {
                char c = data[i];

                // Handle backspace (for terminal editing)
                if (c == '\b' || c == 0x7F) {
                    if (buffer_index > 0) {
                        buffer_index--;
                        command_buffer[buffer_index] = '\0';
                    }
                    continue;
                }

                // Handle carriage return or newline (end of command)
                if (c == '\r' || c == '\n') {
                    if (buffer_index > 0) {
                        command_buffer[buffer_index] = '\0';

                        // Echo back the command (for debugging)
                        uart_write_bytes(UART_PORT_NUM, "\r\n", 2);

                        // Parse and process command
                        if (!uart_parse_command(command_buffer)) {
                            // If parsing failed, send error message
                            const char *error_msg = "ERROR: Invalid command\r\n";
                            uart_write_bytes(UART_PORT_NUM, error_msg, strlen(error_msg));
                        } else {
                            // Send success message
                            const char *success_msg = "OK\r\n";
                            uart_write_bytes(UART_PORT_NUM, success_msg, strlen(success_msg));
                        }

                        // Reset buffer
                        buffer_index = 0;
                        command_buffer[0] = '\0';
                    }
                    continue;
                }

                // Ignore non-printable characters
                if (!isprint(c) && c != ' ') {
                    continue;
                }

                // Add character to buffer if there's space
                if (buffer_index < MAX_COMMAND_LENGTH - 1) {
                    command_buffer[buffer_index++] = c;
                    command_buffer[buffer_index] = '\0';

                    // Echo character back (optional, for interactive terminal)
                    uart_write_bytes(UART_PORT_NUM, &c, 1);
                } else {
                    // Buffer full, send error and reset
                    const char *error_msg = "\r\nERROR: Command too long\r\n";
                    uart_write_bytes(UART_PORT_NUM, error_msg, strlen(error_msg));
                    buffer_index = 0;
                    command_buffer[0] = '\0';
                }
            }
        }

        // Small delay to prevent busy waiting
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool uart_init(void) {
    if (uart_task_handle != NULL) {
        ESP_LOGW(LOG_UART, "UART task already started");
        return true;
    }

    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Install UART driver
    esp_err_t err = uart_driver_install(UART_PORT_NUM,
                                        UART_RX_BUFFER_SIZE,
                                        UART_TX_BUFFER_SIZE,
                                        0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_UART, "Failed to install UART driver: %d", err);
        return false;
    }

    // Configure UART parameters
    err = uart_param_config(UART_PORT_NUM, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_UART, "Failed to configure UART parameters: %d", err);
        uart_driver_delete(UART_PORT_NUM);
        return false;
    }

    // Set UART pins (using default pins for UART0)
    err = uart_set_pin(UART_PORT_NUM,
                       UART_PIN_NO_CHANGE,  // TX pin (default)
                       UART_PIN_NO_CHANGE,  // RX pin (default)
                       UART_PIN_NO_CHANGE,  // RTS pin
                       UART_PIN_NO_CHANGE); // CTS pin
    if (err != ESP_OK) {
        ESP_LOGE(LOG_UART, "Failed to set UART pins: %d", err);
        uart_driver_delete(UART_PORT_NUM);
        return false;
    }

    // Create UART task
    BaseType_t task_result = xTaskCreate(uart_task, "uart_task",
                                         UART_TASK_STACK_SIZE,
                                         NULL, UART_TASK_PRIORITY,
                                         &uart_task_handle);
    if (task_result != pdPASS) {
        ESP_LOGE(LOG_UART, "Failed to create UART task");
        uart_driver_delete(UART_PORT_NUM);
        return false;
    }

    ESP_LOGI(LOG_UART, "UART initialized successfully on port %d, baud rate %d",
             UART_PORT_NUM, UART_BAUD_RATE);

    // Send welcome message
    const char *welcome_msg = "\r\n================================\r\n"
                              "ESP32 Pro2 Controller UART Interface\r\n"
                              "Commands: PRESS <button> or RELEASE <button>\r\n"
                              "Buttons: A, B, X, Y, R, ZR, L, ZL, GR, GL\r\n"
                              "         UP, DOWN, LEFT, RIGHT, LCLICK, RCLICK\r\n"
                              "         PLUS, MINUS, HOME, CAPTURE, C\r\n"
                              "================================\r\n\r\n>";
    uart_write_bytes(UART_PORT_NUM, welcome_msg, strlen(welcome_msg));

    return true;
}

void uart_deinit(void) {
    if (uart_task_handle != NULL) {
        vTaskDelete(uart_task_handle);
        uart_task_handle = NULL;
    }

    uart_driver_delete(UART_PORT_NUM);
    ESP_LOGI(LOG_UART, "UART deinitialized");
}

bool uart_parse_command(const char* command) {
    if (command == NULL || strlen(command) == 0) {
        ESP_LOGE(LOG_UART, "Empty command");
        return false;
    }

    // Make a copy for tokenization
    char cmd_copy[MAX_COMMAND_LENGTH];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    // Tokenize command
    char *action_str = strtok(cmd_copy, " ");
    if (action_str == NULL) {
        ESP_LOGE(LOG_UART, "No action specified in command: %s", command);
        return false;
    }

    char *button_str = strtok(NULL, " ");
    if (button_str == NULL) {
        ESP_LOGE(LOG_UART, "No button specified in command: %s", command);
        return false;
    }

    // Convert to uppercase for case-insensitive comparison
    for (char *p = action_str; *p; p++) *p = toupper(*p);
    for (char *p = button_str; *p; p++) *p = toupper(*p);

    // Determine action
    button_action_t action;
    if (strcmp(action_str, "PRESS") == 0) {
        action = BUTTON_ACTION_PRESS;
    } else if (strcmp(action_str, "RELEASE") == 0) {
        action = BUTTON_ACTION_RELEASE;
    } else {
        ESP_LOGE(LOG_UART, "Unknown action: %s (expected PRESS or RELEASE)", action_str);
        return false;
    }

    // Convert button string to enum
    uart_button_t uart_button = button_str_to_enum(button_str);
    if (uart_button == BTN_UNKNOWN) {
        ESP_LOGE(LOG_UART, "Unknown button: %s", button_str);
        return false;
    }

    // Convert to Pro2 button
    pro2_btns button = uart_button_to_pro2_button(uart_button);

    // Create button event
    button_event_t event = {
        .button = button,
        .action = action
    };

    // Enqueue event
    if (!button_queue_send(&event)) {
        ESP_LOGE(LOG_UART, "Failed to enqueue button event");
        return false;
    }

    ESP_LOGI(LOG_UART, "Command processed: %s %s",
             action == BUTTON_ACTION_PRESS ? "PRESS" : "RELEASE", button_str);
    return true;
}

uart_button_t button_str_to_enum(const char* button_str) {
    if (button_str == NULL) return BTN_UNKNOWN;

    // Compare with known button names (case-insensitive)
    if (strcasecmp(button_str, "A") == 0) return BTN_A;
    if (strcasecmp(button_str, "B") == 0) return BTN_B;
    if (strcasecmp(button_str, "X") == 0) return BTN_X;
    if (strcasecmp(button_str, "Y") == 0) return BTN_Y;
    if (strcasecmp(button_str, "R") == 0) return BTN_R;
    if (strcasecmp(button_str, "ZR") == 0) return BTN_ZR;
    if (strcasecmp(button_str, "L") == 0) return BTN_L;
    if (strcasecmp(button_str, "ZL") == 0) return BTN_ZL;
    if (strcasecmp(button_str, "GR") == 0) return BTN_GR;
    if (strcasecmp(button_str, "GL") == 0) return BTN_GL;
    if (strcasecmp(button_str, "UP") == 0) return BTN_UP;
    if (strcasecmp(button_str, "DOWN") == 0) return BTN_DOWN;
    if (strcasecmp(button_str, "LEFT") == 0) return BTN_LEFT;
    if (strcasecmp(button_str, "RIGHT") == 0) return BTN_RIGHT;
    if (strcasecmp(button_str, "LCLICK") == 0) return BTN_LCLICK;
    if (strcasecmp(button_str, "RCLICK") == 0) return BTN_RCLICK;
    if (strcasecmp(button_str, "PLUS") == 0) return BTN_PLUS;
    if (strcasecmp(button_str, "MINUS") == 0) return BTN_MINUS;
    if (strcasecmp(button_str, "HOME") == 0) return BTN_HOME;
    if (strcasecmp(button_str, "CAPTURE") == 0) return BTN_CAPTURE;
    if (strcasecmp(button_str, "C") == 0) return BTN_C;

    return BTN_UNKNOWN;
}

pro2_btns uart_button_to_pro2_button(uart_button_t button) {
    switch (button) {
        case BTN_A:       return A;
        case BTN_B:       return B;
        case BTN_X:       return X;
        case BTN_Y:       return Y;
        case BTN_R:       return R;
        case BTN_ZR:      return ZR;
        case BTN_L:       return L;
        case BTN_ZL:      return ZL;
        case BTN_GR:      return GR;
        case BTN_GL:      return GL;
        case BTN_UP:      return Up;
        case BTN_DOWN:    return Down;
        case BTN_LEFT:    return Left;
        case BTN_RIGHT:   return Right;
        case BTN_LCLICK:  return LClick;
        case BTN_RCLICK:  return RClick;
        case BTN_PLUS:    return Plus;
        case BTN_MINUS:   return Minus;
        case BTN_HOME:    return Home;
        case BTN_CAPTURE: return Capture;
        case BTN_C:       return C;
        default:          return A;  // Default fallback (should not happen)
    }
}