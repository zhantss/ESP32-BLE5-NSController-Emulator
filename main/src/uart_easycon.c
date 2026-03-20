#include "uart_easycon.h"
#include "uart.h"
#include "hid.h"
#include "device.h"

#include "esp_log.h"
#include <string.h>

// Log tag
#define LOG_EASYCON "easycon_protocol"

// 7-bit packed decoding implementation
void easycon_decode_7bit_packed(const uint8_t* encoded, uint8_t* decoded, size_t decoded_len) {
    uint32_t buffer = 0;
    int bits_available = 0;
    int decoded_idx = 0;

    for (int i = 0; i < 8; i++) {
        uint8_t byte = encoded[i];
        buffer |= ((uint32_t)byte) << bits_available;
        bits_available += 7;

        while (bits_available >= 8 && decoded_idx < decoded_len) {
            decoded[decoded_idx++] = buffer & 0xFF;
            buffer >>= 8;
            bits_available -= 8;
        }
    }
}

// 7-bit packed encoding implementation
void easycon_encode_7bit_packed(const uint8_t* decoded, uint8_t* encoded, size_t decoded_len) {
    uint32_t buffer = 0;
    int bits_used = 0;
    int encoded_idx = 0;

    for (int i = 0; i < decoded_len; i++) {
        buffer |= ((uint32_t)decoded[i]) << bits_used;
        bits_used += 8;

        while (bits_used >= 7) {
            encoded[encoded_idx++] = buffer & 0x7F;
            buffer >>= 7;
            bits_used -= 7;
        }
    }

    if (bits_used > 0) {
        encoded[encoded_idx++] = buffer & 0x7F;
    }

    // Set bit7=1 on last byte as end marker
    if (encoded_idx > 0) {
        encoded[encoded_idx - 1] |= 0x80;
    }
}

// Scale EasyCon stick value (0-255) to HID 12-bit value (0-4095)
uint16_t easycon_scale_stick_value(uint8_t easycon_value) {
    // Shift left 4 bits = multiply by 16
    // 255 * 16 = 4080 (error 15/4095 ≈ 0.37%)
    // Center: 128 * 16 = 2048 (perfect match)
    return (uint16_t)easycon_value << 4;
}

size_t easycon_protocol_get_expected_frame_size(const uint8_t* data, size_t len) {
    (void)data; // Unused parameter
    (void)len;  // Unused parameter

    // EasyCon protocol always uses 8-byte encoded frames
    return EASYCON_PROTOCOL_ENCODED_SIZE;
}

dev_uart_event_type_t easycon_protocol_parse_frame(const uint8_t* data, size_t len, dev_uart_event_t* event) {
    if (len < EASYCON_PROTOCOL_ENCODED_SIZE) {
        return UART_EVENT_UNKNOWN;
    }

    // Decode 8-byte encoded data to 7-byte raw data
    uint8_t raw_data[EASYCON_PROTOCOL_RAW_SIZE];
    easycon_decode_7bit_packed(data, raw_data, EASYCON_PROTOCOL_RAW_SIZE);

    // Parse raw data
    uint8_t button_byte0 = raw_data[0];  // pro2_btn_bits_t byte0 buttons
    uint8_t button_byte1 = raw_data[1];  // pro2_btn_bits_t byte1 non-direction buttons + byte2 buttons
    uint8_t hat_state = raw_data[2];     // HAT direction state
    uint8_t lx_raw = raw_data[3];        // Left stick X
    uint8_t ly_raw = raw_data[4];        // Left stick Y
    uint8_t rx_raw = raw_data[5];        // Right stick X
    uint8_t ry_raw = raw_data[6];        // Right stick Y

    // Combine button bytes (little-endian: byte0 in lower bits, byte1 in higher bits)
    uint16_t button_mask = button_byte0 | ((uint16_t)button_byte1 << 8);

    // Scale stick values
    uint16_t left_stick_x = easycon_scale_stick_value(lx_raw);
    uint16_t left_stick_y = easycon_scale_stick_value(ly_raw);
    uint16_t right_stick_x = easycon_scale_stick_value(rx_raw);
    uint16_t right_stick_y = easycon_scale_stick_value(ry_raw);

    // Fill HID event
    event->type = UART_EVENT_HID;
    event->data.hid.button_mask = button_mask;
    event->data.hid.hat_state = hat_state;
    event->data.hid.left_stick_x = left_stick_x;
    event->data.hid.left_stick_y = left_stick_y;
    event->data.hid.right_stick_x = right_stick_x;
    event->data.hid.right_stick_y = right_stick_y;

    ESP_LOGD(LOG_EASYCON, "HID event: buttons=0x%04X, hat=0x%02X, LX=0x%03X, LY=0x%03X, RX=0x%03X, RY=0x%03X",
             button_mask, hat_state, left_stick_x, left_stick_y, right_stick_x, right_stick_y);

    return UART_EVENT_HID;
}

bool easycon_protocol_detect(const uint8_t* data, size_t len) {
    // Need at least 8 bytes to detect
    if (len < EASYCON_PROTOCOL_ENCODED_SIZE) {
        return false;
    }

    // Check if last byte has bit7=1 (end marker)
    if ((data[7] & 0x80) == 0) {
        return false;
    }

    // Check first 7 bytes have bit7=0 (7-bit data)
    for (int i = 0; i < 7; i++) {
        if ((data[i] & 0x80) != 0) {
            return false;
        }
    }

    // Try to decode and validate HAT value
    uint8_t raw_data[EASYCON_PROTOCOL_RAW_SIZE];
    easycon_decode_7bit_packed(data, raw_data, EASYCON_PROTOCOL_RAW_SIZE);

    uint8_t hat = raw_data[2];
    if (hat > HAT_CENTER) {
        return false;
    }

    ESP_LOGD(LOG_EASYCON, "Detected EasyCon protocol with valid frame");
    return true;
}

// Protocol initialization (no-op for EasyCon protocol)
static void easycon_protocol_init(void) {
    ESP_LOGD(LOG_EASYCON, "EasyCon protocol initialized");
}

// Protocol deinitialization (no-op for EasyCon protocol)
static void easycon_protocol_deinit(void) {
    ESP_LOGD(LOG_EASYCON, "EasyCon protocol deinitialized");
}

// Get protocol version
static const char* easycon_protocol_get_version(void) {
    return "1.0";
}

// Set debug mode
static void easycon_protocol_set_debug(bool enabled) {
    ESP_LOGD(LOG_EASYCON, "Debug mode %s", enabled ? "enabled" : "disabled");
}

// EasyCon protocol implementation structure
const uart_protocol_impl_t easycon_protocol_impl = {
    .protocol = UART_PROTOCOL_EASYCON,
    .name = "EasyCon Protocol",
    .parse_frame = easycon_protocol_parse_frame,
    .detect_protocol = easycon_protocol_detect,
    .get_expected_frame_size = easycon_protocol_get_expected_frame_size,
    .init_protocol = easycon_protocol_init,
    .deinit_protocol = easycon_protocol_deinit,
    .get_version = easycon_protocol_get_version,
    .set_debug = easycon_protocol_set_debug
};