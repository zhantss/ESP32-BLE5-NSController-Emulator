#include "device.h"
#include "pro2.h"
#include "utils.h"

#include "esp_mac.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_gap.h"
#include "host/ble_att.h"
#include "host/util/util.h"

dev_status_t g_status = DEV_BOOT;

struct ble_store_value_sec* g_ltk_sec = NULL;

uint8_t g_adv_opcode = 0x00;

// **************** NVS ****************

static void nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static esp_err_t ns2_addr_init(nvs_handle_t nvs_handle) {
  esp_err_t ret;
  ret = nvs_get_blob(nvs_handle, NVS_KEY_HOST_ADDR, g_dev_ns2.ble_addr.val, &(size_t){ESP_BD_ADDR_LEN});
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_BLE_NVS, "Failed to get NS2 addr from NVS");
  } else {
    ESP_LOGI(LOG_BLE_NVS, "NS2 addr loaded from NVS");
  }
  return ret;
}

static esp_err_t device_info_init() {
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    ret = nvs_open(NVS_NAME_PAIRING, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(LOG_BLE_NVS, "Failed to open NVS namespace: %s", NVS_NAME_PAIRING);
        return ret;
    }

    if (g_dev_controller.type == DEVICE_TYPE_PRO2) {
      ret = pro2_device_init(nvs_handle);
      if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
      }
    }

    ret = ns2_addr_init(nvs_handle);
    if (ret == ESP_OK) {
        g_dev_ns2.ble_addr.type = BLE_ADDR_PUBLIC;
        // set ns2 address to manufacturer data (little endian)
        memcpy(&g_dev_controller.manufacturer_data[12], g_dev_ns2.ble_addr.val, ESP_BD_ADDR_LEN);
    }
    nvs_close(nvs_handle);
    return ret;
}

// **************** BLE Stack ****************

static void bleprph_on_reset(int reason) {
  ESP_LOGW(LOG_APP, "BLE Host Resetting state; reason=%d", reason);
}

static uint8_t own_addr_type;

static void bleprph_on_sync(void) {
  ESP_ERROR_CHECK(ble_hs_util_ensure_addr(0));
  ESP_ERROR_CHECK(ble_hs_id_infer_auto(0, &own_addr_type));
  log_print_addr(g_dev_controller.addr_re);

  // already paired, inject pairing info to BLE context
  if (g_dev_controller.ltk[0] != 0) {
    int rc = pro2_inject_pairing_info_to_ble_context();
    if (rc != 0) {
      ESP_LOGE(LOG_APP, "Failed to inject pairing info to BLE context");
    } else {
      ESP_LOGI(LOG_APP, "Pairing info injected to BLE context");
    }
  }

  ESP_ERROR_CHECK(
    ble_gap_set_prefered_default_le_phy(BLE_HCI_LE_PHY_2M_PREF_MASK, BLE_HCI_LE_PHY_2M_PREF_MASK)
  );

  ble_advertise();
}

void host_task(void *param) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}

void ble_stack_init(void) {
    esp_err_t ret;
    nvs_init();

    // BLE Security
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_oob_data_flag = 0;
    ble_hs_cfg.sm_bonding = 1;            // Enable ESP-IDF Bonding Store Framework
    // Disable Standard Bonding process
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;
    ble_hs_cfg.sm_sc_only = 0;
    ble_hs_cfg.sm_sec_lvl = 2;
    ble_hs_cfg.sm_keypress = 0;
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;

    ESP_LOGI(LOG_APP, "Device info init...");
    ret = device_info_init();
    if (ret == ESP_OK) {
      ESP_LOGI(LOG_APP, "Device info initialized");
      // set wake up mode
      g_adv_opcode = 0x81;
    } else {
      ESP_LOGW(LOG_APP, "No Pairing, will be paired later");
    }

    // nimble init
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(LOG_APP, "Failed to init nimble %d ", ret);
        return;
    }
    ret = ble_att_set_preferred_mtu(512);
    if (ret != 0) {
      ESP_LOGE(LOG_APP, "ble_att_set_preferred_mtu() failed %d ", ret);
      return;
    }

    // BLE Callback
    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.gatts_register_arg = device_gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ESP_ERROR_CHECK(device_gatt_svr_init());

    // Bonding Store - set custom callbacks BEFORE calling ble_store_config_init()
    // This is required because ble_hs_init() may trigger store operations
    ble_hs_cfg.store_read_cb = custom_store_config_read;
    ble_hs_cfg.store_write_cb = custom_store_config_write;
    ble_store_config_init();

    // Nimble Start
    nimble_port_freertos_init(host_task);

    // TODO SCLI
}

