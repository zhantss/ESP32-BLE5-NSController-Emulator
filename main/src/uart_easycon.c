#include "uart_easycon.h"
#include "uart.h"
#include "hid.h"
#include "hid_pro2.h"
#include "device.h"

#include "esp_log.h"
#include <string.h>

// Log tag
#define LOG_EASYCON "easycon_protocol"

easycon_protocol_state_t ec_state = EC_IDLE;
ec_cmd_slice_event_t ec_current_slice_event = {0};

static uint8_t hello_rsp[] = { EASYCON_RPY_HELLO };
static uint8_t mcu_version[] = { 0x77 };
static uint8_t ack_rsp[] = { EASYCON_RPY_ACK };
static uint8_t led_rsp[] = { 0x00 };
static uint8_t script_rsp[] = { EASYCON_RPY_SCRIPT_ACK };

const pro2_btns button_map[] = {
    // Map EasyCon button bits (0-13) to pro2_btns enum values
    // EasyCon bit 0-7 are in button_mask low byte, bit 8-13 in high byte
    Y,      // ID 0: EasyCon bit 0 -> Y
    B,      // ID 1: EasyCon bit 1 -> B
    A,      // ID 2: EasyCon bit 2 -> A
    X,      // ID 3: EasyCon bit 3 -> X
    L,      // ID 4: EasyCon bit 4 -> L
    R,      // ID 5: EasyCon bit 5 -> R
    ZL,     // ID 6: EasyCon bit 6 -> ZL
    ZR,     // ID 7: EasyCon bit 7 -> ZR
    Minus,  // ID 8: EasyCon bit 8 -> Minus
    Plus,   // ID 9: EasyCon bit 9 -> Plus
    LClick, // ID 10: EasyCon bit 10 -> LClick
    RClick, // ID 11: EasyCon bit 11 -> RClick
    Home,   // ID 12: EasyCon bit 12 -> Home
    Capture,// ID 13: EasyCon bit 13 -> Capture

    // Note: EasyCon does not have GR/GL buttons
    // Direction buttons (Down,Right,Left,Up) are handled separately via hat_state
};
const size_t BUTTON_MAP_SIZE = 14; // 14 buttons defined in EasyCon protocol (bit 0-13)

