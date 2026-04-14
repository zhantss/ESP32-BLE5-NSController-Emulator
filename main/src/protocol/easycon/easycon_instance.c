#include "protocol/easycon/easycon_instance.h"

#ifdef CONFIG_PROTOCOL_LAYER_EASYCON

#include "protocol/easycon/easycon_protocol.h"

/* Only the slice parser needs persistent state (two-packet state machine). */
static easycon_slice_state_t easycon_slice_state;

/* Parser entries — stateless parsers use NULL as their state pointer. */
static protocol_parser_t easycon_hello_parser = {
    .ops   = &easycon_parser_hello_ops,
    .state = NULL,
};

static protocol_parser_t easycon_short_cmd_parser = {
    .ops   = &easycon_parser_short_cmd_ops,
    .state = NULL,
};

static protocol_parser_t easycon_simple_cmd_parser = {
    .ops   = &easycon_parser_simple_cmd_ops,
    .state = NULL,
};

static protocol_parser_t easycon_slice_parser = {
    .ops   = &easycon_parser_slice_ops,
    .state = &easycon_slice_state,
};

static protocol_parser_t easycon_hid_parser = {
    .ops   = &easycon_parser_hid_ops,
    .state = NULL,
};

/* Protocol instance */
protocol_instance_t easycon_protocol_instance = {
    .name         = "easycon",
    .max_peek_len = EASYCON_PROTOCOL_ENCODED_SIZE, /* 8, largest among parsers */
    .parser_count = 5,
    .parsers      = {
        &easycon_hello_parser,
        &easycon_short_cmd_parser,
        &easycon_simple_cmd_parser,
        &easycon_slice_parser,
        &easycon_hid_parser,
    },
};

#endif // CONFIG_PROTOCOL_LAYER_EASYCON