// **************** BLE Advertise ****************

static uint8_t instance = 0;

/**
 * legacy advertising, not used
 */
#if 0
static void ble_advertise_normal() {
  int rc;
  // reset device status
  if (g_dev_controller.type == DEVICE_TYPE_JOYCON) {
    // TODO Joycon
    ESP_LOGE(LOG_APP, "Joycon not implemented");
    return;
  }
  g_status = DEV_ADV_IND;

  if (ble_gap_adv_active()) {
    ESP_LOGI(LOG_APP, "Advertising instance already active");
    return;
  }
  struct ble_gap_adv_params adv_params;
  
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
  adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MIN;

  // set manufacturer data
  ESP_LOGI(LOG_APP, "Setting manufacturer data for advertising");
  uint8_t m_head[3] = { 0x02, 0x01, 0x06 };
  uint8_t m_size = sizeof(g_dev_controller.manufacturer_data) + 1; // 27
  uint8_t m_spec[2] = { m_size, 0xFF };
  uint8_t adv_data[sizeof(m_head) + sizeof(m_spec) + sizeof(g_dev_controller.manufacturer_data)];
  memcpy(adv_data, m_head, sizeof(m_head));
  memcpy(adv_data + sizeof(m_head), m_spec, sizeof(m_spec));
  // TODO test wakeup flag
  if (g_adv_opcode != 0x00) {
    g_dev_controller.manufacturer_data[11] = g_adv_opcode;
  }
  memcpy(adv_data + sizeof(m_head) + sizeof(m_spec), 
         g_dev_controller.manufacturer_data, 
         sizeof(g_dev_controller.manufacturer_data));

  rc = ble_gap_adv_set_data(adv_data, sizeof(adv_data));
  if (rc != 0) {
    ESP_LOGE(LOG_APP, "Error setting manufacturer data for advertising; rc=%d", rc);
    return;
  }

  // start advertising
  rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, handle_gap_event, NULL);
  if (rc != 0) {
    ESP_LOGE(LOG_APP, "Error enabling extended advertising; rc=%d", rc);
    return;
  }

}
#endif

