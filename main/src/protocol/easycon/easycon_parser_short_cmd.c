#include "protocol/easycon/easycon_protocol.h"

#ifdef CONFIG_PROTOCOL_LAYER_EASYCON

#include "buffer/zc_buffer.h"
#include "utils.h"

#include <stdint.h>
#include <stdbool.h>

#define PARSER_NAME "easycon_short_cmd"

static void easycon_short_cmd_reset(void *state)
{
    (void)state;
}

static bool easycon_short_cmd_probe(void *state,
                                    uint8_t *head_ptr, uint32_t head_len,
                                    uint8_t *wrap_ptr, uint32_t wrap_len)
{
    (void)state;

    if (peek_byte(head_ptr, head_len, wrap_ptr, wrap_len, 0) != EASYCON_CMD_READY) {
        return false;
    }

    uint8_t cmd = peek_byte(head_ptr, head_len, wrap_ptr, wrap_len, 1);
    switch (cmd) {
        case EASYCON_CMD_SCRIPT_START:
        case EASYCON_CMD_SCRIPT_STOP:
        case EASYCON_CMD_VERSION:
        case EASYCON_CMD_LED:
        case EASYCON_CMD_UNPAIR:
            return true;
        default:
            return false;
    }
}

static uint8_t s_short_cmd_rsp = EASYCON_RPY_ACK;

static parse_result_t easycon_short_cmd_parse_frame(void *state,
                                                    zc_ringbuf_t *rb,
                                                    parser_rsp_t *rsp)
{
    uint8_t frame[EASYCON_PROTOCOL_SHORT_CMD_SIZE];
    uint32_t n = zc_read_bulk(rb, frame, EASYCON_PROTOCOL_SHORT_CMD_SIZE);
    if (n != EASYCON_PROTOCOL_SHORT_CMD_SIZE) {
        return PARSE_INVALID;
    }

    uint8_t cmd = frame[1];
    s_short_cmd_rsp = EASYCON_RPY_ACK;

    if (cmd == EASYCON_CMD_VERSION) {
        s_short_cmd_rsp = 0x77; /* MCU version response */
    } else if (cmd == EASYCON_CMD_SCRIPT_START || cmd == EASYCON_CMD_SCRIPT_STOP) {
        s_short_cmd_rsp = EASYCON_RPY_SCRIPT_ACK;
    } else if (cmd == EASYCON_CMD_LED) {
        s_short_cmd_rsp = 0x00;
    }
    /* UNPAIR and others keep EASYCON_RPY_ACK */

    (void)state;
    rsp->data = &s_short_cmd_rsp;
    rsp->len  = 1;
    return PARSE_OK;
}

const protocol_parser_ops_t easycon_parser_short_cmd_ops = {
    .name         = PARSER_NAME,
    .min_peek_len = EASYCON_PROTOCOL_SHORT_CMD_SIZE,
    .reset        = easycon_short_cmd_reset,
    .probe        = easycon_short_cmd_probe,
    .parse_frame  = easycon_short_cmd_parse_frame,
};

#endif // CONFIG_PROTOCOL_LAYER_EASYCON
