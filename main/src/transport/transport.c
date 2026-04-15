#include "transport/transport.h"
#include "buffer/zc_buffer.h"
#include "protocol/protocol.h"
#include "controller/hid_controller.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef CONFIG_TRANSPORT_LAYER_UART
#include "transport/transport_uart.h"
#include "driver/uart.h"
#endif

#ifdef CONFIG_TRANSPORT_LAYER_USB_SERIAL_JTAG
#include "transport/transport_usb_serial_jtag.h"
#endif

#ifdef CONFIG_PROTOCOL_LAYER_EASYCON
#include "protocol/easycon/easycon_instance.h"
#endif

/*
 * Ring-buffer size rationale:
 * - Typical host reports are ~10 bytes (buttons + sticks).
 * - Host send latency is ~30 ms.
 * - The largest protocol frame is 16 bytes (Simple HID).
 * - At 115200 baud, ~1152 bytes can arrive in 100 ms of worst-case scheduler delay.
 * - 256 bytes provides enough headroom for >25 frames (~750 ms of backlog),
 *   easily covering normal task-scheduling jitter or short BLE interrupts,
 *   while saving ~75 % RAM compared with the legacy 1024-byte UART driver buffer.
 */
#define TRANSPORT_RX_BUF_SIZE   256
#define TRANSPORT_TX_BUF_SIZE   256

static transport_handle_t g_transport;
static zc_ringbuf_t       g_transport_rx_ringbuf;
static uint8_t            g_transport_rx_buffer[TRANSPORT_RX_BUF_SIZE];
static TaskHandle_t       g_transport_protocol_task = NULL;

static protocol_instance_t *g_protocol_inst = NULL;

static void transport_protocol_task(void *arg)
{
    (void)arg;

    ESP_LOGI(LOG_TRANSPORT, "Protocol dispatcher task started");

    while (1) {
        if (g_protocol_inst == NULL || g_transport.ops == NULL || !g_transport.ops->is_ready(&g_transport)) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        parser_rsp_t rsp = {0};
        parse_result_t result = protocol_route(g_protocol_inst, &g_transport_rx_ringbuf, &rsp);
        ESP_LOGD(LOG_TRANSPORT, "Protocol route result: %d", result);

        if (result == PARSE_OK) {
            if (rsp.len > 0 && g_transport.ops->submit_tx != NULL) {
                g_transport.ops->submit_tx(&g_transport, rsp.data, rsp.len);
            }
        } else if (result == PARSE_NEED_MORE) {
            /* No complete frame yet; yield to let the RX task fill the buffer. */
            vTaskDelay(2);
        } else {
            /* PARSE_INVALID: no parser matched or frame error.
             * Yield briefly to avoid tight spinning on garbage data.
             */
            vTaskDelay(1);
        }
    }
}

int transport_init(void)
{
    memset(&g_transport, 0, sizeof(g_transport));
    memset(g_transport_rx_buffer, 0, sizeof(g_transport_rx_buffer));

    if (zc_init(&g_transport_rx_ringbuf, g_transport_rx_buffer, TRANSPORT_RX_BUF_SIZE, 0) != ESP_OK) {
        ESP_LOGE(LOG_TRANSPORT, "Failed to initialize transport RX ring buffer");
        return -1;
    }

#ifdef CONFIG_PROTOCOL_LAYER_EASYCON
    g_protocol_inst = &easycon_protocol_instance;
#else
    g_protocol_inst = NULL;
    ESP_LOGW(LOG_TRANSPORT, "No protocol instance available for current configuration");
#endif

#ifdef CONFIG_TRANSPORT_LAYER_UART
    g_transport.ops = &transport_uart_vtable;

    transport_uart_config_t uart_cfg = {
        .port           = UART_NUM_1,
        .baud_rate      = 115200,
        .rx_pin         = CONFIG_UART_RX_PIN,
        .tx_pin         = CONFIG_UART_TX_PIN,
        .rx_buffer_size = TRANSPORT_RX_BUF_SIZE,
        .tx_buffer_size = TRANSPORT_TX_BUF_SIZE,
        .notify_task    = NULL,
    };

    if (g_transport.ops->open(&g_transport, &uart_cfg) != 0) {
        ESP_LOGE(LOG_TRANSPORT, "Failed to open UART transport");
        return -1;
    }

    if (g_transport.ops->activate_rx(&g_transport, &g_transport_rx_ringbuf) != 0) {
        ESP_LOGE(LOG_TRANSPORT, "Failed to activate UART RX");
        g_transport.ops->close(&g_transport);
        return -1;
    }

    ESP_LOGI(LOG_TRANSPORT, "UART transport initialized");
#elif CONFIG_TRANSPORT_LAYER_USB_SERIAL_JTAG
    g_transport.ops = &transport_usb_serial_jtag_vtable;

    transport_usb_serial_jtag_config_t usj_cfg = {
        .rx_buffer_size = TRANSPORT_RX_BUF_SIZE,
        .tx_buffer_size = TRANSPORT_TX_BUF_SIZE,
        .notify_task    = NULL,
    };

    if (g_transport.ops->open(&g_transport, &usj_cfg) != 0) {
        ESP_LOGE(LOG_TRANSPORT, "Failed to open USB Serial/JTAG transport");
        return -1;
    }

    if (g_transport.ops->activate_rx(&g_transport, &g_transport_rx_ringbuf) != 0) {
        ESP_LOGE(LOG_TRANSPORT, "Failed to activate USB Serial/JTAG RX");
        g_transport.ops->close(&g_transport);
        return -1;
    }

    ESP_LOGI(LOG_TRANSPORT, "USB Serial/JTAG transport initialized");
#else
    ESP_LOGE(LOG_TRANSPORT, "No transport layer selected in configuration");
    return -1;
#endif

    return 0;
}

int transport_start(void)
{
    if (g_transport_protocol_task != NULL) {
        ESP_LOGW(LOG_TRANSPORT, "Protocol dispatcher task already started");
        return 0;
    }

    BaseType_t rc = xTaskCreate(transport_protocol_task,
                                "transport_proto",
                                4096,
                                NULL,
                                4,
                                &g_transport_protocol_task);
    if (rc != pdPASS) {
        ESP_LOGE(LOG_TRANSPORT, "Failed to create protocol dispatcher task");
        return -1;
    }

    ESP_LOGI(LOG_TRANSPORT, "Protocol dispatcher task started");
    return 0;
}
