#include "transport/transport_usb_serial_jtag.h"

#ifdef CONFIG_TRANSPORT_LAYER_USB_SERIAL_JTAG

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"

#define TAG "transport_usb_serial_jtag"

#define USJ_RX_TIMEOUT_MS   10

#if CONFIG_IDF_TARGET_ESP32S3
  #define USJ_TASK_AFFINITY   0
#else
  #define USJ_TASK_AFFINITY   tskNO_AFFINITY
#endif

typedef struct {
  transport_usb_serial_jtag_config_t config;
  transport_handle_t                  *tp;
  TaskHandle_t                         rx_task;
  volatile bool                        running;
  volatile bool                        opened;
} transport_usb_serial_jtag_ctx_t;

/* -------------------------------------------------------------------------- */
/*  Forward declarations                                                      */
/* -------------------------------------------------------------------------- */
static int  transport_usb_serial_jtag_open(void *instance, void *config);
static void transport_usb_serial_jtag_close(void *instance);
static int  transport_usb_serial_jtag_activate_rx(void *instance, zc_ringbuf_t *rb);
static int  transport_usb_serial_jtag_deactivate_rx(void *instance);
static int  transport_usb_serial_jtag_submit_tx(void *instance, const uint8_t *data, uint32_t len);
static int  transport_usb_serial_jtag_flush_tx(void *instance, uint32_t timeout_ms);
static bool transport_usb_serial_jtag_is_ready(void *instance);

const transport_vtable_t transport_usb_serial_jtag_vtable = {
  .name           = "usb_serial_jtag",
  .open           = transport_usb_serial_jtag_open,
  .close          = transport_usb_serial_jtag_close,
  .activate_rx    = transport_usb_serial_jtag_activate_rx,
  .deactivate_rx  = transport_usb_serial_jtag_deactivate_rx,
  .submit_tx      = transport_usb_serial_jtag_submit_tx,
  .flush_tx       = transport_usb_serial_jtag_flush_tx,
  .is_ready       = transport_usb_serial_jtag_is_ready,
};

