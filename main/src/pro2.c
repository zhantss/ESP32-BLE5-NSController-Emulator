#include "device.h"
#include "pro2.h"
#include "utils.h"
#include "ns2_codec.h"

#include "esp_mac.h"
#include "esp_random.h"

static esp_err_t pro2_addr_init(nvs_handle_t nvs_handle) {
    esp_err_t ret;
    ret = nvs_get_blob(nvs_handle, NVS_KEY_CTRL_ADDR, g_controller_firmware.addr_re, &(size_t){ESP_BD_ADDR_LEN});
    if (ret != ESP_OK) {
        ESP_LOGW(LOG_BLE_NVS, "Failed to get Pro2 addr from NVS, will be generated...");
        esp_fill_random(g_controller_firmware.addr + 3, 3);
        
        // save pro2 addr to nvs
        reverse_bytes(g_controller_firmware.addr, g_controller_firmware.addr_re, ESP_BD_ADDR_LEN);
        ret = nvs_set_blob(nvs_handle, NVS_KEY_CTRL_ADDR, g_controller_firmware.addr_re, ESP_BD_ADDR_LEN);
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
        reverse_bytes(g_controller_firmware.addr_re, g_controller_firmware.addr, ESP_BD_ADDR_LEN);
        ESP_LOGI(LOG_BLE_NVS, "Pro2 addr loaded from NVS");
    }
    return ESP_OK;
}

static esp_err_t pro2_ltk_init(nvs_handle_t nvs_handle) {
  esp_err_t ret;
  ret = nvs_get_blob(nvs_handle, NVS_KEY_LTK, g_controller_firmware.ltk, &(size_t){LTK_KEY_SIZE});
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_BLE_NVS, "Failed to get LTK from NVS");
  } else {
    reverse_bytes(g_controller_firmware.ltk, g_controller_firmware.ltk_re, LTK_KEY_SIZE);
    ESP_LOGI(LOG_BLE_NVS, "LTK loaded and reversed successfully");
  }
  return ret;
}

int pro2_device_init(nvs_handle_t nvs_handle) {
    esp_err_t ret;
    esp_err_t ltk_ret;
    ret = pro2_addr_init(nvs_handle);
    if (ret == ESP_OK) {
        // set esp ble stack mac addr, public address
        ret = esp_iface_mac_addr_set(g_controller_firmware.addr, ESP_MAC_BT);
        if (ret != ESP_OK) {
            ESP_LOGE(LOG_APP, "Failed to set Pro2 addr %d", ret);
            return ret;
        }
    }

    ltk_ret = pro2_ltk_init(nvs_handle);
    if (ltk_ret == ESP_OK) {
        ESP_LOGI(LOG_APP, "device already paired.");
        log_print_ltk_hex("LTK", g_controller_firmware.ltk);
    }

    const uint8_t version[3] = {0x01, 0x06, 0x01};
    const uint8_t body_color[3] = {0x23, 0x23, 0x23};
    const uint8_t buttons_color[3] = {0x63, 0xB9, 0x7A};        // pokemon za green
    const uint8_t highlight_color[3] = {0xE6, 0xE6, 0xE6};
    const uint8_t grip_color[3] = {0x32, 0x32, 0x32};
    set_controller_specific(
        0x2069,
        (const uint8_t *)"HEJ71001123456", 14,
        version,
        body_color,
        buttons_color,
        highlight_color,
        grip_color
    );
    g_controller_firmware.manufacturer_data[7] = 0x69;
    g_controller_firmware.manufacturer_data[8] = 0x20;

    return ret;
}

static int pro2_pairing_info_nvs_save() {
  nvs_handle_t nvs_handle;
  esp_err_t ret;
  const char* pairing_ns = (g_controller_firmware.type == CONTROLLER_TYPE_PRO2) ? NVS_NAME_PAIRING_PRO2 : NVS_NAME_PAIRING_JC;
  ret = nvs_open(pairing_ns, NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_BLE_NVS, "Failed to open NVS namespace: %s", pairing_ns);
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_set_blob(nvs_handle, NVS_KEY_LTK, g_controller_firmware.ltk, LTK_KEY_SIZE);
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_BLE_NVS, "Failed to save LTK to NVS");
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_set_blob(nvs_handle, NVS_KEY_HOST_ADDR, g_console_ns2.ble_addr.val, ESP_BD_ADDR_LEN);
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

int pro2_pairing_info_save() {
  #ifdef CONFIG_SAVE_PAIRING_INFO
    return pro2_pairing_info_nvs_save();
  #endif
  return 0;
}