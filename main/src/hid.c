#include "hid.h"
#include "device.h"
#include "utils.h"

#include <string.h>

// global hid report instance
pro2_hid_report_t *pro2_hid_report = NULL;
TaskHandle_t hid_task_handle = NULL;
uint16_t hid_report_gatt_handle = 0x000e;

// 12 bits stick data packed into 3 bytes
static void pack_stick_data(uint8_t out[3], uint16_t x, uint16_t y) {
    // limit to 12 bits
    x &= 0xFFF;
    y &= 0xFFF;
    // packed format: byte0=x[7:0], byte1=(y[3:0]<<4)|x[11:8], byte2=y[11:4]
    out[0] = x & 0xFF;
    out[1] = ((y & 0x0F) << 4) | ((x >> 8) & 0x0F);
    out[2] = (y >> 4) & 0xFF;
}

static void unpack_stick_data(const uint8_t in[3], uint16_t *x, uint16_t *y) {
    // packed format: byte0=x[7:0], byte1=(y[3:0]<<4)|x[11:8], byte2=y[11:4]
    *x = in[0] | ((in[1] & 0x0F) << 8);
    *y = ((in[1] >> 4) & 0x0F) | (in[2] << 4);
    // ensure 12 bits limit (though input should already be valid)
    *x &= 0xFFF;
    *y &= 0xFFF;
}

void pro2_report_init(pro2_hid_report_t *report) {
    if (report == NULL) return;

    memset(report, 0, sizeof(pro2_hid_report_t));

    // set fixed fields
    report->counter = 0;
    report->power_info = 0xFF;      // maybe 0x42
    report->unknown_0x0b = 0x38;
    report->unknown_0x0c = 0x00;
    report->headset_flag = 0x00;
    report->motion_data_len = 0x28;

    // stick set to center (12-bit center value 0x800)
    pro2_set_left_stick(report, 0x800, 0x800);
    pro2_set_right_stick(report, 0x800, 0x800);
}

void pro2_set_left_stick(pro2_hid_report_t *report, uint16_t x, uint16_t y) {
    pack_stick_data(report->left_stick, x, y);
}

void pro2_set_right_stick(pro2_hid_report_t *report, uint16_t x, uint16_t y) {
    pack_stick_data(report->right_stick, x, y);
}

void pro2_set_button(pro2_hid_report_t *report, pro2_btns btn, bool pressed) {
    uint8_t *btn_bytes = (uint8_t*)&report->buttons;

    switch (btn) {
        case A:      pressed ? (btn_bytes[0] |= 0x02) : (btn_bytes[0] &= ~0x02); break;
        case B:      pressed ? (btn_bytes[0] |= 0x01) : (btn_bytes[0] &= ~0x01); break;
        case X:      pressed ? (btn_bytes[0] |= 0x08) : (btn_bytes[0] &= ~0x08); break;
        case Y:      pressed ? (btn_bytes[0] |= 0x04) : (btn_bytes[0] &= ~0x04); break;
        case R:      pressed ? (btn_bytes[0] |= 0x10) : (btn_bytes[0] &= ~0x10); break;
        case ZR:     pressed ? (btn_bytes[0] |= 0x20) : (btn_bytes[0] &= ~0x20); break;
        case Plus:   pressed ? (btn_bytes[0] |= 0x40) : (btn_bytes[0] &= ~0x40); break;
        case RClick: pressed ? (btn_bytes[0] |= 0x80) : (btn_bytes[0] &= ~0x80); break;
        case Down:   pressed ? (btn_bytes[1] |= 0x01) : (btn_bytes[1] &= ~0x01); break;
        case Right:  pressed ? (btn_bytes[1] |= 0x02) : (btn_bytes[1] &= ~0x02); break;
        case Left:   pressed ? (btn_bytes[1] |= 0x04) : (btn_bytes[1] &= ~0x04); break;
        case Up:     pressed ? (btn_bytes[1] |= 0x08) : (btn_bytes[1] &= ~0x08); break;
        case L:      pressed ? (btn_bytes[1] |= 0x10) : (btn_bytes[1] &= ~0x10); break;
        case ZL:     pressed ? (btn_bytes[1] |= 0x20) : (btn_bytes[1] &= ~0x20); break;
        case Minus:  pressed ? (btn_bytes[1] |= 0x40) : (btn_bytes[1] &= ~0x40); break;
        case LClick: pressed ? (btn_bytes[1] |= 0x80) : (btn_bytes[1] &= ~0x80); break;
        case Home:   pressed ? (btn_bytes[2] |= 0x01) : (btn_bytes[2] &= ~0x01); break;
        case Capture:pressed ? (btn_bytes[2] |= 0x02) : (btn_bytes[2] &= ~0x02); break;
        case GR:     pressed ? (btn_bytes[2] |= 0x04) : (btn_bytes[2] &= ~0x04); break;
        case GL:     pressed ? (btn_bytes[2] |= 0x08) : (btn_bytes[2] &= ~0x08); break;
        case C:      pressed ? (btn_bytes[2] |= 0x10) : (btn_bytes[2] &= ~0x10); break;
        default: break;
    }
}

