#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer/zc_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_MAX_PROTOCOL_PARSERS
  #define MAX_PROTOCOL_PARSERS CONFIG_MAX_PROTOCOL_PARSERS
#else
  #define MAX_PROTOCOL_PARSERS 16
#endif

typedef enum {
    PARSE_OK = 0,
    PARSE_NEED_MORE,
    PARSE_INVALID,
    PARSE_OVERFLOW,
} parse_result_t;

typedef struct {
    uint8_t *data;
    uint32_t len;
} parser_rsp_t;

/**
 * @brief Operations table for an individual protocol parser.
 */
typedef struct {
    /** @brief Human-readable parser name (for debugging) */
    const char *name;

    /** @brief Minimum bytes required in the ring buffer before probe() is called */
    uint32_t min_peek_len;

    /**
     * @brief Reset the parser's private state.
     *
     * Called by the router when the parser has claimed a frame (probe
     * returned true) but parsing fails: either parse_frame is missing
     * or parse_frame returns PARSE_INVALID. Must bring the parser back
     * to a clean initial state.
     *
     * @param state Parser-specific state pointer.
     */
    void (*reset)(void *state);

    /**
     * @brief Probe whether this parser can handle the data at the head of the ring buffer.
     * @param state    Parser-specific state pointer.
     * @param head_ptr Pointer to the first contiguous segment.
     * @param head_len Length of the first contiguous segment.
     * @param wrap_ptr Pointer to the second contiguous segment (wrap-around), or NULL.
     * @param wrap_len Length of the second contiguous segment, or 0.
     * @return true if the parser claims this frame, false otherwise.
     */
    bool (*probe)(void *state,
                  uint8_t *head_ptr, uint32_t head_len,
                  uint8_t *wrap_ptr, uint32_t wrap_len);

    /**
     * @brief Parse a complete frame from the ring buffer and consume it.
     * @param state Parser-specific state pointer.
     * @param rb    Zero-copy ring buffer to read from.
     * @param rsp   Output response data (if any). If no response, set rsp->len = 0.
     * @return Parse result code. On PARSE_INVALID the caller (router) will
     *         invoke reset() to clear any partial state.
     */
    parse_result_t (*parse_frame)(void *state,
                                  zc_ringbuf_t *rb,
                                  parser_rsp_t *rsp);
} protocol_parser_ops_t;

/**
 * @brief A registered parser entry: pointer to its ops table and its private state.
 */
typedef struct {
    const protocol_parser_ops_t *ops;
    void *state;
} protocol_parser_t;

/**
 * @brief Protocol router instance.
 *
 * The upper-layer implementer initializes this structure statically.
 * parser_count and parsers[] must be filled in at compile-time or
 * startup by the protocol implementer. max_peek_len should be set to
 * the largest min_peek_len among all registered parsers so that the
 * router can perform a single peek operation.
 */
typedef struct {
    /** @brief Human-readable instance name (for debugging) */
    const char *name;

    /** @brief Largest min_peek_len among all registered parsers */
    uint32_t max_peek_len;

    /** @brief Number of currently registered parsers */
    uint32_t parser_count;

    /** @brief Array of pointers to registered parser entries */
    protocol_parser_t *parsers[MAX_PROTOCOL_PARSERS];
} protocol_instance_t;

/* TODO: Add a compile-time or run-time assertion in the protocol-layer
 *       integration code to ensure parser_count <= MAX_PROTOCOL_PARSERS. */

/**
 * @brief Route incoming data to the first matching parser.
 *
 * Performs a single zc_peek_bulk() up to max_peek_len, then iterates
 * over registered parsers. Parsers whose min_peek_len is not satisfied
 * are skipped. On the first successful probe, parse_frame() is invoked
 * and the parser is responsible for consuming the frame from the ring buffer.
 *
 * If the claiming parser has no parse_frame callback, or if parse_frame()
 * returns PARSE_INVALID, the parser's reset() callback is invoked before
 * returning so that partial state is cleared.
 *
 * @param inst The protocol instance.
 * @param rb   Zero-copy ring buffer containing received data.
 * @param rsp  Output response data (if any). The caller should zero-initialize
 *             rsp before calling; if no parser matches, rsp is left untouched.
 * @return PARSE_OK on success, PARSE_NEED_MORE if more data is required,
 *         PARSE_INVALID if no parser matched.
 */
parse_result_t protocol_route(protocol_instance_t *inst,
                              zc_ringbuf_t *rb,
                              parser_rsp_t *rsp);

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_H
