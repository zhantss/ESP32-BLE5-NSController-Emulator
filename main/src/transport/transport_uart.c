#include "transport/transport_uart.h"

#ifdef CONFIG_TRANSPORT_LAYER_UART

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#define TAG "transport_uart"

#if CONFIG_IDF_TARGET_ESP32C61 || CONFIG_IDF_TARGET_ESP32C6
  #define UART_RX_TIMEOUT_MS   2
  #define UART_TASK_AFFINITY   tskNO_AFFINITY
#elif CONFIG_IDF_TARGET_ESP32S3
  #define UART_RX_TIMEOUT_MS   10
  #define UART_TASK_AFFINITY   1
#else
  #define UART_RX_TIMEOUT_MS   10
  #define UART_TASK_AFFINITY   tskNO_AFFINITY
#endif

typedef struct {
  transport_uart_config_t config;
  transport_handle_t     *tp;
  TaskHandle_t            rx_task;
  volatile bool           running;
  volatile bool           opened;
} transport_uart_ctx_t;

/* -------------------------------------------------------------------------- */
/*  Forward declarations                                                      */
/* -------------------------------------------------------------------------- */
static int     transport_uart_open(void *instance, void *config);
static void    transport_uart_close(void *instance);
static int     transport_uart_activate_rx(void *instance, zc_ringbuf_t *rb);
static int     transport_uart_deactivate_rx(void *instance);
static int     transport_uart_submit_tx(void *instance, const uint8_t *data, uint32_t len);
static int     transport_uart_flush_tx(void *instance, uint32_t timeout_ms);
static bool    transport_uart_is_ready(void *instance);

const transport_vtable_t transport_uart_vtable = {
  .name           = "uart",
  .open           = transport_uart_open,
  .close          = transport_uart_close,
  .activate_rx    = transport_uart_activate_rx,
  .deactivate_rx  = transport_uart_deactivate_rx,
  .submit_tx      = transport_uart_submit_tx,
  .flush_tx       = transport_uart_flush_tx,
  .is_ready       = transport_uart_is_ready,
};

