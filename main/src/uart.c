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
    .report_mutex = NULL,
    .uart_task_handle = NULL,
    .initialized = false,
    .current_protocol = UART_PROTOCOL_SIMPLE,
    .protocol_impl = NULL,
    .protocol_stats = {0}
};

// Simple protocol format:
// Frame format: [START_BYTE][TYPE][DATA...][CHECKSUM]
#define UART_START_BYTE         0xAA
#define UART_TYPE_BUTTON        0x01
#define UART_TYPE_STICK         0x02
// #define UART_MAX_FRAME_SIZE     16

const pro2_btns button_map[] = {
    // ID 0-7: 4 non-direction buttons in pro2_btn_bits_t byte 0
    B,      // ID 0 (enum 0)
    A,      // ID 1 (enum 1)
    Y,      // ID 2 (enum 2)
    X,      // ID 3 (enum 3)
    R,      // ID 4 (enum 4)
    ZR,     // ID 5 (enum 5)
    Plus,   // ID 6 (enum 6)
    RClick, // ID 7 (enum 7)

    // ID 8-11: 4 non-direction buttons in pro2_btn_bits_t byte 1
    L,      // ID 8 (enum 12) - skip Down,Right,Left,Up
    ZL,     // ID 9 (enum 13)
    Minus,  // ID 10 (enum 14)
    LClick, // ID 11 (enum 15)

    // ID 12-15: 4 buttons in pro2_btn_bits_t byte 2 (excluding C)
    Home,   // ID 12 (enum 16)
    Capture,// ID 13 (enum 17)
    GR,     // ID 14 (enum 18)
    GL,     // ID 15 (enum 19)

    // Note: Direction buttons (Down,Right,Left,Up) and C button are not in button_map
    // They are handled separately
};
const size_t BUTTON_MAP_SIZE = 16;  // 16 non-direction, non-C buttons

// Process HAT direction and update direction buttons
static void process_hat_direction_internal(uint8_t hat_value) {
    // Extract direction part (bit0-3)
    uint8_t direction = hat_value & 0x0F;

    // Process direction buttons
    bool up_pressed = false, down_pressed = false, left_pressed = false, right_pressed = false;

    switch (direction) {
        case HAT_UP:           up_pressed = true; break;
        case HAT_UP_RIGHT:     up_pressed = true; right_pressed = true; break;
        case HAT_RIGHT:        right_pressed = true; break;
        case HAT_DOWN_RIGHT:   down_pressed = true; right_pressed = true; break;
        case HAT_DOWN:         down_pressed = true; break;
        case HAT_DOWN_LEFT:    down_pressed = true; left_pressed = true; break;
        case HAT_LEFT:         left_pressed = true; break;
        case HAT_UP_LEFT:      up_pressed = true; left_pressed = true; break;
        case HAT_CENTER:
        default:
            // All directions released
            break;
    }

    // Update direction button states
    pro2_set_button(pro2_hid_report, Up, up_pressed);
    pro2_set_button(pro2_hid_report, Down, down_pressed);
    pro2_set_button(pro2_hid_report, Left, left_pressed);
    pro2_set_button(pro2_hid_report, Right, right_pressed);
}

// Process complete EasyCon HID event
static void process_ec_hid_event(const ec_hid_event_t* hid_event) {
    // Process 16 non-direction, non-C buttons
    for (int i = 0; i < 16; i++) {
        if (i < BUTTON_MAP_SIZE) {
            pro2_btns btn = button_map[i];
            bool pressed = (hid_event->button_mask & (1 << i)) != 0;
            pro2_set_button(pro2_hid_report, btn, pressed);
        }
    }

    // Process HAT direction
    process_hat_direction_internal(hid_event->hat_state);

    // Process stick data
    pro2_set_left_stick(pro2_hid_report, hid_event->left_stick_x, hid_event->left_stick_y);
    pro2_set_right_stick(pro2_hid_report, hid_event->right_stick_x, hid_event->right_stick_y);
}

