#ifndef _HID_H
#define _HID_H

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "device.h"

// HID Report Interval(ms)
#define HID_REPORT_INTERVAL     15
typedef struct {
    dev_type_t type;
    void *report;
} hid_device_report_t;

typedef struct {
    void (*report_init)(hid_device_report_t *report);
    void (*set_button)(hid_device_report_t *report, uint16_t btn_id, bool pressed);
    void (*set_button_custom)(hid_device_report_t *report, uint8_t *data, size_t len);
    void (*set_left_stick)(hid_device_report_t *report, uint16_t x, uint16_t y);
    void (*set_right_stick)(hid_device_report_t *report, uint16_t x, uint16_t y);
    uint8_t* (*next_report)(hid_device_report_t *report);
    size_t (*report_size)(void);
} hid_device_ops_t;

// Double buffer manager structure
typedef struct {
    hid_device_report_t* front_buffer;    // Front buffer (read by HID task)
    hid_device_report_t* back_buffer;     // Back buffer (updated by UART)
    volatile uint32_t swap_request;     // Swap request flag (atomic access)
} hid_double_buffer_t;

// Global double buffer manager
extern hid_double_buffer_t g_hid_double_buffer;

// hid task handle
extern TaskHandle_t hid_task_handle;

// hid report gatt handle
extern uint16_t hid_report_gatt_handle;

void hid_start_task(void);
void hid_stop_task(void);

// hid functions

void hid_report_init(hid_device_report_t *report);
void hid_set_button(hid_device_report_t *report, uint16_t btn_id, bool pressed);
void hid_set_left_stick(hid_device_report_t *report, uint16_t x, uint16_t y);
void hid_set_right_stick(hid_device_report_t *report, uint16_t x, uint16_t y);

const hid_device_ops_t* hid_get_device_ops(dev_type_t type);

// stick data format
void pack_stick_data(uint8_t out[3], uint16_t x, uint16_t y);
void unpack_stick_data(const uint8_t in[3], uint16_t *x, uint16_t *y);

// TODO JoyCon Support

#endif // _HID_H