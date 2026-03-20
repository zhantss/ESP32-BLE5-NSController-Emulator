#include "uart_simple.h"
#include "hid.h"
#include "device.h"

#include "esp_log.h"
#include <string.h>

// Log tag
#define LOG_SIMPLE "simple_protocol"

// Calculate XOR checksum (same as in uart.c)
uint8_t simple_protocol_calculate_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

size_t simple_protocol_get_expected_frame_size(const uint8_t* data, size_t len) {
    if (len < 2) {
        return 0; // Not enough data to determine frame size
    }

    if (data[0] != SIMPLE_PROTOCOL_START_BYTE) {
        return 0; // Not a simple protocol frame
    }

    uint8_t frame_type = data[1];
    switch (frame_type) {
        case SIMPLE_PROTOCOL_TYPE_BUTTON:
            return 5; // [START][TYPE][BUTTON_ID][PRESSED][CHECKSUM]
        case SIMPLE_PROTOCOL_TYPE_STICK:
            return 8; // [START][TYPE][STICK_ID][X_HIGH][X_LOW][Y_HIGH][Y_LOW][CHECKSUM]
        default:
            return SIMPLE_PROTOCOL_MAX_FRAME_SIZE; // Unknown type, return max size
    }
}

dev_uart_event_type_t simple_protocol_parse_frame(const uint8_t* data, size_t len, dev_uart_event_t* event) {
    if (len < 4 || data[0] != SIMPLE_PROTOCOL_START_BYTE) {
        return UART_EVENT_UNKNOWN;
    }

    uint8_t frame_type = data[1];
    size_t data_len = len - 3; // Exclude start byte, type, and checksum

    // Verify checksum
    uint8_t checksum = simple_protocol_calculate_checksum(data, len - 1);
    if (checksum != data[len - 1]) {
        ESP_LOGE(LOG_SIMPLE, "Checksum mismatch: expected 0x%02X, got 0x%02X", checksum, data[len - 1]);
        return UART_EVENT_UNKNOWN;
    }

    switch (frame_type) {
        case SIMPLE_PROTOCOL_TYPE_BUTTON:
            if (data_len == 2) {
                uint8_t button_id = data[2];
                bool pressed = data[3] != 0;

                if (button_id < BUTTON_MAP_SIZE) {
                    event->type = UART_EVENT_BUTTON;
                    event->data.button.button_id = button_id;
                    event->data.button.pressed = pressed;
                    ESP_LOGD(LOG_SIMPLE, "Button event: id=%d, pressed=%d", button_id, pressed);
                    return UART_EVENT_BUTTON;
                }
            }
            break;

        case SIMPLE_PROTOCOL_TYPE_STICK:
            if (data_len == 5) {
                uint8_t stick_id = data[2];
                uint16_t x = (data[3] << 8) | data[4];
                uint16_t y = (data[5] << 8) | data[6];

                // Validate stick coordinates (12-bit)
                if (stick_id < 2 && x <= 0xFFF && y <= 0xFFF) {
                    event->type = UART_EVENT_STICK;
                    event->data.stick.stick_id = stick_id;
                    event->data.stick.x = x;
                    event->data.stick.y = y;
                    ESP_LOGD(LOG_SIMPLE, "Stick event: id=%d, x=0x%03X, y=0x%03X", stick_id, x, y);
                    return UART_EVENT_STICK;
                }
            }
            break;

        default:
            ESP_LOGW(LOG_SIMPLE, "Unknown frame type: 0x%02X", frame_type);
            break;
    }

    return UART_EVENT_UNKNOWN;
}

bool simple_protocol_detect(const uint8_t* data, size_t len) {
    // Simple protocol detection logic
    if (len < 2) {
        return false;
    }

    // Check for start byte
    if (data[0] != SIMPLE_PROTOCOL_START_BYTE) {
        return false;
    }

    // Check for valid frame type
    uint8_t frame_type = data[1];
    if (frame_type != SIMPLE_PROTOCOL_TYPE_BUTTON &&
        frame_type != SIMPLE_PROTOCOL_TYPE_STICK) {
        return false;
    }

    // If we have enough data, check frame size
    if (len >= 4) {
        size_t expected_size = simple_protocol_get_expected_frame_size(data, len);
        if (expected_size > 0 && len >= expected_size) {
            // Verify checksum if we have full frame
            if (len >= expected_size) {
                uint8_t checksum = simple_protocol_calculate_checksum(data, expected_size - 1);
                if (checksum == data[expected_size - 1]) {
                    ESP_LOGD(LOG_SIMPLE, "Detected simple protocol with valid checksum");
                    return true;
                }
            }
        }
    }

    // If we have start byte and valid type, assume it's simple protocol
    return true;
}

// Protocol initialization (no-op for simple protocol)
static void simple_protocol_init(void) {
    ESP_LOGD(LOG_SIMPLE, "Simple protocol initialized");
}

// Protocol deinitialization (no-op for simple protocol)
static void simple_protocol_deinit(void) {
    ESP_LOGD(LOG_SIMPLE, "Simple protocol deinitialize");
}

// Get protocol version
static const char* simple_protocol_get_version(void) {
    return "1.0";
}

static void simple_protocol_set_debug(bool enabled) {
    ESP_LOGD(LOG_SIMPLE, "Debug mode %s", enabled ? "enabled" : "disabled");
}

// Simple protocol implementation structure
const uart_protocol_impl_t simple_protocol_impl = {
    .protocol = UART_PROTOCOL_SIMPLE,
    .name = "Simple Protocol",
    .parse_frame = simple_protocol_parse_frame,
    .detect_protocol = simple_protocol_detect,
    .get_expected_frame_size = simple_protocol_get_expected_frame_size,
    .init_protocol = simple_protocol_init,
    .deinit_protocol = simple_protocol_deinit,
    .get_version = simple_protocol_get_version,
    .set_debug = simple_protocol_set_debug
};