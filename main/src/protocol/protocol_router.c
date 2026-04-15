#include "protocol/protocol.h"

#include "esp_log.h"

parse_result_t protocol_route(protocol_instance_t *inst,
                                     zc_ringbuf_t *rb,
                                     parser_rsp_t *rsp)
{
    if (!inst || !rb || !rsp) {
        return PARSE_INVALID;
    }

    if (inst->parser_count == 0 || inst->max_peek_len == 0) {
        return PARSE_INVALID;
    }

    uint8_t *head_ptr = NULL;
    uint8_t *wrap_ptr = NULL;
    uint32_t head_len = 0;
    uint32_t wrap_len = 0;

    uint32_t avail = zc_peek_bulk(rb, inst->max_peek_len,
                                  &head_ptr, &head_len,
                                  &wrap_ptr, &wrap_len);

    bool has_insufficient = false;

    for (uint32_t i = 0; i < inst->parser_count; i++) {
        protocol_parser_t *parser = inst->parsers[i];
        if (!parser || !parser->ops || !parser->ops->probe) {
            continue;
        }

        if (avail == 0 || avail < parser->ops->min_peek_len) {
            has_insufficient = true;
            continue;
        }

        ESP_LOGD(LOG_PROTOCOL, "probing parser %s", parser->ops->name);

        if (parser->ops->probe(parser->state,
                               head_ptr, head_len,
                               wrap_ptr, wrap_len)) {
            if (!parser->ops->parse_frame) {
                if (parser->ops->reset) {
                    parser->ops->reset(parser->state);
                }
                return PARSE_INVALID;
            }

            ESP_LOGD(LOG_PROTOCOL, "parser %s accepted frame", parser->ops->name);

            parse_result_t result = parser->ops->parse_frame(parser->state, rb, rsp);
            if (result == PARSE_INVALID && parser->ops->reset) {
                parser->ops->reset(parser->state);
            }
            return result;
        }
    }

    /*
     * If at least one parser was skipped because we don't have enough
     * bytes yet, signal that we need more data. Otherwise, every parser
     * with sufficient data has rejected the frame: it's invalid.
     */
    return has_insufficient ? PARSE_NEED_MORE : PARSE_INVALID;
}
