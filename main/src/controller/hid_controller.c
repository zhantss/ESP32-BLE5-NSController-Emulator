#include "controller/hid_controller.h"
#include "controller/hid_controller_pro2.h"
#include "device.h"
#include "utils.h"

#include <string.h>
#include <stdlib.h>

// Global controller instance
controller_handle_t g_controller = {
    .ops = &controller_ops,
    .hid_ops = NULL,
    .type = NULL,
    .buffer = {
        .front_buffer = NULL,
        .back_buffer = NULL,
        .swap_request = 0
    },
    .task_handle = NULL,
    .ns2_notification_handle = NS2_NOTIFICATION_HANDLE
};

// HID report send task
static void controller_task(void *arg) {
    controller_handle_t *ctrl = (controller_handle_t *)arg;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xInterval = pdMS_TO_TICKS(HID_REPORT_INTERVAL);

    ESP_LOGI(LOG_HID, "controller report task start, interval: %dms", HID_REPORT_INTERVAL);
    esp_log_level_set(LOG_HID, ESP_LOG_ERROR);

    while (1) {
        // check if notification is enabled
        g_subscribe_state_t *state = subscribe_entry_get(ctrl->ns2_notification_handle);
        if (state == NULL || !state->notify_enabled ||
            state->conn_handle == BLE_HS_CONN_HANDLE_NONE) {
            vTaskDelay(pdMS_TO_TICKS(100));  // waiting for enable
            continue;
        }

        if (ctrl->buffer.front_buffer != NULL) {
            if (ctrl->hid_ops == NULL || ctrl->hid_ops->next_report == NULL || ctrl->hid_ops->report_size == NULL) {
                ESP_LOGE(LOG_HID, "No valid hid device operations for type %d", ctrl->type);
                continue;
            }

            // Check and perform buffer swap if requested
            if (ctrl->buffer.swap_request) {
                // Ensure the latest values of swap_request and the contents of back_buffer are read.
                MEMORY_BARRIER();

                // Atomically swap front and back buffer pointers
                controller_hid_report_t* temp = ctrl->buffer.front_buffer;
                ctrl->buffer.front_buffer = ctrl->buffer.back_buffer;
                ctrl->buffer.back_buffer = temp;

                // Ensure the pointer swap operation completes before clearing the swap_request.
                MEMORY_BARRIER();

                // Clear swap request
                ctrl->buffer.swap_request = 0;
            }

            // Get report size for this device type
            size_t report_size = ctrl->hid_ops->report_size();

            // Use local buffer to avoid race condition with buffer swap
            // Copy data before potential buffer swap to ensure data integrity
            uint8_t report_buffer[report_size];
            uint8_t* next_report = ctrl->hid_ops->next_report(ctrl->buffer.front_buffer);
            memcpy(report_buffer, next_report, report_size);

            // Send report from local buffer - safe even if buffer swap occurs
            int rc = gatt_notify(state->conn_handle, ctrl->ns2_notification_handle,
                                    report_buffer, report_size);
            if (rc != 0) {
                ESP_LOGE(LOG_HID, "controller report send failed, rc: %d", rc);
            }
        }

        // precise interval
        vTaskDelayUntil(&xLastWakeTime, xInterval);
    }
}

