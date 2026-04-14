#ifndef HID_CONTROLLER_H
#define HID_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

// memory barrier
#define MEMORY_BARRIER() __sync_synchronize()

// HID Report Interval(ms) - now configurable via menuconfig
#ifndef CONFIG_HID_REPORT_INTERVAL
#define CONFIG_HID_REPORT_INTERVAL 15
#endif
#define HID_REPORT_INTERVAL     CONFIG_HID_REPORT_INTERVAL

#define NS2_NOTIFICATION_HANDLE    0x000e

typedef struct controller_handle controller_handle_t;

typedef struct {
    controller_type_t type;
    void *report;
} controller_hid_report_t;

// HID device specific operations (pro2, joycon, etc.)
typedef struct {
    const char *name;
    void (*report_init)(controller_hid_report_t *report);
    void (*set_button)(controller_hid_report_t *report, uint16_t btn_id, bool pressed);
    void (*set_button_custom)(controller_hid_report_t *report, uint8_t *data, size_t len);
    void (*set_left_stick)(controller_hid_report_t *report, uint16_t x, uint16_t y);
    void (*set_right_stick)(controller_hid_report_t *report, uint16_t x, uint16_t y);
    uint8_t* (*next_report)(controller_hid_report_t *report);
    size_t (*report_size)(void);
} controller_hid_ops_t;

// Controller management operations
typedef struct {
    const char *name;
    int  (*init)(controller_handle_t *ctrl, controller_type_t type);
    void (*deinit)(controller_handle_t *ctrl);
    int  (*start_task)(controller_handle_t *ctrl);
    void (*stop_task)(controller_handle_t *ctrl);
    controller_hid_report_t* (*get_back_buffer)(controller_handle_t *ctrl);
    void (*request_swap)(controller_handle_t *ctrl);
} controller_ops_t;

struct controller_handle {
    const controller_ops_t *ops;
    const controller_hid_ops_t *hid_ops;
    controller_type_t type;

    struct {
        controller_hid_report_t *front_buffer;
        controller_hid_report_t *back_buffer;
        volatile uint32_t swap_request;
    } buffer;

    TaskHandle_t task_handle;
    uint16_t     ns2_notification_handle;
};

// Global controller instance
extern controller_handle_t g_hid_controller;

// Global controller operations
extern const controller_ops_t controller_ops;

// 12 bits stick data packed into 3 bytes
static inline void pack_stick_data(uint8_t out[3], uint16_t x, uint16_t y) {
    x &= 0xFFF;
    y &= 0xFFF;
    out[0] = x & 0xFF;
    out[1] = ((y & 0x0F) << 4) | ((x >> 8) & 0x0F);
    out[2] = (y >> 4) & 0xFF;
}

static inline void unpack_stick_data(const uint8_t in[3], uint16_t *x, uint16_t *y) {
    *x = in[0] | ((in[1] & 0x0F) << 8);
    *y = ((in[1] >> 4) & 0x0F) | (in[2] << 4);
    *x &= 0xFFF;
    *y &= 0xFFF;
}

#ifdef __cplusplus
}
#endif

#endif // HID_CONTROLLER_H
