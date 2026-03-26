#include "uart.h"
#include "hid.h"
#include "device.h"
#include "utils.h"
#include "uart_protocol.h"
#include "uart_easycon.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"

// Log tag
#define LOG_UART "uart"

// Global UART manager
dev_uart_manager_t g_uart_manager = {
    .event_queue = NULL,
    .uart_task_handle = NULL,
    .initialized = false,
    .current_protocol = UART_PROTOCOL_SIMPLE,
    .protocol_impl = NULL,
};

static void uart_rx_task(void *arg) {
    uint8_t data[UART_RX_BUFFER_SIZE];
    uint8_t frame_buffer[UART_MAX_FRAME_SIZE];
    size_t frame_pos = 0;
    bool in_frame = false;
    const uart_protocol_impl_t* current_impl = NULL;

    ESP_LOGI(LOG_UART, "UART RX task started");

    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM, data, sizeof(data), pdMS_TO_TICKS(10));

        if (len > 0) {
            ESP_LOGD(LOG_UART, "Received %d bytes", len);

            // Get current protocol implementation
            current_impl = g_uart_manager.protocol_impl;
            if (current_impl == NULL) {
                // Protocol not initialized yet, skip processing
                vTaskDelay(1);
                continue;
            }

            size_t header_size = current_impl->get_frame_header_size();
            size_t frame_size = 0;

            for (int i = 0; i < len; i++) {
                uint8_t byte = data[i];
                if (!in_frame) {
                    if (header_size == 0) {
                        // header not needed, start frame immediately
                        in_frame = true;
                        frame_pos = 0;
                        // no header need setting frame_size
                        frame_size = current_impl->get_frame_size(NULL, 0);
                        continue;
                    }
                    if (frame_pos < header_size) {
                        frame_buffer[frame_pos++] = byte;
                        continue;
                    }
                    frame_size = current_impl->get_frame_size(frame_buffer, frame_pos);
                    if (frame_size > 0) {
                        in_frame = true;
                    } else {
                        // shift left by one byte
                        memmove(frame_buffer, frame_buffer + 1, frame_pos - 1);
                        frame_pos--;
                    }
                } else {
                    if (frame_pos < frame_size) {
                        frame_buffer[frame_pos++] = byte;
                        continue;
                    }
                    dev_uart_event_t event;
                    dev_uart_event_type_t type = uart_protocol_parse_frame(current_impl, frame_buffer, frame_size, &event);
                    if (type != UART_EVENT_UNKNOWN) {
                        if (g_uart_manager.event_queue != NULL) {
                            if (xQueueSend(g_uart_manager.event_queue, &event, 0) != pdTRUE) {
                                ESP_LOGW(LOG_UART, "Event queue full, dropping event");
                            }
                        }
                        ESP_LOGD(LOG_UART, "Frame parsed successfully, type=%d", type);
                    } else {
                        ESP_LOGD(LOG_UART, "Frame parsing failed");
                    }
                    // reset 
                    in_frame = false;
                    frame_pos = 0;
                    frame_size = 0;
                }
            }
        }

        // Small delay to yield CPU
        vTaskDelay(1);
    }
}

int dev_uart_init(void) {
    if (g_uart_manager.initialized) {
        ESP_LOGW(LOG_UART, "UART already initialized");
        return 0;
    }

    // Create event queue
    g_uart_manager.event_queue = xQueueCreate(32, sizeof(dev_uart_event_t));
    if (g_uart_manager.event_queue == NULL) {
        ESP_LOGE(LOG_UART, "Failed to create event queue");
        return -1;
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

    esp_err_t err = uart_param_config(UART_PORT_NUM, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_UART, "Failed to configure UART parameters: %s", esp_err_to_name(err));
        goto error;
    }

    // Set UART pins
    err = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_UART, "Failed to set UART pins: %s", esp_err_to_name(err));
        goto error;
    }

    // Install UART driver
    err = uart_driver_install(UART_PORT_NUM, UART_RX_BUFFER_SIZE, UART_TX_BUFFER_SIZE, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_UART, "Failed to install UART driver: %s", esp_err_to_name(err));
        goto error;
    }

    ESP_LOGI(LOG_UART, "UART initialized on port %d, baud rate %d", UART_PORT_NUM, UART_BAUD_RATE);
    ESP_LOGI(LOG_UART, "RX pin: GPIO%d, TX pin: GPIO%d", UART_RX_PIN, UART_TX_PIN);

    // Set default protocol based on configuration
    uart_protocol_t configured_protocol = uart_protocol_get_configured();

    int protocol_result = dev_uart_set_protocol(configured_protocol);
    if (protocol_result != 0) {
        ESP_LOGW(LOG_UART, "Failed to set protocol %d, using SIMPLE", configured_protocol);
        dev_uart_set_protocol(UART_PROTOCOL_SIMPLE);
    }

    g_uart_manager.initialized = true;
    return 0;

