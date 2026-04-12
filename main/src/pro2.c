#include "device.h"
#include "pro2.h"
#include "utils.h"

#include "esp_mac.h"
#include "esp_random.h"

controller_device_t g_dev_controller = {
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
    .type = DEVICE_TYPE_PRO2,
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

static esp_err_t pro2_addr_init(nvs_handle_t nvs_handle) {
    esp_err_t ret;
    ret = nvs_get_blob(nvs_handle, NVS_KEY_DEVICE_ADDR, g_dev_controller.addr_re, &(size_t){ESP_BD_ADDR_LEN});
    if (ret != ESP_OK) {
        ESP_LOGW(LOG_BLE_NVS, "Failed to get Pro2 addr from NVS, will be generated...");
        esp_fill_random(g_dev_controller.addr + 3, 3);
        
        // save pro2 addr to nvs
        reverse_bytes(g_dev_controller.addr, g_dev_controller.addr_re, ESP_BD_ADDR_LEN);
        ret = nvs_set_blob(nvs_handle, NVS_KEY_DEVICE_ADDR, g_dev_controller.addr_re, ESP_BD_ADDR_LEN);
        if (ret != ESP_OK) {
          ESP_LOGE(LOG_BLE_NVS, "Failed to save Pro2 addr to NVS");
          return ret;
        }

        ret = nvs_commit(nvs_handle);
        if (ret != ESP_OK) {
          ESP_LOGE(LOG_BLE_NVS, "Failed to commit Pro2 addr to NVS");
          return ret;
        }
    } else {
        reverse_bytes(g_dev_controller.addr_re, g_dev_controller.addr, ESP_BD_ADDR_LEN);
        ESP_LOGI(LOG_BLE_NVS, "Pro2 addr loaded from NVS");
    }
    return ESP_OK;
}

static esp_err_t pro2_ltk_init(nvs_handle_t nvs_handle) {
  esp_err_t ret;
  ret = nvs_get_blob(nvs_handle, NVS_KEY_LTK, g_dev_controller.ltk, &(size_t){LTK_KEY_SIZE});
  if (ret != ESP_OK) {
    reverse_bytes(g_dev_controller.ltk, g_dev_controller.ltk_re, LTK_KEY_SIZE);
    ESP_LOGE(LOG_BLE_NVS, "Failed to get LTK from NVS");
  }
  return ret;
}

int pro2_device_init(nvs_handle_t nvs_handle) {
    esp_err_t ret;
    esp_err_t ltk_ret;
    ret = pro2_addr_init(nvs_handle);
    if (ret == ESP_OK) {
        // set esp ble stack mac addr, public address
        ret = esp_iface_mac_addr_set(g_dev_controller.addr, ESP_MAC_BT);
        if (ret != ESP_OK) {
            ESP_LOGE(LOG_APP, "Failed to set Pro2 addr %d", ret);
            return ret;
        }
    }

    ltk_ret = pro2_ltk_init(nvs_handle);
    if (ltk_ret == ESP_OK) {
        ESP_LOGI(LOG_APP, "device already paired.");
        log_print_ltk_hex("LTK", g_dev_controller.ltk);
    }
    return ret;
}

static int pro2_pairing_info_nvs_save() {
  nvs_handle_t nvs_handle;
  esp_err_t ret;
  ret = nvs_open(NVS_NAME_PAIRING, NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_BLE_NVS, "Failed to open NVS namespace: %s", NVS_NAME_PAIRING);
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_set_blob(nvs_handle, NVS_KEY_LTK, g_dev_controller.ltk, LTK_KEY_SIZE);
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_BLE_NVS, "Failed to save LTK to NVS");
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_set_blob(nvs_handle, NVS_KEY_HOST_ADDR, g_dev_ns2.ble_addr.val, ESP_BD_ADDR_LEN);
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_BLE_NVS, "Failed to save NS2 addr to NVS");
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_commit(nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_BLE_NVS, "Failed to commit Pairing Info to NVS");
  }
  nvs_close(nvs_handle);
  return ret;
}

static void pro2_global_ltk_sec_init() {
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
  memcpy(g_ltk_sec->ltk, g_dev_controller.ltk_re, LTK_KEY_SIZE);
  g_ltk_sec->ltk_present = 1;
  g_ltk_sec->peer_addr.type = BLE_ADDR_PUBLIC;
  // use little endian addr
  memcpy(g_ltk_sec-> peer_addr.val, g_dev_ns2.ble_addr.val, ESP_BD_ADDR_LEN);
  // NS2 rand_num and ediv are both 0
  g_ltk_sec->rand_num = 0;
  g_ltk_sec->ediv = 0;
  // g_ltk_sec->csrk_present = 0;
  // g_ltk_sec->irk_present = 0;
  g_ltk_sec->authenticated = 1;
  g_ltk_sec->sc = 1;
}

int pro2_inject_pairing_info_to_ble_context() {
  int rc = 0;
  // init esp ble ltk sec
  pro2_global_ltk_sec_init();
  // write ltk to ble context
  rc = ble_store_write_our_sec(g_ltk_sec);
  if (rc != 0) {
      ESP_LOGE(LOG_APP, "ble_store_write_our_sec failed");
      return rc;
  }
  rc = ble_store_write_peer_sec(g_ltk_sec);
  if (rc != 0) {
      ESP_LOGE(LOG_APP, "ble_store_write_peer_sec failed");
      return rc;
  }

  // manual binding, execute initial binding logic
  ble_gatts_bonding_established(g_dev_ns2.conn_handle);

  return rc;
}

int pro2_pairing_info_save() {
  return pro2_pairing_info_nvs_save();
}