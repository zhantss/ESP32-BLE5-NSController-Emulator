#ifndef TRANSPORT_UART_H
#define TRANSPORT_UART_H

#include "transport/transport.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UART transport configuration
 */
typedef struct {
  uart_port_t  port;
  int          baud_rate;
  int          rx_pin;
  int          tx_pin;
  int          rx_buffer_size;   /* reserved for future driver-based mode */
  int          tx_buffer_size;   /* reserved for future driver-based mode */
  TaskHandle_t notify_task;      /* task to notify via vTaskNotifyGiveFromISR */
} transport_uart_config_t;

/*
 *  Usage: transport_vtable_t is exported directly; the caller binds
 *  tp.ops = &transport_uart_vtable and then calls tp.ops->open(&tp, &cfg).
 */
extern const transport_vtable_t transport_uart_vtable;

#ifdef __cplusplus
}
#endif

#endif // TRANSPORT_UART_H