void ble_advertise() {
  // ble_advertise_normal();
  int rc;
  // reset device status
  if (g_dev_controller.type == DEVICE_TYPE_JOYCON) {
    // TODO Joycon
    ESP_LOGE(LOG_APP, "Joycon not implemented");
    return;
  }
  g_status = DEV_ADV_IND;

  // only one instance advertising
  if (ble_gap_ext_adv_active(instance)) {
    ESP_LOGI(LOG_APP, "Advertising instance %d already active", instance);
    return;
  }

  // reset adv params
  struct ble_gap_ext_adv_params ext_adv_params;
  memset(&ext_adv_params, 0, sizeof(ext_adv_params));
  ext_adv_params.legacy_pdu = 1;
  ext_adv_params.connectable = 1;
  ext_adv_params.scannable = 1;
  ext_adv_params.directed = 0;

  ext_adv_params.own_addr_type = BLE_OWN_ADDR_PUBLIC;
  ext_adv_params.primary_phy = BLE_HCI_LE_PHY_1M;
  ext_adv_params.secondary_phy = BLE_HCI_LE_PHY_1M;
  ext_adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN; // 30ms
  ext_adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MIN; // 30ms
  ext_adv_params.channel_map = BLE_GAP_ADV_DFLT_CHANNEL_MAP;
  ext_adv_params.sid = 0;
  // ext_adv_params.tx_power = 127;
  ext_adv_params.scan_req_notif = false;
  ext_adv_params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;

  rc = ble_gap_ext_adv_configure(instance, 
    &ext_adv_params, NULL, handle_gap_event, NULL);
  if (rc != 0) {
    ESP_LOGE(LOG_APP, "Error configuring extended advertising instance %d; rc=%d", instance, rc);
    return;
  }

  // set manufacturer data
  ESP_LOGI(LOG_APP, "Setting manufacturer data for advertising");
  struct os_mbuf* adv_data;
  uint8_t m_len = sizeof(g_dev_controller.manufacturer_data) + 5;
  uint8_t m_data[m_len];
  // Flags 0x01 LE General Discoverable + BR/EDR Not Supported
  uint8_t m_head[3] = { 0x02, 0x01, 0x06 };
  // Manufacturer Specific Data, len + 0xFF + manufacturer_data
  uint8_t m_size = sizeof(g_dev_controller.manufacturer_data) + 1;
  uint8_t m_spec[2] = { m_size, 0xFF };
  memcpy(m_data, m_head, sizeof(m_head));
  memcpy(m_data + sizeof(m_head), m_spec, sizeof(m_spec));
  // TODO test wakeup flag
  if (g_adv_opcode != 0x00) {
    g_dev_controller.manufacturer_data[11] = g_adv_opcode;
  }
  memcpy(m_data + sizeof(m_head) + sizeof(m_spec), g_dev_controller.manufacturer_data, sizeof(g_dev_controller.manufacturer_data));

  adv_data = os_msys_get_pkthdr(sizeof(m_data), 0);
  rc = os_mbuf_append(adv_data, m_data, sizeof(m_data));
  if (rc != 0) {
    ESP_LOGE(LOG_APP, "Error appending manufacturer data to mbuf; rc=%d", rc);
    os_mbuf_free_chain(adv_data);
    return;
  }
  rc = ble_gap_ext_adv_set_data(instance, adv_data);
  if (rc != 0) {
    ESP_LOGE(LOG_APP, "Error setting manufacturer data for advertising; rc=%d", rc);
    return;
  }

  // start advertising
  rc = ble_gap_ext_adv_start(instance, 0, 0);
  if (rc != 0) {
    ESP_LOGE(LOG_APP, "Error enabling extended advertising; rc=%d", rc);
    return;
  }
}

// **************** BLE Subscription ****************

g_subscribe_entry_t *g_subscribe_map = NULL;
void subscribe_entry_set(uint16_t handle, uint16_t conn_handle, 
  bool notify_enabled, bool indicate_enabled) {
  g_subscribe_entry_t *entry;
  HASH_FIND(hh, g_subscribe_map, &handle, sizeof(uint16_t), entry);
  if (entry == NULL) {
    entry = (g_subscribe_entry_t*)malloc(sizeof(g_subscribe_entry_t));
    if (entry == NULL) {
      ESP_LOGE(LOG_APP, "Failed to allocate memory for subscribe entry");
      return;
    }
    entry->handle = handle;
    HASH_ADD(hh, g_subscribe_map, handle, sizeof(uint16_t), entry);
  }
  entry->state.conn_handle = conn_handle;
  entry->state.notify_enabled = notify_enabled;
  entry->state.indicate_enabled = indicate_enabled;
}
g_subscribe_state_t* subscribe_entry_get(uint16_t handle) {
  g_subscribe_entry_t *entry = NULL;
  HASH_FIND(hh, g_subscribe_map, &handle, sizeof(uint16_t), entry);
  return (entry == NULL) ? NULL : &entry->state;
}
void subscribe_entry_del(uint16_t handle) {
  g_subscribe_entry_t *entry = NULL;
  HASH_FIND(hh, g_subscribe_map, &handle, sizeof(uint16_t), entry);
  if (entry != NULL) {
    HASH_DEL(g_subscribe_map, entry);
    free(entry);
  }
}
void subscribe_map_destroy() {
  g_subscribe_entry_t *entry = NULL;
  g_subscribe_entry_t *tmp = NULL;
  HASH_ITER(hh, g_subscribe_map, entry, tmp) {
    HASH_DEL(g_subscribe_map, entry);
    free(entry);
  }
}

