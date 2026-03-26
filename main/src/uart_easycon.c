#include "uart_easycon.h"
#include "uart.h"
#include "hid.h"
#include "hid_pro2.h"
#include "device.h"

#include "esp_log.h"
#include <string.h>

// Log tag
#define LOG_EASYCON "easycon_protocol"

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
const size_t BUTTON_MAP_SIZE = 16; // 16 non-direction, non-C buttons

// 7-bit packed decoding implementation
static void ec_decode_7bit_packed(const uint8_t* encoded, uint8_t* decoded, size_t decoded_len) {
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
static void ec_encode_7bit_packed(const uint8_t* decoded, uint8_t* encoded, size_t decoded_len) {
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
static uint16_t ec_scale_stick_value(uint8_t easycon_value) {
    // Shift left 4 bits = multiply by 16
    // 255 * 16 = 4080 (error 15/4095 ≈ 0.37%)
    // Center: 128 * 16 = 2048 (perfect match)
    return (uint16_t)easycon_value << 4;
}

static size_t ec_get_frame_header_size() {
    // no header
    return 0;
}

static size_t ec_get_frame_size(const uint8_t* header, size_t len) {
    (void)header;   // Unused parameter
    (void)len;      // Unused parameter
    // EasyCon protocol always uses 8-byte encoded frames
    // buttons(2), hat(1), sticks(4), end(1)
    return EASYCON_PROTOCOL_ENCODED_SIZE;
}

static dev_uart_event_type_t ec_parse_frame(const uint8_t* frame_data, size_t len, dev_uart_event_t* event) {
    if (len < EASYCON_PROTOCOL_ENCODED_SIZE 
        || (frame_data[len - 1] & EASYCON_PROTOCOL_END_MARKER) != 0) {
        return UART_EVENT_UNKNOWN;
    }

    uint8_t raw_data[EASYCON_PROTOCOL_RAW_SIZE];
    ec_decode_7bit_packed(frame_data, raw_data, EASYCON_PROTOCOL_RAW_SIZE);

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
    uint16_t left_stick_x = ec_scale_stick_value(lx_raw);
    uint16_t left_stick_y = ec_scale_stick_value(ly_raw);
    uint16_t right_stick_x = ec_scale_stick_value(rx_raw);
    uint16_t right_stick_y = ec_scale_stick_value(ry_raw);

    event->type = UART_EVENT_EC_HID;
    event->data.ec_hid.button_mask = button_mask;
    event->data.ec_hid.hat_state = hat_state;
    event->data.ec_hid.left_stick_x = left_stick_x;
    event->data.ec_hid.left_stick_y = left_stick_y;
    event->data.ec_hid.right_stick_x = right_stick_x;
    event->data.ec_hid.right_stick_y = right_stick_y;

    ESP_LOGD(LOG_EASYCON, "HID event: buttons=0x%04X, hat=0x%02X, LX=0x%03X, LY=0x%03X, RX=0x%03X, RY=0x%03X",
             button_mask, hat_state, left_stick_x, left_stick_y, right_stick_x, right_stick_y);

    return UART_EVENT_EC_HID;
}

static int ec_process_event(hid_device_report_t* buffer, dev_uart_event_t* event) {
    const hid_device_ops_t* ops = hid_get_device_ops(g_dev_controller.type);
    // buttons
    uint16_t button_mask = event->data.ec_hid.button_mask;
    for (int i = 0; i < BUTTON_MAP_SIZE; i++) {
        pro2_btns btn = button_map[i];
        bool pressed = (button_mask & (1 << i)) != 0;
        ops->set_button(buffer, btn, pressed);
    }

    // hat
    uint8_t direction = event->data.ec_hid.hat_state & 0x0F;
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
    ops->set_button(buffer, Up, up_pressed);
    ops->set_button(buffer, Down, down_pressed);
    ops->set_button(buffer, Left, left_pressed);
    ops->set_button(buffer, Right, right_pressed);

    // stick
    ops->set_left_stick(buffer, event->data.ec_hid.left_stick_x, event->data.ec_hid.left_stick_y);
    ops->set_right_stick(buffer, event->data.ec_hid.right_stick_x, event->data.ec_hid.right_stick_y);

    return 0;
}

static void ec_set_debug(bool enabled) {
    ESP_LOGD(LOG_EASYCON, "Debug mode %s", enabled ? "enabled" : "disabled");
}

const uart_protocol_impl_t easycon_protocol_impl = {
    .protocol = UART_PROTOCOL_EASYCON,
    .get_frame_header_size = ec_get_frame_header_size,
    .get_frame_size = ec_get_frame_size,
    .parse_frame = ec_parse_frame,
    .process_event = ec_process_event,
    .set_debug = ec_set_debug,
};