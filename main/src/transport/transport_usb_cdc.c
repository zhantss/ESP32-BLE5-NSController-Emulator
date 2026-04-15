#include "transport/transport_usb_cdc.h"

#ifdef CONFIG_TRANSPORT_LAYER_USB_CDC

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tusb.h"

#define TAG "transport_usb_cdc"

#define USB_CDC_RX_TIMEOUT_MS   10

/* Keep BLE host on Core 0; move USB/TinyUSB workload to Core 1. */
#if CONFIG_IDF_TARGET_ESP32S3
  #define USB_CDC_TASK_AFFINITY   1
#else
  #define USB_CDC_TASK_AFFINITY   tskNO_AFFINITY
#endif

typedef struct {
  transport_usb_cdc_config_t config;
  transport_handle_t          *tp;
  TaskHandle_t                 rx_task;
  volatile bool                running;
  volatile bool                opened;
} transport_usb_cdc_ctx_t;

/* -------------------------------------------------------------------------- */
/*  USB Descriptors (hardcoded from manufacturer_data)                        */
/* -------------------------------------------------------------------------- */

/*
 * Hardcoded VID/PID from controller manufacturer_data:
 *   Vendor Id  = 0x057E (Nintendo)
 *   Product ID = 0x2069 (Pro2)
 */
static const tusb_desc_device_t usb_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = 0x01,          /* TUSB_DESC_DEVICE */
    .bcdUSB             = 0x0200,        /* USB 2.0 */
    .bDeviceClass       = 0xEF,          /* TUSB_CLASS_MISC */
    .bDeviceSubClass    = 0x02,          /* MISC_SUBCLASS_COMMON */
    .bDeviceProtocol    = 0x01,          /* MISC_PROTOCOL_IAD */
    .bMaxPacketSize0    = 64,            /* CFG_TUD_ENDPOINT0_SIZE */
    .idVendor           = 0x057E,        /* Nintendo */
    .idProduct          = 0x2069,        /* Pro2 */
    .bcdDevice          = 0x0100,        /* Device version: no data available, using 1.00 */
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

static const char *usb_string_descriptor[] = {
    (const char[]){0x09, 0x04}, /* 0: Language ID (English US) */
    "Nintendo",                  /* 1: Manufacturer */
    "Pro2",                      /* 2: Product */
    "000000",                    /* 3: Serial number -- no data in manufacturer_data, placeholder */
};

/* -------------------------------------------------------------------------- */
/*  Forward declarations                                                      */
/* -------------------------------------------------------------------------- */
static int  transport_usb_cdc_open(void *instance, void *config);
static void transport_usb_cdc_close(void *instance);
static int  transport_usb_cdc_activate_rx(void *instance, zc_ringbuf_t *rb);
static int  transport_usb_cdc_deactivate_rx(void *instance);
static int  transport_usb_cdc_submit_tx(void *instance, const uint8_t *data, uint32_t len);
static int  transport_usb_cdc_flush_tx(void *instance, uint32_t timeout_ms);
static bool transport_usb_cdc_is_ready(void *instance);

const transport_vtable_t transport_usb_cdc_vtable = {
  .name           = "usb_cdc",
  .open           = transport_usb_cdc_open,
  .close          = transport_usb_cdc_close,
  .activate_rx    = transport_usb_cdc_activate_rx,
  .deactivate_rx  = transport_usb_cdc_deactivate_rx,
  .submit_tx      = transport_usb_cdc_submit_tx,
  .flush_tx       = transport_usb_cdc_flush_tx,
  .is_ready       = transport_usb_cdc_is_ready,
};

