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

// Calculate CRC-16-CCITT (polynomial 0x1021, initial value 0xFFFF)
// Used for new protocol frames
uint16_t simple_protocol_calculate_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// Unpack stick data from packed format (same as in hid.c)
// packed format: byte0=x[7:0], byte1=(y[3:0]<<4)|x[11:8], byte2=y[11:4]
static void unpack_stick_data(const uint8_t in[3], uint16_t *x, uint16_t *y) {
    *x = in[0] | ((in[1] & 0x0F) << 8);
    *y = ((in[1] >> 4) & 0x0F) | (in[2] << 4);
    // Ensure 12 bits limit
    *x &= 0xFFF;
    *y &= 0xFFF;
}

size_t simple_protocol_get_expected_frame_size(const uint8_t* data, size_t len) {
    if (len < 2) {
        return 0; // Not enough data to determine frame size
    }

    // Check for new protocol (0xAA 0x55 header)
    if (data[0] == SIMPLE_PROTOCOL_NEW_START_BYTE1 && len >= 3) {
        if (data[1] == SIMPLE_PROTOCOL_NEW_START_BYTE2) {
            // New protocol detected
            uint8_t frame_type = data[2];
            switch (frame_type) {
                case SIMPLE_PROTOCOL_TYPE_HID:
                    return SIMPLE_FRAME_SIZE_HID;    // 16 bytes
                case SIMPLE_PROTOCOL_TYPE_MANAGEMENT:
                    return SIMPLE_FRAME_SIZE_MANAGEMENT;  // 8 bytes
                case SIMPLE_PROTOCOL_TYPE_SENSOR:
                    return SIMPLE_FRAME_SIZE_SENSOR;      // 12 bytes
                default:
                    return SIMPLE_PROTOCOL_MAX_FRAME_SIZE; // Unknown new type
            }
        }
    }

    return 0; // Not a simple protocol frame
}

