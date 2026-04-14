#ifndef TRANSPORT_H
#define TRANSPORT_H

#include "buffer/zc_buffer.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *name;
  int (*open)(void *instance, void *config);
  void (*close)(void *instance);

  /**
   *  @brief bind the buffer and start RX.
   */
  int (*activate_rx)(void *instance, zc_ringbuf_t *rb);

  /**
   * @brief pause (for flow control)
   */
  int (*deactivate_rx)(void *instance);

  /**
   * @brief send api, no blocking
   */
  int (*submit_tx)(void *instance, const uint8_t *data, uint32_t len);

  /**
   * @brief wait for tx done, use to key commands
   */
  int (*flush_tx)(void *instance, uint32_t timeout_ms);

  /**
   * @brief check if tx is ready
   */
  bool (*is_ready)(void *instance);
} transport_vtable_t;

typedef struct {
    const transport_vtable_t *ops;        // function table
    void *hardware_ctx;                   // backend hardware contxt
    zc_ringbuf_t *rx_buffer;              // rx ring buffer

    uint32_t tx_seq;
    uint32_t rx_seq;
    uint64_t stats_tx_bytes;
    uint64_t stats_rx_bytes;
    uint64_t stats_rx_overflow;
} transport_handle_t;

#ifdef __cplusplus
}
#endif

#endif // TRANSPORT_H