/* -------------------------------------------------------------------------- */
/*  RX Task                                                                   */
/* -------------------------------------------------------------------------- */
static void transport_usb_cdc_rx_task(void *arg)
{
  transport_usb_cdc_ctx_t *ctx = (transport_usb_cdc_ctx_t *)arg;
  transport_handle_t      *tp  = ctx->tp;

  while (ctx->running) {
    if (!tud_cdc_connected()) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    uint8_t *ptr = NULL;
    uint32_t avail = zc_reserve(tp->rx_buffer, &ptr);
    if (avail == 0) {
      uint8_t drain[64];
      size_t drain_len = 0;
      esp_err_t err = tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0, drain, sizeof(drain), &drain_len);
      if (err == ESP_OK && drain_len > 0) {
        tp->stats_rx_overflow += (uint32_t)drain_len;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    size_t rx_size = 0;
    esp_err_t err = tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0, ptr, avail, &rx_size);
    if (err == ESP_OK && rx_size > 0) {
      zc_commit(tp->rx_buffer, (uint32_t)rx_size);
      tp->stats_rx_bytes += (uint32_t)rx_size;
      tp->rx_seq++;

      if (ctx->config.notify_task != NULL) {
        xTaskNotifyGive(ctx->config.notify_task);
      }
    } else {
      /* Yield briefly when no data to prevent busy-looping. */
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  ctx->rx_task = NULL;
  vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------- */
/*  VTable implementations                                                    */
/* -------------------------------------------------------------------------- */
static int transport_usb_cdc_open(void *instance, void *config)
{
  transport_handle_t *tp = (transport_handle_t *)instance;

  if (tp == NULL || config == NULL) {
    ESP_LOGE(TAG, "invalid arguments");
    return -1;
  }

  transport_usb_cdc_ctx_t *ctx = (transport_usb_cdc_ctx_t *)tp->hardware_ctx;
  if (ctx != NULL && ctx->opened) {
    ESP_LOGW(TAG, "USB CDC already opened");
    return 0;
  }

  ctx = (transport_usb_cdc_ctx_t *)malloc(sizeof(transport_usb_cdc_ctx_t));
  if (ctx == NULL) {
    ESP_LOGE(TAG, "failed to allocate context");
    return -1;
  }
  memset(ctx, 0, sizeof(transport_usb_cdc_ctx_t));

  memcpy(&ctx->config, config, sizeof(transport_usb_cdc_config_t));

  const tinyusb_config_t tusb_cfg = {
    .port = TINYUSB_PORT_FULL_SPEED_0,
    .task = {
      .size     = 4096,
      .priority = 5,
#if CONFIG_IDF_TARGET_ESP32S3
      .xCoreID  = 1,
#else
      .xCoreID  = tskNO_AFFINITY,
#endif
    },
    .descriptor = {
      .device           = &usb_device_descriptor,
      .qualifier        = NULL,          /* Full-speed only, no qualifier needed */
      .string           = usb_string_descriptor,
      .string_count     = sizeof(usb_string_descriptor) / sizeof(usb_string_descriptor[0]),
      .full_speed_config = NULL,         /* Using TinyUSB default CDC descriptor */
      .high_speed_config = NULL,         /* ESP32-S3 USB OTG 1.1 full-speed only */
    },
  };

  esp_err_t err = tinyusb_driver_install(&tusb_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(err));
    free(ctx);
    return -1;
  }

  tinyusb_config_cdcacm_t acm_cfg = {
    .cdc_port  = TINYUSB_CDC_ACM_0,
    .callback_rx = NULL,
    .callback_rx_wanted_char = NULL,
    .callback_line_state_changed = NULL,
    .callback_line_coding_changed = NULL,
  };

  err = tinyusb_cdcacm_init(&acm_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "tinyusb_cdcacm_init failed: %s", esp_err_to_name(err));
    tinyusb_driver_uninstall();
    free(ctx);
    return -1;
  }

  ctx->opened = true;
  tp->hardware_ctx = ctx;

  ESP_LOGI(TAG, "USB CDC opened: rx_buf=%d, tx_buf=%d",
           ctx->config.rx_buffer_size, ctx->config.tx_buffer_size);
  return 0;
}

static void transport_usb_cdc_close(void *instance)
{
  if (instance == NULL) {
    return;
  }

  transport_handle_t      *tp  = (transport_handle_t *)instance;
  transport_usb_cdc_ctx_t *ctx = (transport_usb_cdc_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    return;
  }

  transport_usb_cdc_deactivate_rx(instance);

  vTaskDelay(pdMS_TO_TICKS(20));

  tinyusb_cdcacm_deinit(TINYUSB_CDC_ACM_0);
  tinyusb_driver_uninstall();
  ctx->opened = false;
  tp->rx_buffer = NULL;

  ESP_LOGI(TAG, "USB CDC closed");

  free(ctx);
  tp->hardware_ctx = NULL;
}

static int transport_usb_cdc_activate_rx(void *instance, zc_ringbuf_t *rb)
{
  if (instance == NULL) {
    return -1;
  }

  transport_handle_t      *tp  = (transport_handle_t *)instance;
  transport_usb_cdc_ctx_t *ctx = (transport_usb_cdc_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    ESP_LOGE(TAG, "USB CDC not opened");
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
                    transport_usb_cdc_rx_task,
                    "usb_cdc_rx_tp",
                    4096,
                    ctx,
                    4,
                    &ctx->rx_task,
                    USB_CDC_TASK_AFFINITY);
#else
  BaseType_t rc = xTaskCreate(
                    transport_usb_cdc_rx_task,
                    "usb_cdc_rx_tp",
                    4096,
                    ctx,
                    4,
                    &ctx->rx_task);
#endif

  if (rc != pdPASS) {
    ESP_LOGE(TAG, "Failed to create USB CDC RX task");
    ctx->running = false;
    return -1;
  }

  ESP_LOGI(TAG, "USB CDC RX activated");
  return 0;
}

static int transport_usb_cdc_deactivate_rx(void *instance)
{
  if (instance == NULL) {
    return 0;
  }

  transport_handle_t      *tp  = (transport_handle_t *)instance;
  transport_usb_cdc_ctx_t *ctx = (transport_usb_cdc_ctx_t *)tp->hardware_ctx;

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

  ESP_LOGI(TAG, "USB CDC RX deactivated");
  return 0;
}

static int transport_usb_cdc_submit_tx(void *instance, const uint8_t *data, uint32_t len)
{
  if (instance == NULL || data == NULL) {
    return -1;
  }

  transport_handle_t      *tp  = (transport_handle_t *)instance;
  transport_usb_cdc_ctx_t *ctx = (transport_usb_cdc_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    ESP_LOGE(TAG, "USB CDC not ready for TX");
    return -1;
  }

  if (!tud_cdc_connected()) {
    ESP_LOGW(TAG, "USB CDC not connected to host");
    return -1;
  }

  size_t written = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, data, len);
  if (written < len) {
    ESP_LOGW(TAG, "USB CDC write queue truncated: %d/%d", (int)written, (int)len);
  }

  esp_err_t err = tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(100));
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "tinyusb_cdcacm_write_flush failed: %s", esp_err_to_name(err));
  }

  tp->stats_tx_bytes += (uint32_t)written;
  tp->tx_seq++;
  return (int)written;
}

static int transport_usb_cdc_flush_tx(void *instance, uint32_t timeout_ms)
{
  if (instance == NULL) {
    return -1;
  }

  transport_handle_t      *tp  = (transport_handle_t *)instance;
  transport_usb_cdc_ctx_t *ctx = (transport_usb_cdc_ctx_t *)tp->hardware_ctx;

  if (ctx == NULL || !ctx->opened) {
    return -1;
  }

  esp_err_t err = tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(timeout_ms));
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "tinyusb_cdcacm_write_flush failed: %s", esp_err_to_name(err));
    return -1;
  }

  return 0;
}

static bool transport_usb_cdc_is_ready(void *instance)
{
  if (instance == NULL) {
    return false;
  }

  transport_handle_t      *tp  = (transport_handle_t *)instance;
  transport_usb_cdc_ctx_t *ctx = (transport_usb_cdc_ctx_t *)tp->hardware_ctx;

  return (ctx != NULL) && ctx->opened && tud_cdc_connected();
}

#endif // CONFIG_TRANSPORT_LAYER_USB_CDC
