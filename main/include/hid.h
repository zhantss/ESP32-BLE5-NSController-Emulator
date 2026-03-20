#ifndef _HID_H
#define _HID_H

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// HID Report Interval(ms)
#define HID_REPORT_INTERVAL     15

// pro2 button format
// https://github.com/ndeadly/switch2_controller_research/blob/master/hid_reports.md#button-format-3
typedef struct {
    // Buttons Byte 0
    uint8_t B        : 1; // 0x01
    uint8_t A        : 1; // 0x02
    uint8_t Y        : 1; // 0x04
    uint8_t X        : 1; // 0x08
    uint8_t R        : 1; // 0x10
    uint8_t ZR       : 1; // 0x20
    uint8_t Plus     : 1; // 0x40
    uint8_t RClick   : 1; // 0x80

    // Buttons Byte 1
    uint8_t Down     : 1; // 0x01
    uint8_t Right    : 1; // 0x02
    uint8_t Left     : 1; // 0x04
    uint8_t Up       : 1; // 0x08
    uint8_t L        : 1; // 0x10
    uint8_t ZL       : 1; // 0x20
    uint8_t Minus    : 1; // 0x40
    uint8_t LClick   : 1; // 0x80

    // Buttons Byte 2
    uint8_t Home     : 1; // 0x01
    uint8_t Capture  : 1; // 0x02
    uint8_t GR       : 1; // 0x04
    uint8_t GL       : 1; // 0x08
    uint8_t C        : 1; // 0x10
    uint8_t reserved0: 3; // 0x20-0x80 Placeholder
} pro2_btn_bits_t;

typedef enum {
    // pro2_btn_bits_t Byte 0
    B,      // bit 0
    A,      // bit 1
    Y,      // bit 2
    X,      // bit 3
    R,      // bit 4
    ZR,     // bit 5
    Plus,   // bit 6
    RClick, // bit 7

    // pro2_btn_bits_t Byte 1
    Down,   // bit 0
    Right,  // bit 1
    Left,   // bit 2
    Up,     // bit 3
    L,      // bit 4
    ZL,     // bit 5
    Minus,  // bit 6
    LClick, // bit 7

    // pro2_btn_bits_t Byte 2
    Home,   // bit 0
    Capture,// bit 1
    GR,     // bit 2
    GL,     // bit 3
    C,      // bit 4
    // bit 5-7 reserved
} pro2_btns;

typedef struct {
    uint16_t x : 12;    // x
    uint16_t y : 12;    // y
} pro2_analog_stick_t;

typedef struct __attribute__((packed)) {
    uint8_t counter;            // 0x00 0x01 Counter
    uint8_t power_info;         // 0x01 0x01 Power Info
    pro2_btn_bits_t buttons;    // 0x02 0x03 Buttons
    uint8_t left_stick[3];      // 0x04 0x06 Left Analog Stick
    uint8_t right_stick[3];     // 0x07 0x09 Right Analog Stick
    uint8_t unknown_0x0b;       // 0x0B 0x01 Unknown Always 0x38?
    uint8_t unknown_0x0c;       // 0x0C 0x01 Unknown Always 0x00?
    uint8_t headset_flag;       // 0x0D 0x01 Headset Flags
    uint8_t motion_data_len;    // 0x0E 0x01 Motion Data Length Always 0x28
    uint8_t motion_data[0x28];  // 0x0F 0x28 Motion Data
    uint8_t reserved[8];        // 0x37 0x08 Placeholder Default 0x00
} pro2_hid_report_t;

static_assert(sizeof(pro2_hid_report_t) == 63);

// HID Report Handle

// pro2 hid report entity
extern pro2_hid_report_t *pro2_hid_report;

// hid task handle
extern TaskHandle_t hid_task_handle;

// hid report gatt handle
extern uint16_t hid_report_gatt_handle;

void hid_start_task(void);
void hid_stop_task(void);

// Pro2 Functions

void pro2_report_init(pro2_hid_report_t *report);
void pro2_set_left_stick(pro2_hid_report_t *report, uint16_t x, uint16_t y);
void pro2_set_right_stick(pro2_hid_report_t *report, uint16_t x, uint16_t y);
void pro2_set_button(pro2_hid_report_t *report, pro2_btns btn, bool pressed);
void pro2_press_button(pro2_hid_report_t *report, pro2_btns btn);
void pro2_release_button(pro2_hid_report_t *report, pro2_btns btn);

// TODO JoyCon Support

#endif // _HID_H