// Process Simple HID event (new protocol)
static void process_simple_hid_event(const simple_hid_event_t* simple_hid) {
    // Process all 21 buttons from 3 button bytes
    // Button bytes correspond to pro2_btn_bits_t structure
    uint8_t* btn_bytes = (uint8_t*)simple_hid->button_bytes;

    // Byte 0: B, A, Y, X, R, ZR, Plus, RClick
    if (btn_bytes[0] & 0x01) pro2_press_button(pro2_hid_report, B);
    else pro2_release_button(pro2_hid_report, B);
    if (btn_bytes[0] & 0x02) pro2_press_button(pro2_hid_report, A);
    else pro2_release_button(pro2_hid_report, A);
    if (btn_bytes[0] & 0x04) pro2_press_button(pro2_hid_report, Y);
    else pro2_release_button(pro2_hid_report, Y);
    if (btn_bytes[0] & 0x08) pro2_press_button(pro2_hid_report, X);
    else pro2_release_button(pro2_hid_report, X);
    if (btn_bytes[0] & 0x10) pro2_press_button(pro2_hid_report, R);
    else pro2_release_button(pro2_hid_report, R);
    if (btn_bytes[0] & 0x20) pro2_press_button(pro2_hid_report, ZR);
    else pro2_release_button(pro2_hid_report, ZR);
    if (btn_bytes[0] & 0x40) pro2_press_button(pro2_hid_report, Plus);
    else pro2_release_button(pro2_hid_report, Plus);
    if (btn_bytes[0] & 0x80) pro2_press_button(pro2_hid_report, RClick);
    else pro2_release_button(pro2_hid_report, RClick);

    // Byte 1: Down, Right, Left, Up, L, ZL, Minus, LClick
    if (btn_bytes[1] & 0x01) pro2_press_button(pro2_hid_report, Down);
    else pro2_release_button(pro2_hid_report, Down);
    if (btn_bytes[1] & 0x02) pro2_press_button(pro2_hid_report, Right);
    else pro2_release_button(pro2_hid_report, Right);
    if (btn_bytes[1] & 0x04) pro2_press_button(pro2_hid_report, Left);
    else pro2_release_button(pro2_hid_report, Left);
    if (btn_bytes[1] & 0x08) pro2_press_button(pro2_hid_report, Up);
    else pro2_release_button(pro2_hid_report, Up);
    if (btn_bytes[1] & 0x10) pro2_press_button(pro2_hid_report, L);
    else pro2_release_button(pro2_hid_report, L);
    if (btn_bytes[1] & 0x20) pro2_press_button(pro2_hid_report, ZL);
    else pro2_release_button(pro2_hid_report, ZL);
    if (btn_bytes[1] & 0x40) pro2_press_button(pro2_hid_report, Minus);
    else pro2_release_button(pro2_hid_report, Minus);
    if (btn_bytes[1] & 0x80) pro2_press_button(pro2_hid_report, LClick);
    else pro2_release_button(pro2_hid_report, LClick);

    // Byte 2: Home, Capture, GR, GL, C, reserved(3 bits)
    if (btn_bytes[2] & 0x01) pro2_press_button(pro2_hid_report, Home);
    else pro2_release_button(pro2_hid_report, Home);
    if (btn_bytes[2] & 0x02) pro2_press_button(pro2_hid_report, Capture);
    else pro2_release_button(pro2_hid_report, Capture);
    if (btn_bytes[2] & 0x04) pro2_press_button(pro2_hid_report, GR);
    else pro2_release_button(pro2_hid_report, GR);
    if (btn_bytes[2] & 0x08) pro2_press_button(pro2_hid_report, GL);
    else pro2_release_button(pro2_hid_report, GL);
    if (btn_bytes[2] & 0x10) pro2_press_button(pro2_hid_report, C);
    else pro2_release_button(pro2_hid_report, C);

    // Process stick data
    pro2_set_left_stick(pro2_hid_report, simple_hid->left_stick_x, simple_hid->left_stick_y);
    pro2_set_right_stick(pro2_hid_report, simple_hid->right_stick_x, simple_hid->right_stick_y);
}

static uint8_t calculate_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

dev_uart_event_type_t dev_uart_parse_frame(const uint8_t* data, size_t len, dev_uart_event_t* event) {
    // Use protocol abstraction layer
    if (g_uart_manager.protocol_impl == NULL) {
        ESP_LOGE(LOG_UART, "No protocol implementation selected");
        return UART_EVENT_UNKNOWN;
    }

    return uart_protocol_parse_frame(g_uart_manager.protocol_impl, data, len, event);
}