/* -------------------------------------------------------------------------- */
/*  RX Task                                                                   */
/* -------------------------------------------------------------------------- */
static void transport_usb_serial_jtag_rx_task(void *arg)
{
  transport_usb_serial_jtag_ctx_t *ctx = (transport_usb_serial_jtag_ctx_t *)arg;
  transport_handle_t              *tp  = ctx->tp;

  while (ctx->running) {
    if (!usb_serial_jtag_is_connected()) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    uint8_t *ptr = NULL;
    uint32_t avail = zc_reserve(tp->rx_buffer, &ptr);
    if (avail == 0) {
      uint8_t drain[64];
      int drain_len = usb_serial_jtag_read_bytes(drain, sizeof(drain),
                                                  pdMS_TO_TICKS(USJ_RX_TIMEOUT_MS));
      if (drain_len > 0) {
        tp->stats_rx_overflow += (uint32_t)drain_len;
      }
      continue;
    }

    int len = usb_serial_jtag_read_bytes(ptr, avail, pdMS_TO_TICKS(USJ_RX_TIMEOUT_MS));
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
static int transport_usb_serial_jtag_open(void *instance, void *config)
{
  transport_handle_t *tp = (transport_handle_t *)instance;

  if (tp == NULL || config == NULL) {
    ESP_LOGE(TAG, "invalid arguments");
    return -1;
  }

  transport_usb_serial_jtag_ctx_t *ctx = (transport_usb_serial_jtag_ctx_t *)tp->hardware_ctx;
  if (ctx != NULL && ctx->opened) {
    ESP_LOGW(TAG, "USB Serial/JTAG already opened");
    return 0;
  }

  ctx = (transport_usb_serial_jtag_ctx_t *)malloc(sizeof(transport_usb_serial_jtag_ctx_t));
  if (ctx == NULL) {
    ESP_LOGE(TAG, "failed to allocate context");
    return -1;
  }
  memset(ctx, 0, sizeof(transport_usb_serial_jtag_ctx_t));

  memcpy(&ctx->config, config, sizeof(transport_usb_serial_jtag_config_t));

  usb_serial_jtag_driver_config_t driver_cfg = {
    .rx_buffer_size = ctx->config.rx_buffer_size,
    .tx_buffer_size = ctx->config.tx_buffer_size,
  };

  esp_err_t err = usb_serial_jtag_driver_install(&driver_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "usb_serial_jtag_driver_install failed: %s", esp_err_to_name(err));
    free(ctx);
    return -1;
  }

  ctx->opened = true;
  tp->hardware_ctx = ctx;

  ESP_LOGI(TAG, "USB Serial/JTAG opened: rx_buf=%d, tx_buf=%d",
           ctx->config.rx_buffer_size, ctx->config.tx_buffer_size);
  return 0;
}

static void transport_usb_serial_jtag_close(void *instance)
{
  if (instance == NULL) {
    return;
  }

  transport_handle_t              *tp  = (transport_handle_t *)instance;
  transport_usb_serial_jtag_ctx_t *ctx = (transport_usb_serial_jtag_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    return;
  }

  transport_usb_serial_jtag_deactivate_rx(instance);

  vTaskDelay(pdMS_TO_TICKS(20));

  usb_serial_jtag_driver_uninstall();
  ctx->opened = false;
  tp->rx_buffer = NULL;

  ESP_LOGI(TAG, "USB Serial/JTAG closed");

  free(ctx);
  tp->hardware_ctx = NULL;
}

static int transport_usb_serial_jtag_activate_rx(void *instance, zc_ringbuf_t *rb)
{
  if (instance == NULL) {
    return -1;
  }

  transport_handle_t              *tp  = (transport_handle_t *)instance;
  transport_usb_serial_jtag_ctx_t *ctx = (transport_usb_serial_jtag_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    ESP_LOGE(TAG, "USB Serial/JTAG not opened");
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
                    transport_usb_serial_jtag_rx_task,
                    "usj_rx_tp",
                    4096,
                    ctx,
                    4,
                    &ctx->rx_task,
                    USJ_TASK_AFFINITY);
#else
  BaseType_t rc = xTaskCreate(
                    transport_usb_serial_jtag_rx_task,
                    "usj_rx_tp",
                    4096,
                    ctx,
                    4,
                    &ctx->rx_task);
#endif

  if (rc != pdPASS) {
    ESP_LOGE(TAG, "Failed to create USB Serial/JTAG RX task");
    ctx->running = false;
    return -1;
  }

  ESP_LOGI(TAG, "USB Serial/JTAG RX activated");
  return 0;
}

static int transport_usb_serial_jtag_deactivate_rx(void *instance)
{
  if (instance == NULL) {
    return 0;
  }

  transport_handle_t              *tp  = (transport_handle_t *)instance;
  transport_usb_serial_jtag_ctx_t *ctx = (transport_usb_serial_jtag_ctx_t *)tp->hardware_ctx;

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

  ESP_LOGI(TAG, "USB Serial/JTAG RX deactivated");
  return 0;
}

static int transport_usb_serial_jtag_submit_tx(void *instance, const uint8_t *data, uint32_t len)
{
  if (instance == NULL || data == NULL) {
    return -1;
  }

  transport_handle_t              *tp  = (transport_handle_t *)instance;
  transport_usb_serial_jtag_ctx_t *ctx = (transport_usb_serial_jtag_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    ESP_LOGE(TAG, "USB Serial/JTAG not ready for TX");
    return -1;
  }

  if (!usb_serial_jtag_is_connected()) {
    ESP_LOGW(TAG, "USB Serial/JTAG not connected to host");
    return -1;
  }

  int written = usb_serial_jtag_write_bytes(data, len, pdMS_TO_TICKS(100));
  if (written < 0) {
    ESP_LOGE(TAG, "usb_serial_jtag_write_bytes failed");
    return -1;
  }

  tp->stats_tx_bytes += (uint32_t)written;
  tp->tx_seq++;
  return written;
}

static int transport_usb_serial_jtag_flush_tx(void *instance, uint32_t timeout_ms)
{
  if (instance == NULL) {
    return -1;
  }

  transport_handle_t              *tp  = (transport_handle_t *)instance;
  transport_usb_serial_jtag_ctx_t *ctx = (transport_usb_serial_jtag_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    return -1;
  }

  (void)timeout_ms;
  /* ESP-IDF USB Serial/JTAG driver does not provide a synchronous flush API. */
  return 0;
}

static bool transport_usb_serial_jtag_is_ready(void *instance)
{
  if (instance == NULL) {
    return false;
  }

  transport_handle_t              *tp  = (transport_handle_t *)instance;
  transport_usb_serial_jtag_ctx_t *ctx = (transport_usb_serial_jtag_ctx_t *)tp->hardware_ctx;

  return (ctx != NULL) && ctx->opened && usb_serial_jtag_is_connected();
}

#endif // CONFIG_TRANSPORT_LAYER_USB_SERIAL_JTAG
