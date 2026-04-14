#include "protocol/easycon/easycon_protocol.h"

#ifdef CONFIG_PROTOCOL_LAYER_EASYCON

#include "buffer/zc_buffer.h"
#include "utils.h"

#include <stdint.h>
#include <stdbool.h>

#define PARSER_NAME "easycon_hello"

static void easycon_hello_reset(void *state)
{
    (void)state;
}

static bool easycon_hello_probe(void *state,
                                uint8_t *head_ptr, uint32_t head_len,
                                uint8_t *wrap_ptr, uint32_t wrap_len)
{
    (void)state;
    if (peek_byte(head_ptr, head_len, wrap_ptr, wrap_len, 0) != EASYCON_CMD_READY) {
        return false;
    }
    if (peek_byte(head_ptr, head_len, wrap_ptr, wrap_len, 1) != EASYCON_CMD_READY) {
        return false;
    }
    if (peek_byte(head_ptr, head_len, wrap_ptr, wrap_len, 2) != EASYCON_CMD_HELLO) {
        return false;
    }
    return true;
}

static uint8_t s_hello_rsp = EASYCON_RPY_HELLO;

static parse_result_t easycon_hello_parse_frame(void *state,
                                                zc_ringbuf_t *rb,
                                                parser_rsp_t *rsp)
{
    uint8_t frame[EASYCON_PROTOCOL_HELLO_SIZE];
    uint32_t n = zc_read_bulk(rb, frame, EASYCON_PROTOCOL_HELLO_SIZE);
    if (n != EASYCON_PROTOCOL_HELLO_SIZE) {
        return PARSE_INVALID;
    }

    (void)state;
    (void)frame; /* frame content already validated by probe */

    rsp->data = &s_hello_rsp;
    rsp->len  = 1;
    return PARSE_OK;
}

const protocol_parser_ops_t easycon_parser_hello_ops = {
    .name        = PARSER_NAME,
    .min_peek_len = EASYCON_PROTOCOL_HELLO_SIZE,
    .reset       = easycon_hello_reset,
    .probe       = easycon_hello_probe,
    .parse_frame = easycon_hello_parse_frame,
};

#endif // CONFIG_PROTOCOL_LAYER_EASYCON