// 7-bit packed decoding implementation
// Matches C# encoder: data added to low bits, extracted from high bits
static void ec_decode_7bit_packed(const uint8_t* encoded, uint8_t* decoded, size_t decoded_len) {
    uint32_t buffer = 0;
    int bits_available = 0;
    int encoded_idx = 0;

    for (int i = 0; i < decoded_len; i++) {
        // Ensure we have at least 8 bits in buffer
        while (bits_available < 8) {
            uint8_t byte = encoded[encoded_idx++];
            // Add new 7-bit data to low bits (matching encoder's bit order)
            buffer = (buffer << 7) | (byte & 0x7F);
            bits_available += 7;
        }
        // Extract 8 bits from high bits of buffer
        bits_available -= 8;
        decoded[i] = (buffer >> bits_available) & 0xFF;
        // Clear used bits
        buffer &= (1U << bits_available) - 1;
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

static int ec_init() {
    #ifdef CONFIG_MCU_DEBUG
        esp_log_level_set(LOG_EASYCON, ESP_LOG_DEBUG);
    #endif
    return 0;
}

static size_t ec_get_frame_header_size() {
    // Judge in get_frame_size
    return 2;
}

static size_t ec_get_frame_size(const uint8_t* header, size_t len) {
    if (len != 2) return 0;
    if (ec_current_slice_event.code != 0) {
        // TODO flash color amiibo
        return ec_current_slice_event.len;
    }
    if (header[0] == EASYCON_CMD_READY) {
        // HELLO
        if (header[1] == EASYCON_CMD_READY) {
            // hello and heartbeat -> 3
            return EASYCON_PROTOCOL_HELLO_SIZE;
        }
        
        // if (header[2] == EASYCON_CMD_CHANGE_CONTROLLER_MODE 
        //     || header[2] == EASYCON_CMD_CHANGE_AMIIBO_INDEX) {
        //     return EASYCON_PROTOCOL_SIMPLE_CMD_SIZE;
        // }

        if (header[1] == EASYCON_CMD_SCRIPT_START 
            || header[1] == EASYCON_CMD_SCRIPT_STOP
            || header[1] == EASYCON_CMD_VERSION
            || header[1] == EASYCON_CMD_LED
            || header[1] == EASYCON_CMD_UNPAIR) {
            return EASYCON_PROTOCOL_SHORT_CMD_SIZE;
        }
        // flash color amiibo
        // Ready | i & 0x7F | i >> 7 | len & 0x7F | len >> 7 | CMD_TAG
        return EASYCON_PROTOCOL_SLICE_CMD_SIZE;
    } else {
        // HID
        // buttons(2) | hat(1) | sticks(4) | END_MARKER(1)
        return EASYCON_PROTOCOL_ENCODED_SIZE;
    }
}

static dev_uart_event_type_t ec_parse_frame(const uint8_t* frame_data, size_t len, dev_uart_event_t* event) {
    ESP_LOGD(LOG_EASYCON, "ec_parse_frame: len=%d", len);
    if (ec_current_slice_event.code != 0) {
        ESP_LOGD(LOG_EASYCON, "ec_parse_frame: slice event");
        event->type = UART_EVENT_EC_CMD_SLICE_DATA;
        event->data.ec_cmd_slice_data.cmd.code = ec_current_slice_event.code;
        event->data.ec_cmd_slice_data.cmd.index = ec_current_slice_event.index;
        event->data.ec_cmd_slice_data.cmd.len = ec_current_slice_event.len;
        event->data.ec_cmd_slice_data.data = malloc(event->data.ec_cmd_slice_data.cmd.len);
        if (event->data.ec_cmd_slice_data.data == NULL) {
            // memory allocation failed
            return UART_EVENT_ERROR;
        }
        // copy data from frame buffer
        // !! NOT CAST POINTER
        memcpy(event->data.ec_cmd_slice_data.data, 
            frame_data, event->data.ec_cmd_slice_data.cmd.len);

        ec_current_slice_event.code = 0; // reset
        return UART_EVENT_EC_CMD_SLICE_DATA;
    }

    if (len == EASYCON_PROTOCOL_HELLO_SIZE || len == EASYCON_PROTOCOL_SIMPLE_CMD_SIZE) {
        ESP_LOGD(LOG_EASYCON, "ec_parse_frame: hello or simple cmd");
        event->type = UART_EVENT_EC_CMD;
        if (frame_data[2] == EASYCON_CMD_HELLO) {
            // hello
            event->data.ec_cmd.code = EASYCON_CMD_HELLO;
            event->data.ec_cmd.data = 0;
        } else {
            // simple command
            switch(frame_data[2]) {
                case EASYCON_CMD_CHANGE_CONTROLLER_MODE:
                    event->data.ec_cmd.code = EASYCON_CMD_CHANGE_CONTROLLER_MODE;
                    event->data.ec_cmd.data = frame_data[1];
                    break;
                case EASYCON_CMD_CHANGE_AMIIBO_INDEX:
                    event->data.ec_cmd.code = EASYCON_CMD_CHANGE_AMIIBO_INDEX;
                    event->data.ec_cmd.data = frame_data[1];
                    break;
                default:
                    return UART_EVENT_UNKNOWN;
            }
        }
        return UART_EVENT_EC_CMD;
    }

    if (len == EASYCON_PROTOCOL_SHORT_CMD_SIZE) {
        ESP_LOGD(LOG_EASYCON, "ec_parse_frame: short cmd");
        event->type = UART_EVENT_EC_CMD;
        event->data.ec_cmd.code = frame_data[1];
        event->data.ec_cmd.data = 0;
        return UART_EVENT_EC_CMD;
    }

    if (len == EASYCON_PROTOCOL_SLICE_CMD_SIZE) {
        ESP_LOGD(LOG_EASYCON, "ec_parse_frame: slice cmd");
        // flash color amiibo
        event->data.ec_cmd_slice.index = (frame_data[1] & 0x7F) | frame_data[2];
        event->data.ec_cmd_slice.len = (frame_data[3] & 0x7F) | frame_data[4];
        switch(frame_data[EASYCON_PROTOCOL_SLICE_CMD_SIZE - 1]) {
            case EASYCON_CMD_FLASH:
                event->data.ec_cmd_slice.code = EASYCON_CMD_FLASH;
                break;
            case EASYCON_CMD_CHANGE_CONTROLLER_COLOR:
                event->data.ec_cmd_slice.code = EASYCON_CMD_CHANGE_CONTROLLER_COLOR;
                break;
            case EASYCON_CMD_SAVE_AMIIBO:
                event->data.ec_cmd_slice.code = EASYCON_CMD_SAVE_AMIIBO;
                break;
            default:
                return UART_EVENT_UNKNOWN;
        }
        return UART_EVENT_EC_CMD_SLICE;
    }

    if (len < EASYCON_PROTOCOL_ENCODED_SIZE
        || (frame_data[len - 1] & EASYCON_PROTOCOL_END_MARKER) == 0) {
        ESP_LOGE(LOG_EASYCON, "Invalid frame size or end marker: len=%d, end_marker=0x%02X",
            len, frame_data[len - 1] & EASYCON_PROTOCOL_END_MARKER);
        return UART_EVENT_UNKNOWN;
    }
    ESP_LOGD(LOG_EASYCON, "ec_parse_frame: HID");

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

    // Combine button bytes (big-endian from PC: byte0 in higher bits, byte1 in lower bits)
    uint16_t button_mask = ((uint16_t)button_byte0 << 8) | button_byte1;
    // Scale stick values
    // Note: Y-axis is inverted between EasyCon (0=up, 255=down) and HID (0=up, 4095=down)
    // But user reports direction is reversed, so EasyCon may use (0=down, 255=up)
    // Invert Y values: 255 - raw_value
    uint16_t left_stick_x = ec_scale_stick_value(lx_raw);
    uint16_t left_stick_y = ec_scale_stick_value(255 - ly_raw);
    uint16_t right_stick_x = ec_scale_stick_value(rx_raw);
    uint16_t right_stick_y = ec_scale_stick_value(255 - ry_raw);

    event->type = UART_EVENT_EC_HID;
    event->data.ec_hid.button_mask = button_mask;
    event->data.ec_hid.hat_state = hat_state;
    event->data.ec_hid.left_stick_x = left_stick_x;
    event->data.ec_hid.left_stick_y = left_stick_y;
    event->data.ec_hid.right_stick_x = right_stick_x;
    event->data.ec_hid.right_stick_y = right_stick_y;

    ESP_LOGD(LOG_EASYCON, "HID event: buttons=0x%04X, hat=0x%02X, LX=%d, LY=%d, RX=%d, RY=%d",
             button_mask, hat_state, left_stick_x, left_stick_y, right_stick_x, right_stick_y);
    

    return UART_EVENT_EC_HID;
}

static int ec_process_event(hid_device_report_t* buffer, dev_uart_event_t* event, dev_uart_event_rsp_t* rsp) {
    const hid_device_ops_t* ops = hid_get_device_ops(g_dev_controller.type);

    switch (event->type) {
        case UART_EVENT_EC_HID:
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

            // no uart response
            rsp->len = 0;
            rsp->data = NULL;
            return 0;
        case UART_EVENT_EC_CMD:
            switch(event->data.ec_cmd.code) {
                case EASYCON_CMD_HELLO:
                    ESP_LOGD(LOG_EASYCON, "Received Hello command");
                    rsp->len = 1;
                    rsp->data = hello_rsp;
                    return 0;
                case EASYCON_CMD_VERSION:
                    rsp->len = 1;
                    rsp->data = mcu_version;
                    return 0;
                // TODO Device Logic
                case EASYCON_CMD_LED:
                    rsp->len = 1;
                    rsp->data = led_rsp;
                    return 0;
                case EASYCON_CMD_UNPAIR:
                case EASYCON_CMD_CHANGE_CONTROLLER_MODE:
                case EASYCON_CMD_CHANGE_AMIIBO_INDEX:
                    rsp->len = 1;
                    rsp->data = ack_rsp;
                    return 0;
                case EASYCON_CMD_SCRIPT_START:
                case EASYCON_CMD_SCRIPT_STOP:
                    rsp->len = 1;
                    rsp->data = script_rsp;
                    return 0;
                default:
                    return -1;
            }
            return 0;
        // TODO Slice CMD
        case UART_EVENT_EC_CMD_SLICE:
        case UART_EVENT_EC_CMD_SLICE_DATA:
            rsp->len = 1;
            rsp->data = ack_rsp;
            return 0;
        default:
            return -1;
    }
    return -1;
}

static void ec_set_debug(bool enabled) {
    ESP_LOGD(LOG_EASYCON, "Debug mode %s", enabled ? "enabled" : "disabled");
}

const uart_protocol_impl_t easycon_protocol_impl = {
    .protocol = UART_PROTOCOL_EASYCON,
    .init = ec_init,
    .get_frame_header_size = ec_get_frame_header_size,
    .get_frame_size = ec_get_frame_size,
    .parse_frame = ec_parse_frame,
    .process_event = ec_process_event,
    .set_debug = ec_set_debug,
};