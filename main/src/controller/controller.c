#include "device.h"
#include "controller/controller.h"
#include "utils.h"
#include "pro2.h"

#include "esp_log.h"

controller_firmware_t g_controller_firmware = {
    // 78:81:8c -> Nintendo (Area HK)
    .addr = { 0x78, 0x81, 0x8c, 0x00, 0x00, 0x00 },
    // not initialized
    .addr_re = { 0 },
    .ltk = { 0 },
    .ltk_re = { 0 },
    .ltk_key_b1 = {
        0x5C, 0xF6, 0xEE, 0x79, 0x2C, 0xDF, 0x05, 0xE1,
        0xBA, 0x2B, 0x63, 0x25, 0xC4, 0x1A, 0x5F, 0x10
    },
    .type = CONTROLLER_TYPE_PRO2,
    .manufacturer_data = {
        0x53, 0x05,                                 // Manufacturer ID, Nintendo
        0x01, 0x00, 0x03,                           // fixed, Maybe Version
        0x7E, 0x05,                                 // Vendor Id, Nintendo
        0x69, 0x20,                                 // Product ID, Pro2
        0x00, 0x01,                                 // fixed, Maybe Placeholder
        0x00,                                       // 0x81 -> wake adv, 0x00 otherwise
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,         // ns2 address, little endian
        0x0F,                                       // fixed
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00    // placeholder
    }
};


static void ltk_sec_init() {
  if (g_ltk_sec == NULL) {
      g_ltk_sec = (struct ble_store_value_sec *)malloc(sizeof(struct ble_store_value_sec));
      if (g_ltk_sec == NULL) {
          ESP_LOGE(LOG_APP, "malloc failed");
          return;
      }
  }
  g_ltk_sec->bond_count = 1;
  g_ltk_sec->key_size = LTK_KEY_SIZE;
  // use little endian ltk
  memcpy(g_ltk_sec->ltk, g_controller_firmware.ltk_re, LTK_KEY_SIZE);
  g_ltk_sec->ltk_present = 1;
  g_ltk_sec->peer_addr.type = BLE_ADDR_PUBLIC;
  // use little endian addr
  memcpy(g_ltk_sec-> peer_addr.val, g_console_ns2.ble_addr.val, ESP_BD_ADDR_LEN);
  // NS2 rand_num and ediv are both 0
  g_ltk_sec->rand_num = 0;
  g_ltk_sec->ediv = 0;
  // g_ltk_sec->csrk_present = 0;
  // g_ltk_sec->irk_present = 0;
  g_ltk_sec->authenticated = 1;
  g_ltk_sec->sc = 1;
}

int inject_pairing_info_to_ble_ctx() {
  int rc = 0;
  // init esp ble ltk sec
  ltk_sec_init();
  // write ltk to ble context
  rc = ble_store_write_our_sec(g_ltk_sec);
  if (rc != 0) {
      ESP_LOGE(LOG_APP, "ble_store_write_our_sec failed, rc=%d", rc);
      return rc;
  }
  ESP_LOGI(LOG_APP, "Injecting LTK for peer addr:");
  log_print_addr(g_ltk_sec->peer_addr.val);

  rc = ble_store_write_peer_sec(g_ltk_sec);
  if (rc != 0) {
      ESP_LOGE(LOG_APP, "ble_store_write_peer_sec failed, rc=%d", rc);
      return rc;
  }

  // TIP: Maybe not necessary to call this function
  // manual binding, execute initial binding logic
  // ble_gatts_bonding_established(g_console_ns2.conn_handle);

  return rc;
}

void controller_type_init(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    uint8_t type = CONTROLLER_TYPE_PRO2;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_open(NVS_NAME_CONFIG, NVS_READONLY, &nvs_handle);
    if (ret == ESP_OK) {
        ret = nvs_get_u8(nvs_handle, NVS_KEY_CONTROLLER_TYPE, &type);
        if (ret != ESP_OK) {
            ESP_LOGW(LOG_BLE_NVS, "Controller type not found in NVS, defaulting to PRO2");
            type = CONTROLLER_TYPE_PRO2;
        } else {
            ESP_LOGI(LOG_BLE_NVS, "Controller type loaded from NVS: %d", type);
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGW(LOG_BLE_NVS, "Failed to open NVS config namespace, defaulting to PRO2");
    }

    g_controller_firmware.type = type;
}

int controller_init(nvs_handle_t nvs_handle) {
    if (g_controller_firmware.type == CONTROLLER_TYPE_PRO2) {
        return pro2_device_init(nvs_handle);
    }
    ESP_LOGE(LOG_APP, "JoyCon device init not implemented");
    return ESP_FAIL;
}

int controller_pairing_info_save(void) {
    if (g_controller_firmware.type == CONTROLLER_TYPE_PRO2) {
        return pro2_pairing_info_save();
    }
    ESP_LOGE(LOG_APP, "JoyCon pairing info save not implemented");
    return -1;
}