static void uart_rx_task(void *arg) {
    uint8_t data[UART_RX_BUFFER_SIZE];
    uint8_t frame_buffer[UART_MAX_FRAME_SIZE];
    size_t frame_pos = 0;
    bool in_frame = false;
    const uart_protocol_impl_t* current_impl = NULL;
    const size_t MAX_HEADER_DETECT_LEN = 8; // Maximum bytes needed for protocol detection

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

            for (int i = 0; i < len; i++) {
                uint8_t byte = data[i];

                if (!in_frame) {
                    // Add byte to frame buffer for detection
                    if (frame_pos < UART_MAX_FRAME_SIZE) {
                        frame_buffer[frame_pos++] = byte;
                    }

                    // If buffer exceeds detection window, discard oldest byte
                    if (frame_pos > MAX_HEADER_DETECT_LEN) {
                        // Shift left by one byte
                        memmove(frame_buffer, frame_buffer + 1, frame_pos - 1);
                        frame_pos--;
                    }

                    // Try to detect protocol with current buffer
                    if (current_impl->detect_protocol != NULL) {
                        size_t header_offset = current_impl->detect_protocol(frame_buffer, frame_pos);
                        if (header_offset > 0) {
                            // Convert back to 0-based offset
                            header_offset--;

                            // If header not at start, shift buffer to align
                            if (header_offset > 0) {
                                memmove(frame_buffer, frame_buffer + header_offset, frame_pos - header_offset);
                                frame_pos -= header_offset;
                                ESP_LOGD(LOG_UART, "Aligned frame header, offset %d, new buffer size %d", header_offset, frame_pos);
                            }

                            // Start frame collection
                            in_frame = true;
                            ESP_LOGD(LOG_UART, "Frame start detected, buffer size %d", frame_pos);
                        }
                    }
                } else {
                    // Continue frame collection
                    if (frame_pos < UART_MAX_FRAME_SIZE) {
                        frame_buffer[frame_pos++] = byte;
                    } else {
                        // Frame too long
                        ESP_LOGW(LOG_UART, "Frame too long, resetting");
                        in_frame = false;
                        frame_pos = 0;
                        continue;
                    }

                    // Check if we have a complete frame
                    bool frame_complete = false;
                    size_t expected_size = 0;

                    if (current_impl->get_expected_frame_size != NULL) {
                        expected_size = current_impl->get_expected_frame_size(frame_buffer, frame_pos);
                        if (expected_size > 0 && frame_pos >= expected_size) {
                            frame_complete = true;
                        }
                    }

                    // If protocol doesn't know expected size, try parsing after minimum bytes
                    if (!frame_complete && frame_pos >= 4) {
                        // Try parsing anyway for protocols without get_expected_frame_size
                        frame_complete = true;
                    }

                    if (frame_complete) {
                        dev_uart_event_t event;
                        dev_uart_event_type_t type = dev_uart_parse_frame(frame_buffer, frame_pos, &event);

                        if (type != UART_EVENT_UNKNOWN) {
                            // Valid frame, send to queue
                            if (g_uart_manager.event_queue != NULL) {
                                if (xQueueSend(g_uart_manager.event_queue, &event, 0) != pdTRUE) {
                                    ESP_LOGW(LOG_UART, "Event queue full, dropping event");
                                }
                            }
                            ESP_LOGD(LOG_UART, "Frame parsed successfully, type=%d", type);
                        } else {
                            ESP_LOGD(LOG_UART, "Frame parsing failed");
                        }

                        // Reset for next frame
                        in_frame = false;
                        frame_pos = 0;
                    }
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

    // Create mutex for HID report access
    g_uart_manager.report_mutex = xSemaphoreCreateMutex();
    if (g_uart_manager.report_mutex == NULL) {
        ESP_LOGE(LOG_UART, "Failed to create mutex");
        vQueueDelete(g_uart_manager.event_queue);
        g_uart_manager.event_queue = NULL;
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

    // Initialize protocol system
    uart_protocol_init_all();

    // Set default protocol based on configuration
    uart_protocol_t configured_protocol = uart_protocol_get_configured();

    int protocol_result = dev_uart_set_protocol(configured_protocol);
    if (protocol_result != 0) {
        ESP_LOGW(LOG_UART, "Failed to set protocol %d, using SIMPLE", configured_protocol);
        dev_uart_set_protocol(UART_PROTOCOL_SIMPLE);
    }

    ESP_LOGI(LOG_UART, "Protocol set to: %s", uart_protocol_get_name(configured_protocol));

    g_uart_manager.initialized = true;
    return 0;

error:
    if (g_uart_manager.event_queue != NULL) {
        vQueueDelete(g_uart_manager.event_queue);
        g_uart_manager.event_queue = NULL;
    }
    if (g_uart_manager.report_mutex != NULL) {
        vSemaphoreDelete(g_uart_manager.report_mutex);
        g_uart_manager.report_mutex = NULL;
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

    if (g_uart_manager.report_mutex != NULL) {
        vSemaphoreDelete(g_uart_manager.report_mutex);
        g_uart_manager.report_mutex = NULL;
    }

    // Uninstall UART driver
    uart_driver_delete(UART_PORT_NUM);

    g_uart_manager.initialized = false;
    ESP_LOGI(LOG_UART, "UART deinitialized");
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
    while (dev_uart_get_event(&event)) {
        // Update HID report with mutex protection
        if (xSemaphoreTake(g_uart_manager.report_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (pro2_hid_report != NULL) {
                // Process event based on type
                switch (event.type) {
                    case UART_EVENT_EC_HID:
                        process_ec_hid_event(&event.data.ec_hid);
                        ESP_LOGD(LOG_UART, "Processed HID event: buttons=0x%04X, hat=0x%02X",
                                 event.data.ec_hid.button_mask, event.data.ec_hid.hat_state);
                        break;

                    case UART_EVENT_SIMPLE_HID:
                        process_simple_hid_event(&event.data.simple_hid);
                        ESP_LOGD(LOG_UART, "Processed full HID event: buttons=0x%02X%02X%02X",
                                 event.data.simple_hid.button_bytes[0],
                                 event.data.simple_hid.button_bytes[1],
                                 event.data.simple_hid.button_bytes[2]);
                        break;

                    case UART_EVENT_SIMPLE_MANAGEMENT:
                        // Handle management commands
                        ESP_LOGD(LOG_UART, "Management event: command=0x%02X",
                                 event.data.management.command);
                        // TODO: Implement management command handling
                        // (handshake, heartbeat, reboot, etc.)
                        break;

                    case UART_EVENT_SIMPLE_SENSOR:
                        // Handle sensor data
                        ESP_LOGD(LOG_UART, "Sensor event: type=0x%02X, data=0x%02X%02X%02X%02X",
                                 event.data.sensor.sensor_type,
                                 event.data.sensor.sensor_data[0],
                                 event.data.sensor.sensor_data[1],
                                 event.data.sensor.sensor_data[2],
                                 event.data.sensor.sensor_data[3]);
                        // TODO: Process sensor data as needed
                        break;

                    case UART_EVENT_UNKNOWN:
                    default:
                        ESP_LOGD(LOG_UART, "Unknown event type: %d", event.type);
                        break;
                }
            }
            xSemaphoreGive(g_uart_manager.report_mutex);
        }
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

// Function to update HID report from button event (to be called from hid_task)
void uart_update_hid_report_from_button(uint8_t button_id, bool pressed) {
    if (button_id >= BUTTON_MAP_SIZE) {
        ESP_LOGW(LOG_UART, "Invalid button ID: %d", button_id);
        return;
    }

    if (xSemaphoreTake(g_uart_manager.report_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (pro2_hid_report != NULL) {
            pro2_set_button(pro2_hid_report, button_map[button_id], pressed);
            ESP_LOGD(LOG_UART, "Updated button %d (%s) to %s",
                     button_id,
                     button_id < BUTTON_MAP_SIZE ? "valid" : "invalid",
                     pressed ? "pressed" : "released");
        }
        xSemaphoreGive(g_uart_manager.report_mutex);
    }
}

// Function to update HID report from stick event (to be called from hid_task)
void uart_update_hid_report_from_stick(uint8_t stick_id, uint16_t x, uint16_t y) {
    if (stick_id >= 2) {
        ESP_LOGW(LOG_UART, "Invalid stick ID: %d", stick_id);
        return;
    }

    if (xSemaphoreTake(g_uart_manager.report_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (pro2_hid_report != NULL) {
            if (stick_id == 0) {
                pro2_set_left_stick(pro2_hid_report, x, y);
                ESP_LOGD(LOG_UART, "Updated left stick: x=0x%03X, y=0x%03X", x, y);
            } else {
                pro2_set_right_stick(pro2_hid_report, x, y);
                ESP_LOGD(LOG_UART, "Updated right stick: x=0x%03X, y=0x%03X", x, y);
            }
        }
        xSemaphoreGive(g_uart_manager.report_mutex);
    }
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

    // Deinitialize old protocol if needed
    if (g_uart_manager.protocol_impl != NULL &&
        g_uart_manager.protocol_impl->deinit_protocol != NULL) {
        g_uart_manager.protocol_impl->deinit_protocol();
    }

    // Initialize new protocol
    if (new_impl->init_protocol != NULL) {
        new_impl->init_protocol();
    }

    // Update manager
    g_uart_manager.current_protocol = protocol;
    g_uart_manager.protocol_impl = new_impl;

    // Update statistics
    uart_protocol_update_stats_protocol_switch();

    ESP_LOGI(LOG_UART, "Protocol switched to: %s (v%s)",
             new_impl->name,
             new_impl->get_version != NULL ? new_impl->get_version() : "unknown");

    return 0;
}

uart_protocol_t dev_uart_get_protocol(void) {
    return g_uart_manager.current_protocol;
}

void dev_uart_get_protocol_stats(uart_protocol_stats_t* stats) {
    if (stats != NULL) {
        // Combine global stats with manager stats
        uart_protocol_get_stats(stats);
        // Also include manager stats if needed
        stats->protocol_switches = g_uart_manager.protocol_stats.protocol_switches;
    }
}

void dev_uart_reset_protocol_stats(void) {
    uart_protocol_reset_stats();
    memset(&g_uart_manager.protocol_stats, 0, sizeof(uart_protocol_stats_t));
    ESP_LOGI(LOG_UART, "Protocol statistics reset");
}

void dev_uart_set_debug_logging(bool enable) {
    uart_protocol_set_debug(enable);
    ESP_LOGI(LOG_UART, "Debug logging %s", enable ? "enabled" : "disabled");
}