void pro2_press_button(pro2_hid_report_t *report, pro2_btns btn) {
    pro2_set_button(report, btn, true);
}

void pro2_release_button(pro2_hid_report_t *report, pro2_btns btn) {
    pro2_set_button(report, btn, false);
}

// hid report send task
static void hid_task(void *arg) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xInterval = pdMS_TO_TICKS(HID_REPORT_INTERVAL);

    ESP_LOGI(LOG_HID, "hid report task start, interval: %dms", HID_REPORT_INTERVAL);

    while (1) {
        // check if notification is enabled
        g_subscribe_state_t *state = subscribe_entry_get(hid_report_gatt_handle);
        if (state == NULL || !state->notify_enabled ||
            state->conn_handle == BLE_HS_CONN_HANDLE_NONE) {
            vTaskDelay(pdMS_TO_TICKS(100));  // waiting for enable
            continue;
        }

        if (pro2_hid_report != NULL) {
            // update report counter, wrap around 0-255
            pro2_hid_report->counter++;

            // TODO Process button events from queue

            // send hid report
            int rc = gatt_notify(state->conn_handle, hid_report_gatt_handle,
                                (uint8_t*)pro2_hid_report, sizeof(pro2_hid_report_t));
            if (rc != 0) {
                ESP_LOGW(LOG_HID, "hid report send failed, rc: %d", rc);
            }
        }

        // precise 15ms interval
        vTaskDelayUntil(&xLastWakeTime, xInterval);
    }
}

void hid_start_task(void) {
    if (hid_task_handle != NULL) {
        ESP_LOGW(LOG_HID, "hid report task already started");
        return;
    }

    if (g_dev_controller.type == DEVICE_TYPE_PRO2) {
        // malloc hid report memory
        if (pro2_hid_report == NULL) {
            pro2_hid_report = (pro2_hid_report_t*)malloc(sizeof(pro2_hid_report_t));
            if (pro2_hid_report == NULL) {
                ESP_LOGE(LOG_HID, "malloc hid report memory failed");
                return;
            }
            pro2_report_init(pro2_hid_report);
        }

        // create task for sending hid report
        BaseType_t rc = xTaskCreate(hid_task, "hid_task", 4096, NULL, 5, &hid_task_handle);
        if (rc != pdPASS) {
            ESP_LOGE(LOG_HID, "create hid report task failed, rc: %d", rc);
            free(pro2_hid_report);
            pro2_hid_report = NULL;
        }
    } else {
        // TODO JoyCon Support
        ESP_LOGE(LOG_HID, "hid report task not supported for device type: %d", g_dev_controller.type);
    }
    
}

void hid_stop_task(void) {
    if (hid_task_handle != NULL) {
        vTaskDelete(hid_task_handle);
        hid_task_handle = NULL;
    }
    if (pro2_hid_report != NULL) {
        free(pro2_hid_report);
        pro2_hid_report = NULL;
    }
}
