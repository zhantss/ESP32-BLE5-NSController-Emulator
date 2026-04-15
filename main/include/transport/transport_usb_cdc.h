#ifndef TRANSPORT_USB_CDC_H
#define TRANSPORT_USB_CDC_H

#include "transport/transport.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB CDC (Virtual Serial) transport configuration
 */
typedef struct {
  int          rx_buffer_size;
  int          tx_buffer_size;
  TaskHandle_t notify_task;      /* task to notify via xTaskNotifyGive */
} transport_usb_cdc_config_t;

/*
 *  Usage: transport_vtable_t is exported directly; the caller binds
 *  tp.ops = &transport_usb_cdc_vtable and then calls tp.ops->open(&tp, &cfg).
 */
extern const transport_vtable_t transport_usb_cdc_vtable;

#ifdef __cplusplus
}
#endif

#endif // TRANSPORT_USB_CDC_H
