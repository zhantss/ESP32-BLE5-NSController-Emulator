#include "protocol/easycon/easycon_protocol.h"

#ifdef CONFIG_PROTOCOL_LAYER_EASYCON

#include "buffer/zc_buffer.h"
#include "utils.h"

#include <stdint.h>
#include <stdbool.h>

#define PARSER_NAME "easycon_simple_cmd"

static void easycon_simple_cmd_reset(void *state)
{
    (void)state;
}

static bool easycon_simple_cmd_probe(void *state,
                                     uint8_t *head_ptr, uint32_t head_len,
                                     uint8_t *wrap_ptr, uint32_t wrap_len)
{
    (void)state;

    if (peek_byte(head_ptr, head_len, wrap_ptr, wrap_len, 0) != EASYCON_CMD_READY) {
        return false;
    }

    uint8_t cmd = peek_byte(head_ptr, head_len, wrap_ptr, wrap_len, 2);
    return (cmd == EASYCON_CMD_CHANGE_CONTROLLER_MODE ||
            cmd == EASYCON_CMD_CHANGE_AMIIBO_INDEX);
}

static uint8_t s_simple_cmd_rsp = EASYCON_RPY_ACK;

static parse_result_t easycon_simple_cmd_parse_frame(void *state,
                                                     zc_ringbuf_t *rb,
                                                     parser_rsp_t *rsp)
{
    uint8_t frame[EASYCON_PROTOCOL_SIMPLE_CMD_SIZE];
    uint32_t n = zc_read_bulk(rb, frame, EASYCON_PROTOCOL_SIMPLE_CMD_SIZE);
    if (n != EASYCON_PROTOCOL_SIMPLE_CMD_SIZE) {
        return PARSE_INVALID;
    }

    uint8_t data = frame[1];
    uint8_t cmd  = frame[2];
    (void)data;
    (void)cmd;
    /* TODO: wire data to actual device logic if needed */

    (void)state;
    s_simple_cmd_rsp = EASYCON_RPY_ACK;
    rsp->data = &s_simple_cmd_rsp;
    rsp->len  = 1;
    return PARSE_OK;
}

const protocol_parser_ops_t easycon_parser_simple_cmd_ops = {
    .name         = PARSER_NAME,
    .min_peek_len = EASYCON_PROTOCOL_SIMPLE_CMD_SIZE,
    .reset        = easycon_simple_cmd_reset,
    .probe        = easycon_simple_cmd_probe,
    .parse_frame  = easycon_simple_cmd_parse_frame,
};

#endif // CONFIG_PROTOCOL_LAYER_EASYCON
