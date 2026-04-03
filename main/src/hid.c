#include "hid.h"
#include "hid_pro2.h"
#include "device.h"
#include "utils.h"
#include "uart.h"

#include <string.h>

// global hid report instance
// pro2_hid_report_t *pro2_hid_report = NULL;
TaskHandle_t hid_task_handle = NULL;
uint16_t hid_report_gatt_handle = 0x000e;

// double buffer manager
hid_double_buffer_t g_hid_double_buffer = {
    .front_buffer = NULL,
    .back_buffer = NULL,
    .swap_request = 0
};

const hid_device_ops_t* hid_get_device_ops(dev_type_t type) {
    switch (type) {
        case DEVICE_TYPE_PRO2:
            return &pro2_hid_ops;
        case DEVICE_TYPE_JOYCON:
            // TODO return &joycon_hid_ops;
            ESP_LOGW(LOG_HID, "JoyCon support not implemented yet");
            return NULL;
        default:
            ESP_LOGE(LOG_HID, "Unknown device type: %d", type);
            return NULL;
    }
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

        if (g_hid_double_buffer.front_buffer != NULL) {
            const hid_device_ops_t *ops = hid_get_device_ops(g_hid_double_buffer.front_buffer->type);
            if (ops == NULL || ops->next_report == NULL || ops->report_size == NULL) {
                ESP_LOGE(LOG_HID, "No valid device operations for type %d", g_hid_double_buffer.front_buffer->type);
                continue;
            }
            
            // Check and perform buffer swap if requested
            if (g_hid_double_buffer.swap_request) {
                // Ensure the latest values of swap_request and the contents of back_buffer are read.
                MEMORY_BARRIER();

                // Atomically swap front and back buffer pointers
                hid_device_report_t* temp = g_hid_double_buffer.front_buffer;
                g_hid_double_buffer.front_buffer = g_hid_double_buffer.back_buffer;
                g_hid_double_buffer.back_buffer = temp;

                // Ensure the pointer swap operation completes before clearing the swap_request.
                MEMORY_BARRIER();

                // Clear swap request
                g_hid_double_buffer.swap_request = 0;
            }

            // Get report size for this device type
            size_t report_size = ops->report_size();

            // Use local buffer to avoid race condition with buffer swap
            // Copy data before potential buffer swap to ensure data integrity
            uint8_t report_buffer[report_size];
            uint8_t* next_report = ops->next_report(g_hid_double_buffer.front_buffer);
            memcpy(report_buffer, next_report, report_size);

            // Send report from local buffer - safe even if buffer swap occurs
            int rc = gatt_notify(state->conn_handle, hid_report_gatt_handle,
                                    report_buffer, report_size);
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

    // malloc double buffer memory
    if (g_hid_double_buffer.front_buffer == NULL) {
        g_hid_double_buffer.front_buffer = (hid_device_report_t*)malloc(sizeof(hid_device_report_t));
        g_hid_double_buffer.back_buffer = (hid_device_report_t*)malloc(sizeof(hid_device_report_t));
        g_hid_double_buffer.swap_request = 0;
    }

    if (g_hid_double_buffer.front_buffer == NULL || g_hid_double_buffer.back_buffer == NULL) {
        ESP_LOGE(LOG_HID, "malloc hid report double buffer memory failed");
        goto error;
    }

    const hid_device_ops_t *ops = hid_get_device_ops(g_dev_controller.type);
    ops->report_init(g_hid_double_buffer.front_buffer);
    ops->report_init(g_hid_double_buffer.back_buffer);
    
    if (g_hid_double_buffer.front_buffer->report == NULL || g_hid_double_buffer.back_buffer->report == NULL) {
        ESP_LOGE(LOG_HID, "malloc hid report double buffer memory failed");
        goto error;
    }

    BaseType_t rc = xTaskCreate(hid_task, "hid_task", 4096, NULL, 7, &hid_task_handle);
    if (rc != pdPASS) {
        ESP_LOGE(LOG_HID, "create hid report task failed, rc: %d", rc);
        goto error;
    }
    return;
error:
    // Clean up
    if (g_hid_double_buffer.front_buffer != NULL) {
        free(g_hid_double_buffer.front_buffer);
        g_hid_double_buffer.front_buffer = NULL;
    }
    if (g_hid_double_buffer.back_buffer != NULL) {
        free(g_hid_double_buffer.back_buffer);
        g_hid_double_buffer.back_buffer = NULL;
    }
    return;
}

void hid_stop_task(void) {
    if (hid_task_handle != NULL) {
        vTaskDelete(hid_task_handle);
        hid_task_handle = NULL;
    }

    // free
    if (g_hid_double_buffer.front_buffer != NULL) {
        free(g_hid_double_buffer.front_buffer);
        g_hid_double_buffer.front_buffer = NULL;
    }
    if (g_hid_double_buffer.back_buffer != NULL) {
        free(g_hid_double_buffer.back_buffer);
        g_hid_double_buffer.back_buffer = NULL;
    }

    g_hid_double_buffer.swap_request = 0;
}


// 12 bits stick data packed into 3 bytes
void pack_stick_data(uint8_t out[3], uint16_t x, uint16_t y) {
    // limit to 12 bits
    x &= 0xFFF;
    y &= 0xFFF;
    // packed format: byte0=x[7:0], byte1=(y[3:0]<<4)|x[11:8], byte2=y[11:4]
    out[0] = x & 0xFF;
    out[1] = ((y & 0x0F) << 4) | ((x >> 8) & 0x0F);
    out[2] = (y >> 4) & 0xFF;
}

void unpack_stick_data(const uint8_t in[3], uint16_t *x, uint16_t *y) {
    // packed format: byte0=x[7:0], byte1=(y[3:0]<<4)|x[11:8], byte2=y[11:4]
    *x = in[0] | ((in[1] & 0x0F) << 8);
    *y = ((in[1] >> 4) & 0x0F) | (in[2] << 4);
    // ensure 12 bits limit (though input should already be valid)
    *x &= 0xFFF;
    *y &= 0xFFF;
}
