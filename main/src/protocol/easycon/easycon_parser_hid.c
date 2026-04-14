#include "protocol/easycon/easycon_protocol.h"

#ifdef CONFIG_PROTOCOL_LAYER_EASYCON

#include "buffer/zc_buffer.h"
#include "controller/hid_controller.h"
#include "controller/hid_controller_pro2.h"
#include "device.h"
#include "utils.h"

#include <stdint.h>
#include <stdbool.h>

#define PARSER_NAME "easycon_hid"

static void easycon_hid_reset(void *state)
{
    (void)state;
}

static bool easycon_hid_probe(void *state,
                              uint8_t *head_ptr, uint32_t head_len,
                              uint8_t *wrap_ptr, uint32_t wrap_len)
{
    (void)state;
    /* HID frame: fixed 8 bytes, last byte must have bit7 set */
    uint8_t end_marker = peek_byte(head_ptr, head_len, wrap_ptr, wrap_len,
                                   EASYCON_PROTOCOL_ENCODED_SIZE - 1);
    return (end_marker & EASYCON_PROTOCOL_END_MARKER) != 0;
}

/* 7-bit packed decoding — ported from legacy uart_easycon.c */
static void ec_decode_7bit_packed(const uint8_t *encoded, uint8_t *decoded, size_t decoded_len)
{
    uint32_t buffer = 0;
    int bits_available = 0;
    int encoded_idx = 0;

    for (size_t i = 0; i < decoded_len; i++) {
        while (bits_available < 8) {
            uint8_t byte = encoded[encoded_idx++];
            buffer = (buffer << 7) | (byte & 0x7FU);
            bits_available += 7;
        }
        bits_available -= 8;
        decoded[i] = (buffer >> bits_available) & 0xFFU;
        buffer &= (1U << bits_available) - 1;
    }
}

static uint16_t ec_scale_stick_value(uint8_t easycon_value)
{
    return (uint16_t)easycon_value << 4;
}

static const btns_pro2 button_map[] = {
    Y, B, A, X, L, R, ZL, ZR, Minus, Plus, LClick, RClick, Home, Capture
};
static const size_t BUTTON_MAP_SIZE = 14;

static parse_result_t easycon_hid_parse_frame(void *state,
                                              zc_ringbuf_t *rb,
                                              parser_rsp_t *rsp)
{
    (void)state;
    uint8_t encoded[EASYCON_PROTOCOL_ENCODED_SIZE];
    uint32_t n = zc_read_bulk(rb, encoded, EASYCON_PROTOCOL_ENCODED_SIZE);
    if (n != EASYCON_PROTOCOL_ENCODED_SIZE) {
        return PARSE_INVALID;
    }

    if ((encoded[EASYCON_PROTOCOL_ENCODED_SIZE - 1] & EASYCON_PROTOCOL_END_MARKER) == 0) {
        return PARSE_INVALID;
    }

    uint8_t raw[EASYCON_PROTOCOL_RAW_SIZE];
    ec_decode_7bit_packed(encoded, raw, EASYCON_PROTOCOL_RAW_SIZE);

    uint8_t button_byte0 = raw[0];
    uint8_t button_byte1 = raw[1];
    uint8_t hat_state    = raw[2];
    uint8_t lx_raw       = raw[3];
    uint8_t ly_raw       = raw[4];
    uint8_t rx_raw       = raw[5];
    uint8_t ry_raw       = raw[6];

    uint16_t button_mask = ((uint16_t)button_byte0 << 8) | button_byte1;
    uint16_t left_stick_x  = ec_scale_stick_value(lx_raw);
    uint16_t left_stick_y  = ec_scale_stick_value(255 - ly_raw);
    uint16_t right_stick_x = ec_scale_stick_value(rx_raw);
    uint16_t right_stick_y = ec_scale_stick_value(255 - ry_raw);

    const controller_hid_ops_t *ops = g_controller.hid_ops;
    controller_hid_report_t *back_buffer = g_controller.ops->get_back_buffer(&g_controller);
    if (ops && back_buffer) {
        for (size_t i = 0; i < BUTTON_MAP_SIZE; i++) {
            btns_pro2 btn = button_map[i];
            bool pressed = (button_mask & (1U << i)) != 0;
            ops->set_button(back_buffer, btn, pressed);
        }

        uint8_t direction = hat_state & 0x0FU;
        bool up_pressed    = false;
        bool down_pressed  = false;
        bool left_pressed  = false;
        bool right_pressed = false;

        switch (direction) {
            case HAT_UP:         up_pressed = true; break;
            case HAT_UP_RIGHT:   up_pressed = true; right_pressed = true; break;
            case HAT_RIGHT:      right_pressed = true; break;
            case HAT_DOWN_RIGHT: down_pressed = true; right_pressed = true; break;
            case HAT_DOWN:       down_pressed = true; break;
            case HAT_DOWN_LEFT:  down_pressed = true; left_pressed = true; break;
            case HAT_LEFT:       left_pressed = true; break;
            case HAT_UP_LEFT:    up_pressed = true; left_pressed = true; break;
            case HAT_CENTER:
            default:
                break;
        }

        ops->set_button(back_buffer, Up,    up_pressed);
        ops->set_button(back_buffer, Down,  down_pressed);
        ops->set_button(back_buffer, Left,  left_pressed);
        ops->set_button(back_buffer, Right, right_pressed);

        ops->set_left_stick(back_buffer, left_stick_x, left_stick_y);
        ops->set_right_stick(back_buffer, right_stick_x, right_stick_y);

        /* TODO: After introducing HID bridge, replace direct back_buffer access
         * with bridge API or mark rsp as HID packet so the router can set
         * swap_request uniformly. Currently the caller must call
         * g_controller.ops->request_swap(&g_controller) after protocol_route(). */
    }

    rsp->len = 0;
    return PARSE_OK;
}

const protocol_parser_ops_t easycon_parser_hid_ops = {
    .name         = PARSER_NAME,
    .min_peek_len = EASYCON_PROTOCOL_ENCODED_SIZE,
    .reset        = easycon_hid_reset,
    .probe        = easycon_hid_probe,
    .parse_frame  = easycon_hid_parse_frame,
};

#endif // CONFIG_PROTOCOL_LAYER_EASYCON
