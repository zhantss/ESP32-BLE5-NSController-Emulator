#include "usb_serial_jtag.h"
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"

// Log tag
#define LOG_USB_SERIAL_JTAG "usb_serial_jtag"

// Global state
static bool s_initialized = false;

int dev_usb_serial_jtag_init(void) {
    if (s_initialized) {
        ESP_LOGW(LOG_USB_SERIAL_JTAG, "USB Serial/JTAG already initialized");
        return 0;
    }

    usb_serial_jtag_driver_config_t cfg = {
        .rx_buffer_size = 1024,
        .tx_buffer_size = 1024,
    };

    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_USB_SERIAL_JTAG, "Failed to install USB Serial/JTAG driver: %s", esp_err_to_name(err));
        return -1;
    }

    s_initialized = true;
    ESP_LOGI(LOG_USB_SERIAL_JTAG, "USB Serial/JTAG initialized");
    return 0;
}

void dev_usb_serial_jtag_deinit(void) {
    if (!s_initialized) {
        return;
    }

    usb_serial_jtag_driver_uninstall();
    s_initialized = false;
    ESP_LOGI(LOG_USB_SERIAL_JTAG, "USB Serial/JTAG deinitialized");
}

int dev_usb_serial_jtag_read_bytes(uint8_t* buf, size_t len, uint32_t timeout_ms) {
    if (!s_initialized) {
        return -1;
    }

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    int read_len = usb_serial_jtag_read_bytes(buf, len, ticks);
    return read_len;
}

int dev_usb_serial_jtag_send_data(const uint8_t* data, size_t len) {
    if (!s_initialized) {
        ESP_LOGE(LOG_USB_SERIAL_JTAG, "USB Serial/JTAG not initialized");
        return -1;
    }

    if (!usb_serial_jtag_is_connected()) {
        ESP_LOGW(LOG_USB_SERIAL_JTAG, "USB Serial/JTAG not connected to host");
        return -1;
    }

    int written = usb_serial_jtag_write_bytes(data, len, pdMS_TO_TICKS(100));
    if (written < 0) {
        ESP_LOGE(LOG_USB_SERIAL_JTAG, "Failed to write data");
        return -1;
    }

    return written;
}

bool dev_usb_serial_jtag_is_connected(void) {
    if (!s_initialized) {
        return false;
    }
    return usb_serial_jtag_is_connected();
}

int dev_usb_serial_jtag_start_task(void) {
    // USB Serial/JTAG driver doesn't require a separate task
    // Reading is done in the same uart_rx_task with transport abstraction
    ESP_LOGI(LOG_USB_SERIAL_JTAG, "USB Serial/JTAG ready for communication");
    return 0;
}

void dev_usb_serial_jtag_stop_task(void) {
    // Nothing to stop, task is shared with UART mode
    ESP_LOGI(LOG_USB_SERIAL_JTAG, "USB Serial/JTAG communication stopped");
}
