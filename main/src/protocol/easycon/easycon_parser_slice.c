#include "protocol/easycon/easycon_protocol.h"

#ifdef CONFIG_PROTOCOL_LAYER_EASYCON

#include "buffer/zc_buffer.h"
#include "utils.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define PARSER_NAME "easycon_slice"

static void easycon_slice_reset(void *state)
{
    easycon_slice_state_t *st = (easycon_slice_state_t *)state;
    st->pending_code  = 0;
    st->pending_index = 0;
    st->pending_len   = 0;
}

static bool easycon_slice_probe(void *state,
                                uint8_t *head_ptr, uint32_t head_len,
                                uint8_t *wrap_ptr, uint32_t wrap_len)
{
    easycon_slice_state_t *st = (easycon_slice_state_t *)state;

    /* Continuation of a previously accepted slice header */
    if (st->pending_code != 0) {
        return true;
    }

    if (peek_byte(head_ptr, head_len, wrap_ptr, wrap_len, 0) != EASYCON_CMD_READY) {
        return false;
    }

    uint8_t cmd = peek_byte(head_ptr, head_len, wrap_ptr, wrap_len, 5);
    switch (cmd) {
        case EASYCON_CMD_FLASH:
        case EASYCON_CMD_CHANGE_CONTROLLER_COLOR:
        case EASYCON_CMD_SAVE_AMIIBO:
            return true;
        default:
            return false;
    }
}

static uint8_t s_slice_rsp = EASYCON_RPY_ACK;

static parse_result_t easycon_slice_parse_frame(void *state,
                                                zc_ringbuf_t *rb,
                                                parser_rsp_t *rsp)
{
    easycon_slice_state_t *st = (easycon_slice_state_t *)state;

    /* Case 1: continuation data packet */
    if (st->pending_code != 0) {
        uint16_t len = st->pending_len;
        if (len == 0 || len > EASYCON_PROTOCOL_SLICE_MAX_SIZE) {
            easycon_slice_reset(state);
            return PARSE_INVALID;
        }

        uint8_t slice_buf[EASYCON_PROTOCOL_SLICE_MAX_SIZE];
        uint32_t n = zc_read_bulk(rb, slice_buf, len);
        if (n != len) {
            return PARSE_NEED_MORE;
        }

        /* TODO: implement actual Flash / Color / Amiibo device logic here */
        (void)slice_buf;

        easycon_slice_reset(state);

        s_slice_rsp = EASYCON_RPY_ACK;
        rsp->data = &s_slice_rsp;
        rsp->len  = 1;
        return PARSE_OK;
    }

    /* Case 2: slice command header */
    uint8_t frame[EASYCON_PROTOCOL_SLICE_CMD_SIZE];
    uint32_t n = zc_read_bulk(rb, frame, EASYCON_PROTOCOL_SLICE_CMD_SIZE);
    if (n != EASYCON_PROTOCOL_SLICE_CMD_SIZE) {
        return PARSE_INVALID;
    }

    uint16_t index = (frame[1] & 0x7FU) | ((uint16_t)frame[2] << 7);
    uint16_t len   = (frame[3] & 0x7FU) | ((uint16_t)frame[4] << 7);
    uint8_t  cmd   = frame[5];

    st->pending_code  = cmd;
    st->pending_index = index;
    st->pending_len   = len;

    s_slice_rsp = EASYCON_RPY_ACK;
    rsp->data = &s_slice_rsp;
    rsp->len  = 1;
    return PARSE_OK;
}

const protocol_parser_ops_t easycon_parser_slice_ops = {
    .name         = PARSER_NAME,
    .min_peek_len = EASYCON_PROTOCOL_SLICE_CMD_SIZE,
    .reset        = easycon_slice_reset,
    .probe        = easycon_slice_probe,
    .parse_frame  = easycon_slice_parse_frame,
};

#endif // CONFIG_PROTOCOL_LAYER_EASYCON
