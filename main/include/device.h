#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <stdint.h>

#include "nimble/ble.h"
#include "host/ble_gap.h"
#include "host/ble_store.h"

#include "uthash.h"
#include "console.h"
#include "controller/controller.h"

// Log tags
#define LOG_APP                 "app"
#define LOG_HID                 "hid"
#define LOG_BLE_GATT            "ble_gatt"
#define LOG_BLE_GAP             "ble_gap"
#define LOG_BLE_NVS             "ble_nvs"

// NVS Key
#define NVS_NAME_CONFIG         "CONFIG"
#define NVS_KEY_CONTROLLER_TYPE "CTRL_TYPE"

#define NVS_NAME_PAIRING_PRO2   "PAIRING_PRO2"
#define NVS_NAME_PAIRING_JC     "PAIRING_JC"

#define NVS_KEY_CTRL_ADDR       "CTRL_MAC"
#define NVS_KEY_HOST_ADDR       "HOST_MAC"
#define NVS_KEY_LTK             "BLE_LTK"

// BLE support

typedef enum DEVICE_STATUS {
    DEV_BOOT,           // esp device boot
    DEV_ADV_IND,        // ble adv started
    DEV_CONNECT_IND,    // ble connected
    DEV_PAIRING,        // ble pairing
    DEV_READY,          // ble paired, ready for hid
} device_status_t;

extern device_status_t g_device_status;

// **************** BLE Stack ****************

// BLE stack init
void ble_stack_init(void);

// gap handle
int handle_gap_event(struct ble_gap_event* event, void* arg);

// gatt svr init
int device_gatt_svr_init();

// gatt scr register callback
void device_gatt_svr_register_cb(struct ble_gatt_register_ctxt* ctxt, void* arg);

// BLE bonding helper function, !!DO NOT MODIFY!! Use to custom bonding, not Impl
void ble_gatts_bonding_established(uint16_t conn_handle);

// **************** BLE Advertise ****************

// Pro2 ADV opcode
extern uint8_t g_adv_opcode;

// adv
void ble_advertise();

// **************** BLE Subscription ****************

// Subscribe State
typedef struct {
  uint16_t handle;
  uint16_t conn_handle;
  bool notify_enabled;
  bool indicate_enabled;
} g_subscribe_state_t;

// Subscribe Entry
typedef struct {
  uint16_t handle;
  g_subscribe_state_t state;
  UT_hash_handle hh;
} g_subscribe_entry_t;

// Subscribe Table
extern g_subscribe_entry_t *g_subscribe_map;

void subscribe_entry_set(uint16_t handle, uint16_t conn_handle, 
    bool notify_enabled, bool indicate_enabled);
g_subscribe_state_t* subscribe_entry_get(uint16_t handle);
void subscribe_entry_del(uint16_t handle);
void subscribe_map_destroy();

// subscription notify function
int gatt_notify(uint16_t conn_handle, uint16_t chr_val_handle, 
    const uint8_t* data, const size_t data_len);

// **************** BLE Store ****************

// LTK Store Value ESP Impl
extern struct ble_store_value_sec* g_ltk_sec;

// BLE Store Config 

void ble_store_config_init(void);
int ble_store_config_read(int obj_type, const union ble_store_key *key,
                      union ble_store_value *value);
int ble_store_config_write(int obj_type, const union ble_store_value *val);

// BLE Custom Store Config read
int custom_store_config_read(int obj_type, const union ble_store_key *key, 
    union ble_store_value *value);

// BLE Custom Store Config write
int custom_store_config_write(int obj_type, const union ble_store_value *val);

// BLE Store Gen Key Callback, !ONLY BLE_STORE_GEN_KEY_LTK Impl, Other return -1
int custom_store_gen_key_cb(uint8_t key,struct ble_store_gen_key *gen_key, uint16_t conn_handle);

#endif // _DEVICE_H_