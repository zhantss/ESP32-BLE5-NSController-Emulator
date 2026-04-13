#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "device.h"
#include "hid.h"
#include "uart.h"
#include "ns2_ble_gatt.h"

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    #ifdef CONFIG_MCU_DEBUG
        esp_log_level_set(LOG_APP, ESP_LOG_DEBUG);
        esp_log_level_set(LOG_BLE_GAP, ESP_LOG_DEBUG);
        esp_log_level_set(LOG_BLE_GATT, ESP_LOG_DEBUG);
    #endif
    ESP_LOGI(LOG_APP, "Nintendo Switch Controller Emulator");
    ESP_LOGI(LOG_APP, "Starting initialization...");

    // Initialize UART for external input
    int uart_ret = dev_uart_init();
    if (uart_ret != 0) {
        ESP_LOGE(LOG_APP, "Failed to initialize UART, continuing without serial input");
    } else {
        ESP_LOGI(LOG_APP, "UART initialized successfully");
        // Start UART RX task
        if (dev_uart_start_task() == 0) {
            ESP_LOGI(LOG_APP, "UART RX task started");
        } else {
            ESP_LOGE(LOG_APP, "Failed to start UART RX task");
        }
    }

    // Initialize BLE stack
    ble_stack_init();
    ESP_LOGI(LOG_APP, "BLE stack initialized");

    // Keep the main task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