dev_uart_event_type_t simple_protocol_parse_frame(const uint8_t* data, size_t len, dev_uart_event_t* event) {
    // First check for new protocol (0xAA 0x55 header)
    if (len >= 8 && data[0] == SIMPLE_PROTOCOL_NEW_START_BYTE1 && data[1] == SIMPLE_PROTOCOL_NEW_START_BYTE2) {
        // New protocol detected
        if (len < 8) { // Minimum frame size for new protocol (management frame)
            return UART_EVENT_UNKNOWN;
        }

        // Get frame type (third byte)
        uint8_t frame_type = data[2];

        // Get expected frame size
        size_t expected_size = simple_protocol_get_expected_frame_size(data, len);
        if (expected_size == 0 || len < expected_size) {
            return UART_EVENT_UNKNOWN;
        }

        // Verify frame footer (last 2 bytes should be 0x55 0xAA)
        if (data[expected_size - 2] != SIMPLE_PROTOCOL_NEW_END_BYTE1 ||
            data[expected_size - 1] != SIMPLE_PROTOCOL_NEW_END_BYTE2) {
            ESP_LOGE(LOG_SIMPLE, "Invalid frame footer: 0x%02X 0x%02X",
                     data[expected_size - 2], data[expected_size - 1]);
            return UART_EVENT_UNKNOWN;
        }

        // Verify CRC-16 (bytes before footer)
        // CRC covers from frame_type (byte 2) to payload end (before CRC)
        size_t crc_data_len = expected_size - 6; // Exclude header(2), footer(2), CRC(2)
        uint16_t crc_calculated = simple_protocol_calculate_crc16(&data[2], crc_data_len);
        uint16_t crc_received = (data[expected_size - 4] << 8) | data[expected_size - 3];

        if (crc_calculated != crc_received) {
            ESP_LOGE(LOG_SIMPLE, "CRC mismatch: expected 0x%04X, got 0x%04X",
                     crc_calculated, crc_received);
            return UART_EVENT_UNKNOWN;
        }

        // Parse based on frame type
        switch (frame_type) {
            case SIMPLE_PROTOCOL_TYPE_HID: {
                // Full HID data frame (16 bytes total)
                if (expected_size != SIMPLE_FRAME_SIZE_HID) {
                    ESP_LOGE(LOG_SIMPLE, "Invalid full HID frame size: %zu", expected_size);
                    return UART_EVENT_UNKNOWN;
                }

                // Parse button data (bytes 3-5)
                uint8_t button_bytes[3];
                button_bytes[0] = data[3];
                button_bytes[1] = data[4];
                button_bytes[2] = data[5];

                // Parse left stick data (bytes 6-8, packed format)
                uint16_t left_x, left_y;
                unpack_stick_data(&data[6], &left_x, &left_y);

                // Parse right stick data (bytes 9-11, packed format)
                uint16_t right_x, right_y;
                unpack_stick_data(&data[9], &right_x, &right_y);

                // Fill event
                event->type = UART_EVENT_SIMPLE_HID;
                memcpy(event->data.simple_hid.button_bytes, button_bytes, 3);
                event->data.simple_hid.left_stick_x = left_x;
                event->data.simple_hid.left_stick_y = left_y;
                event->data.simple_hid.right_stick_x = right_x;
                event->data.simple_hid.right_stick_y = right_y;

                ESP_LOGD(LOG_SIMPLE, "Full HID event: buttons=0x%02X%02X%02X, L(%03X,%03X), R(%03X,%03X)",
                         button_bytes[0], button_bytes[1], button_bytes[2],
                         left_x, left_y, right_x, right_y);
                return UART_EVENT_SIMPLE_HID;
            }

            case SIMPLE_PROTOCOL_TYPE_MANAGEMENT: {
                // Management frame (8 bytes total)
                if (expected_size != SIMPLE_FRAME_SIZE_MANAGEMENT) {
                    ESP_LOGE(LOG_SIMPLE, "Invalid management frame size: %zu", expected_size);
                    return UART_EVENT_UNKNOWN;
                }

                // Command code is at byte 3
                uint8_t command = data[3];

                event->type = UART_EVENT_SIMPLE_MANAGEMENT;
                event->data.management.command = command;

                ESP_LOGD(LOG_SIMPLE, "Management event: command=0x%02X", command);
                return UART_EVENT_SIMPLE_MANAGEMENT;
            }

            case SIMPLE_PROTOCOL_TYPE_SENSOR: {
                // Sensor data frame (12 bytes total)
                if (expected_size != SIMPLE_FRAME_SIZE_SENSOR) {
                    ESP_LOGE(LOG_SIMPLE, "Invalid sensor frame size: %zu", expected_size);
                    return UART_EVENT_UNKNOWN;
                }

                // Sensor type at byte 3, data at bytes 4-7
                uint8_t sensor_type = data[3];
                uint8_t sensor_data[4];
                sensor_data[0] = data[4];
                sensor_data[1] = data[5];
                sensor_data[2] = data[6];
                sensor_data[3] = data[7];

                event->type = UART_EVENT_SIMPLE_SENSOR;
                event->data.sensor.sensor_type = sensor_type;
                memcpy(event->data.sensor.sensor_data, sensor_data, 4);

                ESP_LOGD(LOG_SIMPLE, "Sensor event: type=0x%02X, data=0x%02X%02X%02X%02X",
                         sensor_type, sensor_data[0], sensor_data[1],
                         sensor_data[2], sensor_data[3]);
                return UART_EVENT_SIMPLE_SENSOR;
            }

            default:
                ESP_LOGW(LOG_SIMPLE, "Unknown new frame type: 0x%02X", frame_type);
                return UART_EVENT_UNKNOWN;
        }
    }

    // Verify checksum
    uint8_t checksum = simple_protocol_calculate_checksum(data, len - 1);
    if (checksum != data[len - 1]) {
        ESP_LOGE(LOG_SIMPLE, "Checksum mismatch: expected 0x%02X, got 0x%02X", checksum, data[len - 1]);
        return UART_EVENT_UNKNOWN;
    }

    return UART_EVENT_UNKNOWN;
}

size_t simple_protocol_detect(const uint8_t* data, size_t len) {
    // Simple protocol detection logic with sliding window
    if (len < 2) {
        return 0;
    }

    // Search for frame header 0xAA 0x55 in the buffer
    for (size_t i = 0; i <= len - 2; i++) {
        if (data[i] == SIMPLE_PROTOCOL_NEW_START_BYTE1 && data[i + 1] == SIMPLE_PROTOCOL_NEW_START_BYTE2) {
            // Found header, check if we have at least frame type byte
            if (i + 2 < len) {
                uint8_t frame_type = data[i + 2];
                if (frame_type == SIMPLE_PROTOCOL_TYPE_HID ||
                    frame_type == SIMPLE_PROTOCOL_TYPE_MANAGEMENT ||
                    frame_type == SIMPLE_PROTOCOL_TYPE_SENSOR) {

                    // If we have enough data, check frame size and CRC
                    if (len - i >= 8) { // Minimum frame size for new protocol (management frame)
                        size_t expected_size = simple_protocol_get_expected_frame_size(data + i, len - i);
                        if (expected_size > 0 && (len - i) >= expected_size) {
                            // Verify CRC-16 if we have full frame
                            uint16_t crc_calculated = simple_protocol_calculate_crc16(&data[i + 2], expected_size - 6); // Type+payload
                            uint16_t crc_received = (data[i + expected_size - 4] << 8) | data[i + expected_size - 3]; // Before footer
                            if (crc_calculated == crc_received) {
                                ESP_LOGD(LOG_SIMPLE, "Detected new simple protocol with valid CRC");
                                return i + 1;
                            }
                        }
                    }
                    // Valid header and type, assume it's simple protocol
                    ESP_LOGD(LOG_SIMPLE, "Detected simple protocol header at offset %d", i);
                    return i + 1;
                }
            }
        }
    }

    return 0;
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