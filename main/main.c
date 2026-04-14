#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "device.h"
#include "controller/hid_controller.h"
#include "transport/transport.h"
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

    // Initialize transport layer
    if (transport_init() == 0) {
        ESP_LOGI(LOG_APP, "Transport initialized successfully");
        if (transport_start() == 0) {
            ESP_LOGI(LOG_APP, "Transport protocol task started");
        } else {
            ESP_LOGE(LOG_APP, "Failed to start transport protocol task");
        }
    } else {
        ESP_LOGE(LOG_APP, "Failed to initialize transport, continuing without serial input");
    }

    // Initialize controller (HID)
    if (g_controller.ops->init(&g_controller, g_controller_firmware.type) == 0) {
        ESP_LOGI(LOG_APP, "Controller initialized");
    } else {
        ESP_LOGE(LOG_APP, "Failed to initialize controller");
    }

    // Initialize BLE stack
    ble_stack_init();
    ESP_LOGI(LOG_APP, "BLE stack initialized");

    // Keep the main task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
