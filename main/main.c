#include <stdio.h>
#include "esp_log.h"
#include "device.h"
#include "hid.h"
#include "uart.h"

void app_main(void)
{
    // Initialize BLE stack
    ESP_LOGI("APP", "Initializing BLE stack...");
    ble_stack_init();

    // Initialize button event queue (needed for UART commands)
    ESP_LOGI("APP", "Initializing button event queue...");
    button_queue_init();

    // Initialize UART for command interface
    ESP_LOGI("APP", "Initializing UART command interface...");
    if (!uart_init()) {
        ESP_LOGE("APP", "Failed to initialize UART");
        // Continue anyway, UART is not critical for basic operation
    }

    // Note: HID task will be started automatically when BLE connection is established
    // via the appropriate BLE event handlers

    ESP_LOGI("APP", "System initialization complete");

    // Keep the task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