error:
    if (g_uart_manager.event_queue != NULL) {
        vQueueDelete(g_uart_manager.event_queue);
        g_uart_manager.event_queue = NULL;
    }
    return -1;
}

int dev_uart_start_task(void) {
    if (!g_uart_manager.initialized) {
        ESP_LOGE(LOG_UART, "UART not initialized");
        return -1;
    }

    if (g_uart_manager.uart_task_handle != NULL) {
        ESP_LOGW(LOG_UART, "UART task already started");
        return 0;
    }

    // Create UART RX task
    BaseType_t rc = xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 4, &g_uart_manager.uart_task_handle);
    if (rc != pdPASS) {
        ESP_LOGE(LOG_UART, "Failed to create UART task: %d", rc);
        return -1;
    }

    ESP_LOGI(LOG_UART, "UART RX task started");
    return 0;
}

void dev_uart_stop_task(void) {
    if (g_uart_manager.uart_task_handle != NULL) {
        vTaskDelete(g_uart_manager.uart_task_handle);
        g_uart_manager.uart_task_handle = NULL;
        ESP_LOGI(LOG_UART, "UART task stopped");
    }
}

void dev_uart_deinit(void) {
    dev_uart_stop_task();

    if (g_uart_manager.event_queue != NULL) {
        vQueueDelete(g_uart_manager.event_queue);
        g_uart_manager.event_queue = NULL;
    }

    // Uninstall UART driver
    uart_driver_delete(UART_PORT_NUM);

    g_uart_manager.initialized = false;
    ESP_LOGI(LOG_UART, "UART deinitialize");
}

bool dev_uart_get_event(dev_uart_event_t* event) {
    if (!g_uart_manager.initialized || g_uart_manager.event_queue == NULL) {
        return false;
    }

    return xQueueReceive(g_uart_manager.event_queue, event, 0) == pdTRUE;
}

void dev_uart_process_events(void) {
    if (!g_uart_manager.initialized || g_uart_manager.event_queue == NULL) {
        return;
    }

    dev_uart_event_t event;
    bool events_processed = false;

    while (dev_uart_get_event(&event)) {
        events_processed = true;
        if (event.type != UART_EVENT_UNKNOWN) {
            int rc = g_uart_manager.protocol_impl->process_event(
                g_hid_double_buffer.back_buffer, &event);
            if (rc != 0) {
                ESP_LOGE(LOG_UART, "Failed to process event: %d", event.type);
            }
        }
    }

    // If events were processed, request buffer swap
    if (events_processed) {
        g_hid_double_buffer.swap_request = 1;
    }
}

int dev_uart_send_data(const uint8_t* data, size_t len) {
    if (!g_uart_manager.initialized) {
        return -1;
    }

    int bytes_written = uart_write_bytes(UART_PORT_NUM, (const char*)data, len);
    if (bytes_written < 0) {
        ESP_LOGE(LOG_UART, "Failed to write UART data");
        return -1;
    }

    return bytes_written;
}

// Protocol management functions

int dev_uart_set_protocol(uart_protocol_t protocol) {
    if (!g_uart_manager.initialized) {
        ESP_LOGE(LOG_UART, "UART not initialized");
        return -1;
    }

    // Check if protocol is changing
    if (g_uart_manager.current_protocol == protocol &&
        g_uart_manager.protocol_impl != NULL) {
        ESP_LOGD(LOG_UART, "Protocol already set to %d", protocol);
        return 0;
    }

    // Get protocol implementation
    const uart_protocol_impl_t* new_impl = uart_protocol_get_impl(protocol);
    if (new_impl == NULL) {
        ESP_LOGE(LOG_UART, "Protocol implementation not found: %d", protocol);
        return -1;
    }

    // Update manager
    g_uart_manager.current_protocol = protocol;
    g_uart_manager.protocol_impl = new_impl;

    return 0;
}

uart_protocol_t dev_uart_get_protocol(void) {
    return g_uart_manager.current_protocol;
}

void dev_uart_set_debug_logging(bool enable) {
    uart_protocol_set_debug(enable);
}