// **************** BLE Store ****************

int custom_store_config_read(int obj_type, const union ble_store_key *key, 
    union ble_store_value *value) {
  int ret =  ble_store_config_read(obj_type, key, value);
  ESP_LOGD(LOG_APP, "custom_store_config_read, obj_type=%d", obj_type);
  if (ret != 0) {
    ESP_LOGE(LOG_APP, "sec read failed, reason=%02x", ret);
    log_print_addr(&key->sec.peer_addr.val);
  }
  switch(obj_type) {
    case BLE_STORE_OBJ_TYPE_OUR_SEC:
      ESP_LOGD(LOG_APP, "custom_store_config_read, BLE_STORE_OBJ_TYPE_OUR_SEC");
      log_print_addr(&key->sec.peer_addr.val);
      break;
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
      ESP_LOGD(LOG_APP, "custom_store_config_read, BLE_STORE_OBJ_TYPE_PEER_SEC");
      log_print_addr(&key->sec.peer_addr.val);
      break;
    case BLE_STORE_OBJ_TYPE_CCCD:
      ESP_LOGD(LOG_APP, "custom_store_config_read, BLE_STORE_OBJ_TYPE_CCCD");
      ESP_LOGD(LOG_APP, 
        "custom_store_config_read, val_handle=0x%04x, idx=%d", 
        &key->cccd.chr_val_handle, &key->cccd.idx);
      break;
    case BLE_STORE_OBJ_TYPE_CSFC:
      ESP_LOGD(LOG_APP, "custom_store_config_read, BLE_STORE_OBJ_TYPE_CSFC");
      break;
    default:
      break;
  }
  return ret;
}

int custom_store_config_write(int obj_type, const union ble_store_value *val) {
  ESP_LOGD(LOG_APP, "custom_store_config_write, obj_type=%d", obj_type);
  switch(obj_type) {
    case BLE_STORE_OBJ_TYPE_OUR_SEC:
      ESP_LOGD(LOG_APP, "custom_store_config_write, BLE_STORE_OBJ_TYPE_OUR_SEC");
      log_print_addr(&val->sec.peer_addr.val);
      break;
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
      ESP_LOGD(LOG_APP, "custom_store_config_write, BLE_STORE_OBJ_TYPE_PEER_SEC");
      log_print_addr(&val->sec.peer_addr.val);
      break;
    case BLE_STORE_OBJ_TYPE_CCCD:
      ESP_LOGD(LOG_APP, "custom_store_config_write, BLE_STORE_OBJ_TYPE_CCCD");
      ESP_LOGD(LOG_APP, 
        "custom_store_config_write, val_handle=0x%04x", 
        &val->cccd.chr_val_handle);
      break;
    default:
      break;
  }
  return ble_store_config_write(obj_type, val);
}

int custom_store_gen_key_cb(uint8_t key,struct ble_store_gen_key *gen_key, uint16_t conn_handle) {
  if (key == BLE_STORE_GEN_KEY_LTK && conn_handle == g_dev_ns2.conn_handle) {
        ESP_LOGD(LOG_APP, "call custom_store_gen_key_cb LTK");
        // Only intercept LTK generation and verify conn_handle ,wait testing
        // copy ltk to KEY generate callback function
        memcpy(g_dev_controller.ltk, gen_key->ltk_periph, LTK_KEY_SIZE);
    }
    return -1;
}