/* -------------------------------------------------------------------------- */
/*  RX Task (driver API path)                                                 */
/* -------------------------------------------------------------------------- */
static void transport_uart_rx_task(void *arg)
{
  transport_uart_ctx_t *ctx = (transport_uart_ctx_t *)arg;
  transport_handle_t   *tp  = ctx->tp;

  while (ctx->running) {
    uint8_t *ptr = NULL;
    uint32_t avail = zc_reserve(tp->rx_buffer, &ptr);
    if (avail == 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    int len = uart_read_bytes(ctx->config.port, ptr, avail,
                              pdMS_TO_TICKS(UART_RX_TIMEOUT_MS));
    if (len > 0) {
      zc_commit(tp->rx_buffer, (uint32_t)len);
      tp->stats_rx_bytes += (uint32_t)len;
      tp->rx_seq++;

      if (ctx->config.notify_task != NULL) {
        xTaskNotifyGive(ctx->config.notify_task);
      }
    }
  }

  ctx->rx_task = NULL;
  vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------- */
/*  VTable implementations                                                    */
/* -------------------------------------------------------------------------- */
static int transport_uart_open(void *instance, void *config)
{
  transport_handle_t *tp = (transport_handle_t *)instance;

  if (tp == NULL || config == NULL) {
    ESP_LOGE(TAG, "invalid arguments");
    return -1;
  }

  transport_uart_ctx_t *ctx = (transport_uart_ctx_t *)tp->hardware_ctx;
  if (ctx != NULL && ctx->opened) {
    ESP_LOGW(TAG, "UART already opened");
    return 0;
  }

  ctx = (transport_uart_ctx_t *)malloc(sizeof(transport_uart_ctx_t));
  if (ctx == NULL) {
    ESP_LOGE(TAG, "failed to allocate context");
    return -1;
  }
  memset(ctx, 0, sizeof(transport_uart_ctx_t));

  memcpy(&ctx->config, config, sizeof(transport_uart_config_t));

  uart_config_t uart_cfg = {
    .baud_rate  = ctx->config.baud_rate,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };

  esp_err_t err;
  err = uart_driver_install(ctx->config.port,
                            ctx->config.rx_buffer_size,
                            ctx->config.tx_buffer_size,
                            0, NULL, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
    free(ctx);
    return -1;
  }

  err = uart_param_config(ctx->config.port, &uart_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
    uart_driver_delete(ctx->config.port);
    free(ctx);
    return -1;
  }

  err = uart_set_pin(ctx->config.port,
                     ctx->config.tx_pin,
                     ctx->config.rx_pin,
                     UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
    uart_driver_delete(ctx->config.port);
    free(ctx);
    return -1;
  }

  ctx->opened = true;
  tp->hardware_ctx = ctx;

  ESP_LOGI(TAG, "UART opened: port=%d, baud=%d, rx_pin=%d, tx_pin=%d",
           ctx->config.port, ctx->config.baud_rate,
           ctx->config.rx_pin, ctx->config.tx_pin);
  return 0;
}

static void transport_uart_close(void *instance)
{
  if (instance == NULL) {
    return;
  }

  transport_handle_t   *tp  = (transport_handle_t *)instance;
  transport_uart_ctx_t *ctx = (transport_uart_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    return;
  }

  transport_uart_deactivate_rx(instance);

  uart_driver_delete(ctx->config.port);
  ctx->opened = false;
  tp->rx_buffer = NULL;

  ESP_LOGI(TAG, "UART closed: port=%d", ctx->config.port);

  free(ctx);
  tp->hardware_ctx = NULL;
}

static int transport_uart_activate_rx(void *instance, zc_ringbuf_t *rb)
{
  transport_handle_t   *tp  = (transport_handle_t *)instance;
  transport_uart_ctx_t *ctx = (transport_uart_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    ESP_LOGE(TAG, "UART not opened");
    return -1;
  }

  if (rb == NULL) {
    ESP_LOGE(TAG, "rx buffer is NULL");
    return -1;
  }

  if (ctx->running) {
    ESP_LOGW(TAG, "RX already active");
    return 0;
  }

  tp->rx_buffer = rb;
  ctx->tp       = tp;
  ctx->running  = true;

#if CONFIG_IDF_TARGET_ESP32S3
  BaseType_t rc = xTaskCreatePinnedToCore(
                    transport_uart_rx_task,
                    "uart_rx_tp",
                    4096,
                    ctx,
                    4,
                    &ctx->rx_task,
                    UART_TASK_AFFINITY);
#else
  BaseType_t rc = xTaskCreate(
                    transport_uart_rx_task,
                    "uart_rx_tp",
                    4096,
                    ctx,
                    4,
                    &ctx->rx_task);
#endif

  if (rc != pdPASS) {
    ESP_LOGE(TAG, "Failed to create UART RX task");
    ctx->running = false;
    return -1;
  }

  ESP_LOGI(TAG, "UART RX activated");
  return 0;
}

static int transport_uart_deactivate_rx(void *instance)
{
  transport_handle_t   *tp  = (transport_handle_t *)instance;
  transport_uart_ctx_t *ctx = (transport_uart_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->running) {
    return 0;
  }

  ctx->running = false;

  if (ctx->rx_task != NULL) {
    const uint32_t step_ms = 5;
    uint32_t waited = 0;
    while (ctx->rx_task != NULL && waited < 1000) {
      vTaskDelay(pdMS_TO_TICKS(step_ms));
      waited += step_ms;
    }
    if (ctx->rx_task != NULL) {
      ESP_LOGW(TAG, "RX task did not exit in time");
    }
  }

  ESP_LOGI(TAG, "UART RX deactivated");
  return 0;
}

static int transport_uart_submit_tx(void *instance, const uint8_t *data, uint32_t len)
{
  if (instance == NULL || data == NULL) {
    return -1;
  }

  transport_handle_t   *tp  = (transport_handle_t *)instance;
  transport_uart_ctx_t *ctx = (transport_uart_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    ESP_LOGE(TAG, "UART not ready for TX");
    return -1;
  }

  int written = uart_write_bytes(ctx->config.port, (const char *)data, len);
  if (written < 0) {
    ESP_LOGE(TAG, "uart_write_bytes failed");
    return -1;
  }

  tp->stats_tx_bytes += (uint32_t)written;
  tp->tx_seq++;
  return written;
}

static int transport_uart_flush_tx(void *instance, uint32_t timeout_ms)
{
  if (instance == NULL) {
    return -1;
  }

  transport_handle_t   *tp  = (transport_handle_t *)instance;
  transport_uart_ctx_t *ctx = (transport_uart_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    return -1;
  }

  esp_err_t err = uart_wait_tx_done(ctx->config.port, pdMS_TO_TICKS(timeout_ms));
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_wait_tx_done failed: %s", esp_err_to_name(err));
    return -1;
  }

  return 0;
}

static bool transport_uart_is_ready(void *instance)
{
  if (instance == NULL) {
    return false;
  }

  transport_handle_t   *tp  = (transport_handle_t *)instance;
  transport_uart_ctx_t *ctx = (transport_uart_ctx_t *)tp->hardware_ctx;

  return (ctx != NULL) && ctx->opened;
}

#endif // CONFIG_TRANSPORT_LAYER_UART

