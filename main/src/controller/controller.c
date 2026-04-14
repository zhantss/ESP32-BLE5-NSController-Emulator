#include "device.h"
#include "controller/controller.h"
#include "utils.h"

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