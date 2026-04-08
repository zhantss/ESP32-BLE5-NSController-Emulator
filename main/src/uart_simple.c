#include "uart_simple.h"
#include "hid.h"
#include "device.h"

#include "esp_log.h"
#include <string.h>

// Log tag
#define LOG_SIMPLE "simple_protocol"

// Calculate XOR checksum (same as in uart.c)
static uint8_t simple_protocol_calculate_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

// Calculate CRC-16-CCITT (polynomial 0x1021, initial value 0xFFFF)
static uint16_t simple_protocol_calculate_crc16(const uint8_t* data, size_t len) {
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

static int simple_init() {
    return 0;
}

static size_t simple_get_frame_header_size() {
    return 3;
}

static size_t simple_get_frame_size(const uint8_t* header, size_t len) {
    if (len < 3) {
        return 0;
    }

    // 0xAA 0x55
    if (header[0] == SIMPLE_PROTOCOL_NEW_START_BYTE1 
        && header[1] == SIMPLE_PROTOCOL_NEW_START_BYTE2) {
        uint8_t frame_type = header[2];
        switch (frame_type) {
            case SIMPLE_PROTOCOL_TYPE_HID:
                return SIMPLE_FRAME_SIZE_HID;    // 16 bytes
            case SIMPLE_PROTOCOL_TYPE_MANAGEMENT:
                return SIMPLE_FRAME_SIZE_MANAGEMENT;  // 8 bytes
            default:
                return 0; // Not a simple protocol frame
        }
    }

    return 0; // Not a simple protocol frame
}

static dev_uart_event_type_t simple_parse_frame(const uint8_t* frame_data, size_t len, dev_uart_event_t* event) {
    if (len < 4) {
        return UART_EVENT_UNKNOWN;
    }
    // check frame
    // 0xAA 0x55
    if (frame_data[0] != SIMPLE_PROTOCOL_NEW_START_BYTE1 
        || frame_data[1] != SIMPLE_PROTOCOL_NEW_START_BYTE2
        // 0x55 0xAA
        || frame_data[len - 2] != SIMPLE_PROTOCOL_NEW_END_BYTE1
        || frame_data[len - 1] != SIMPLE_PROTOCOL_NEW_END_BYTE2) {
        ESP_LOGE(LOG_SIMPLE, "Invalid Simple frame");
        return UART_EVENT_UNKNOWN;
    }

    // verify crc-16
    size_t data_len = len - 6; // exclude start(2), end(2) and crc(2)
    uint16_t crc_calculated = simple_protocol_calculate_crc16(frame_data, data_len);
    uint16_t crc_received = (frame_data[len - 4] << 8) | frame_data[len - 3];

    if (crc_calculated != crc_received) {
        ESP_LOGE(LOG_SIMPLE, "CRC mismatch: expected 0x%04X, got 0x%04X",
                     crc_calculated, crc_received);
        return UART_EVENT_UNKNOWN;
    }

    uint8_t frame_type = frame_data[2];
    switch (frame_type) {
        case SIMPLE_PROTOCOL_TYPE_HID: {
            if (len != SIMPLE_FRAME_SIZE_HID) {
                ESP_LOGE(LOG_SIMPLE, "Invalid HID frame size: %zu", len);
                return UART_EVENT_UNKNOWN;
            }

            uint8_t btn_bytes[3];
            btn_bytes[0] = frame_data[3];
            btn_bytes[1] = frame_data[4];
            btn_bytes[2] = frame_data[5];
            
            uint16_t left_x, left_y, right_x, right_y;
            unpack_stick_data(&frame_data[6], &left_x, &left_y);
            unpack_stick_data(&frame_data[9], &right_x, &right_y);

            event->type = UART_EVENT_SIMPLE_HID;
            memcpy(event->data.simple_hid.button_bytes, btn_bytes, 3);
            event->data.simple_hid.left_stick_x = left_x;
            event->data.simple_hid.left_stick_y = left_y;
            event->data.simple_hid.right_stick_x = right_x;
            event->data.simple_hid.right_stick_y = right_y;

            ESP_LOGD(LOG_SIMPLE, "Full HID event: buttons=0x%02X%02X%02X, L(%03X,%03X), R(%03X,%03X)",
                         btn_bytes[0], btn_bytes[1], btn_bytes[2],
                         left_x, left_y, right_x, right_y);
            return UART_EVENT_SIMPLE_HID;
        }
        case SIMPLE_PROTOCOL_TYPE_MANAGEMENT: {
            if (len != SIMPLE_FRAME_SIZE_MANAGEMENT) {
                ESP_LOGE(LOG_SIMPLE, "Invalid management frame size: %zu", len);
                return UART_EVENT_UNKNOWN;
            }

            uint8_t cmd = frame_data[3];
            event->type = UART_EVENT_SIMPLE_MANAGEMENT;
            event->data.management.command = cmd;
            ESP_LOGD(LOG_SIMPLE, "Management event: command=0x%02X", cmd);
            return UART_EVENT_SIMPLE_MANAGEMENT;
        }
        default:
            ESP_LOGW(LOG_SIMPLE, "Unknown new frame type: 0x%02X", frame_type);
            return UART_EVENT_UNKNOWN;
    }
}

static int simple_process_hid_event(hid_device_report_t* buffer, simple_hid_event_t* simple_hid) {
    const hid_device_ops_t* ops = hid_get_device_ops(g_dev_controller.type);
    // set button
    ops->set_button_custom(buffer, (uint8_t*)simple_hid->button_bytes, 3);
    // set stick 
    ops->set_left_stick(buffer, simple_hid->left_stick_x, simple_hid->left_stick_y);
    ops->set_right_stick(buffer, simple_hid->right_stick_x, simple_hid->right_stick_y);
    return 0;
}

static int simple_process_management_event(simple_management_event_t* simple_management) {
    // TODO: implement management event processing
    ESP_LOGD(LOG_SIMPLE, "Management event: command=0x%02X", simple_management->command);
    return 0;
}

static int simple_process_event(hid_device_report_t* buffer, dev_uart_event_t* event, dev_uart_event_rsp_t* rsp) {
    switch (event->type) {
        case UART_EVENT_SIMPLE_HID: {
            // no uart response
            rsp->len = 0;
            rsp->data = NULL;
            return simple_process_hid_event(buffer, &event->data.simple_hid);
        }
        case UART_EVENT_SIMPLE_MANAGEMENT: {
            // TODO simple command response
            return simple_process_management_event(&event->data.management);
        }
        default: {
            ESP_LOGE(LOG_SIMPLE, "Unknown event type: %d", event->type);
            return -1;
        }
    }
    return -1;
}

static void simple_set_debug(bool enabled) {
    ESP_LOGD(LOG_SIMPLE, "Debug mode %s", enabled ? "enabled" : "disabled");
}

const uart_protocol_impl_t simple_protocol_impl = {
    .protocol = UART_PROTOCOL_SIMPLE,
    .init = simple_init,
    .get_frame_header_size = simple_get_frame_header_size,
    .get_frame_size = simple_get_frame_size,
    .parse_frame = simple_parse_frame,
    .process_event = simple_process_event,
    .set_debug = simple_set_debug,
};