static int controller_init_impl(controller_handle_t *ctrl, dev_type_t type) {
    if (ctrl == NULL) {
        return -1;
    }

    // Clean up any existing state first
    if (ctrl->ops != NULL && ctrl->ops->deinit != NULL) {
        ctrl->ops->deinit(ctrl);
    }

    // TODO JoyCon support
    ctrl->hid_ops = type == DEVICE_TYPE_PRO2 ? &controller_pro2_ops : NULL;
    if (ctrl->hid_ops == NULL) {
        ESP_LOGE(LOG_HID, "No valid hid device operations for type %d", type);
        return -1;
    }
    ctrl->type = type;

    // Allocate double buffer memory
    ctrl->buffer.front_buffer = (controller_hid_report_t*)malloc(sizeof(controller_hid_report_t));
    ctrl->buffer.back_buffer = (controller_hid_report_t*)malloc(sizeof(controller_hid_report_t));
    ctrl->buffer.swap_request = 0;

    if (ctrl->buffer.front_buffer == NULL || ctrl->buffer.back_buffer == NULL) {
        ESP_LOGE(LOG_HID, "malloc controller report double buffer memory failed");
        goto error;
    }

    memset(ctrl->buffer.front_buffer, 0, sizeof(controller_hid_report_t));
    memset(ctrl->buffer.back_buffer, 0, sizeof(controller_hid_report_t));

    ctrl->hid_ops->report_init(ctrl->buffer.front_buffer);
    ctrl->hid_ops->report_init(ctrl->buffer.back_buffer);

    if (ctrl->buffer.front_buffer->report == NULL || ctrl->buffer.back_buffer->report == NULL) {
        ESP_LOGE(LOG_HID, "controller report init failed");
        goto error;
    }

    return 0;

error:
    if (ctrl->ops != NULL && ctrl->ops->deinit != NULL) {
        ctrl->ops->deinit(ctrl);
    }
    return -1;
}

static void controller_deinit_impl(controller_handle_t *ctrl) {
    if (ctrl == NULL) {
        return;
    }

    if (ctrl->ops != NULL && ctrl->ops->stop_task != NULL) {
        ctrl->ops->stop_task(ctrl);
    }

    if (ctrl->buffer.front_buffer != NULL) {
        if (ctrl->buffer.front_buffer->report != NULL) {
            free(ctrl->buffer.front_buffer->report);
        }
        free(ctrl->buffer.front_buffer);
        ctrl->buffer.front_buffer = NULL;
    }
    if (ctrl->buffer.back_buffer != NULL) {
        if (ctrl->buffer.back_buffer->report != NULL) {
            free(ctrl->buffer.back_buffer->report);
        }
        free(ctrl->buffer.back_buffer);
        ctrl->buffer.back_buffer = NULL;
    }

    ctrl->buffer.swap_request = 0;
    ctrl->hid_ops = NULL;
}

static int controller_start_task_impl(controller_handle_t *ctrl) {
    if (ctrl == NULL) {
        return -1;
    }

    if (ctrl->task_handle != NULL) {
        ESP_LOGW(LOG_HID, "controller report task already started");
        return 0;
    }

    if (ctrl->buffer.front_buffer == NULL || ctrl->buffer.back_buffer == NULL) {
        ESP_LOGE(LOG_HID, "controller not initialized");
        return -1;
    }

    BaseType_t rc = xTaskCreate(controller_task, "controller_task", 4096, ctrl, 4, &ctrl->task_handle);
    if (rc != pdPASS) {
        ESP_LOGE(LOG_HID, "create controller report task failed, rc: %d", rc);
        return -1;
    }
    return 0;
}

static void controller_stop_task_impl(controller_handle_t *ctrl) {
    if (ctrl == NULL) {
        return;
    }

    if (ctrl->task_handle != NULL) {
        vTaskDelete(ctrl->task_handle);
        ctrl->task_handle = NULL;
    }
}

static controller_hid_report_t* controller_get_back_buffer_impl(controller_handle_t *ctrl) {
    if (ctrl == NULL) {
        return NULL;
    }
    return ctrl->buffer.back_buffer;
}

static void controller_request_swap_impl(controller_handle_t *ctrl) {
    if (ctrl == NULL) {
        return;
    }
    MEMORY_BARRIER();
    ctrl->buffer.swap_request = 1;
}

const controller_ops_t controller_ops = {
    .name           = "controller",
    .init           = controller_init_impl,
    .deinit         = controller_deinit_impl,
    .start_task     = controller_start_task_impl,
    .stop_task      = controller_stop_task_impl,
    .get_back_buffer = controller_get_back_buffer_impl,
    .request_swap   = controller_request_swap_impl,
};
