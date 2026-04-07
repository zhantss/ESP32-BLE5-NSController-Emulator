#include "device.h"
#include "hid.h"
#include "utils.h"

static void print_conn_desc(struct ble_gap_conn_desc* desc) {
  ESP_LOGI(LOG_BLE_GAP, "handle=%d our_ota_addr_type=%d our_ota_addr=", 
    desc->conn_handle, desc->our_ota_addr.type);
  log_print_addr(desc->our_ota_addr.val);
  ESP_LOGI(LOG_BLE_GAP, " our_id_addr_type=%d our_id_addr=",
    desc->our_id_addr.type);
  log_print_addr(desc->our_id_addr.val);
  ESP_LOGI(LOG_BLE_GAP, " peer_ota_addr_type=%d peer_ota_addr=",
    desc->peer_ota_addr.type);
  log_print_addr(desc->peer_ota_addr.val);
  ESP_LOGI(LOG_BLE_GAP, " peer_id_addr_type=%d peer_id_addr=",
    desc->peer_id_addr.type);
  log_print_addr(desc->peer_id_addr.val);
  ESP_LOGI(LOG_BLE_GAP, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
    "encrypted=%d authenticated=%d bonded=%d\n",
    desc->conn_itvl, desc->conn_latency,
    desc->supervision_timeout,
    desc->sec_state.encrypted,
    desc->sec_state.authenticated,
    desc->sec_state.bonded);
}

int handle_gap_event(struct ble_gap_event* event, void* arg) {
  struct ble_gap_conn_desc desc;
  int rc;

  switch(event->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status == 0) {
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        print_conn_desc(&desc);
        if (g_status == DEV_ADV_IND) {
          g_dev_ns2.ble_addr.type = desc.peer_ota_addr.type;
          memcpy(g_dev_ns2.ble_addr.val, desc.peer_ota_addr.val, 6);
          ESP_LOGI(LOG_BLE_GAP, "connected, set nintendo switch addr, addr=");
          log_print_addr(g_dev_ns2.ble_addr.val);
        } else {
          ESP_LOGE(LOG_BLE_GAP, "device not ready, reset device");
          g_status = DEV_BOOT;
        }
      } else {
        // failed, restart advertising
        ESP_LOGE(LOG_BLE_GAP, "connection failed, status=%d, restart advertising",
          event->connect.status);
        ble_advertise();
      }
      return 0;
    case BLE_GAP_EVENT_DISCONNECT:
      ESP_LOGI(LOG_BLE_GAP, "disconnected, reason=%d, restart advertising", event->disconnect.reason);
      // TODO dev env not restart adv
      // ble_advertise();
      // stop hid task
      hid_stop_task();
      return 0;
    case BLE_GAP_EVENT_CONN_UPDATE:
      ESP_LOGI(LOG_BLE_GAP, "connection updated, conn_handle=%d, status=%d", 
        event->conn_update.conn_handle, event->conn_update.status);
      return 0;
    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
      ESP_LOGI(LOG_BLE_GAP, "connection update request, conn_handle=%d", event->conn_update_req.conn_handle);
      // set conn params
      *event->conn_update_req.self_params = *event->conn_update_req.peer_params;
      return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
      ESP_LOGI(LOG_BLE_GAP, "adv complete");
      return 0;
    case BLE_GAP_EVENT_ENC_CHANGE:
      ESP_LOGI(LOG_BLE_GAP, "encryption change event; status=%d ", event->enc_change.status);
      rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
      assert(rc == 0);
      print_conn_desc(&desc);
      return 0;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
      ESP_LOGI(LOG_BLE_GAP, "passkey action event; action=%d", event->passkey.params.action);
      return 0;
    case BLE_GAP_EVENT_NOTIFY_TX:
      ESP_LOGD(LOG_BLE_GAP, "notify_tx event; conn_handle=%d attr_handle=%d "
        "status=%d is_indication=%d",
        event->notify_tx.conn_handle,
        event->notify_tx.attr_handle,
        event->notify_tx.status,
        event->notify_tx.indication);
      return 0;
    case BLE_GAP_EVENT_SUBSCRIBE:
      ESP_LOGI(LOG_BLE_GAP, "subscribe event; conn_handle=0x00%02x attr_handle=0x00%02x "
        "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
        event->subscribe.conn_handle,
        event->subscribe.attr_handle,
        event->subscribe.reason,
        event->subscribe.prev_notify,
        event->subscribe.cur_notify,
        event->subscribe.prev_indicate,
        event->subscribe.cur_indicate);
      // cccd subscribe
      subscribe_entry_set(event->subscribe.attr_handle, 
        event->subscribe.conn_handle,
        event->subscribe.cur_notify == 1,
        event->subscribe.cur_indicate == 1);
      
      // 0x000e init hid report
      if (event->subscribe.attr_handle == 0x000e && event->subscribe.cur_notify == 1) {
        g_status = DEV_READY;
        hid_start_task();
      }
      break;
    case BLE_GAP_EVENT_MTU:
      ESP_LOGI(LOG_BLE_GAP, "mtu changed, conn_handle=%d, channel_id=%d, mtu=%d", 
        event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
      return 0;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
      rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
      assert(rc == 0);
      ble_store_util_delete_peer(&desc.peer_id_addr);
      return BLE_GAP_REPEAT_PAIRING_RETRY;
    case BLE_GAP_EVENT_PARING_COMPLETE:
      ESP_LOGI(LOG_BLE_GAP, "paring complete event; status=%d",
        event->pairing_complete.status);
      return 0;
    case BLE_GAP_EVENT_AUTHORIZE:
      ESP_LOGI(LOG_BLE_GAP, "authorize event; conn_handle=%d", event->authorize.conn_handle);
      return 0;
    default:
      break;
  }
  return 0;
}