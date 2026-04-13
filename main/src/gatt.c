#include "device.h"
#include "pro2.h"
#include "ns2_ble_gatt.h"
#include "utils.h"

// #region GATT UUID

// https://github.com/ndeadly/switch2_controller_research/blob/master/bluetooth_interface.md#gatt-attributes

// Primary Service 0x0001-0x0007 UUID 00c5af5d-1964-4e30-8f51-1956f96bd280
static const ble_uuid128_t GATT_ALL_PRIMARY_SERVICE_0x0001_0x0007 = BLE_UUID128_INIT(
  0x80, 0xd2, 0x6b, 0xf9, 0x56, 0x19, 0x51, 0x8f,
  0x30, 0x4e, 0x64, 0x19, 0x5d, 0xaf, 0xc5, 0x00
);

// Characteristic 0x0003 UUID 00c5af5d-1964-4e30-8f51-1956f96bd281 | R
static const ble_uuid128_t GATT_ALL_CHARACTERISTIC_0x0003 = BLE_UUID128_INIT(
  0x81, 0xd2, 0x6b, 0xf9, 0x56, 0x19, 0x51, 0x8f,
  0x30, 0x4e, 0x64, 0x19, 0x5d, 0xaf, 0xc5, 0x00
);

// Read 00c5af5d-1964-4e30-8f51-1956f96bd281
static uint16_t gatt_svr_chr_0003_val_handle;
static uint8_t gatt_svr_chr_0003_val[] = { 0x04, 0x00, 0x05, 0x00, 0x01, 0x01, 0x00 };
  
// Characteristic 0x0005 UUID 00c5af5d-1964-4e30-8f51-1956f96bd282 | W
static const ble_uuid128_t GATT_ALL_CHARACTERISTIC_0x0005 = BLE_UUID128_INIT(
  0x82, 0xd2, 0x6b, 0xf9, 0x56, 0x19, 0x51, 0x8f,
  0x30, 0x4e, 0x64, 0x19, 0x5d, 0xaf, 0xc5, 0x00
);

// Write 0x0005 00c5af5d-1964-4e30-8f51-1956f96bd282
static uint16_t gatt_svr_chr_0005_val_handle;

// Characteristic 0x0007 UUID 00c5af5d-1964-4e30-8f51-1956f96bd283 | R
static const ble_uuid128_t GATT_ALL_CHARACTERISTIC_0x0007 = BLE_UUID128_INIT(
  0x83, 0xd2, 0x6b, 0xf9, 0x56, 0x19, 0x51, 0x8f,
  0x30, 0x4e, 0x64, 0x19, 0x5d, 0xaf, 0xc5, 0x00
);

// Read 00c5af5d-1964-4e30-8f51-1956f96bd283
static uint16_t gatt_svr_chr_0007_val_handle;
// TODO unknown read value may need to generate(random) and store
static uint8_t gatt_svr_chr_0007_val[] = { 0x36, 0x80, 0x74, 0xee, 0xbb, 0x3d, 0x8e, 0x13 };

// Primary Service 0x0008-0x0032 UUID ab7de9be-89fe-49ad-828f-118f09df7fd0
static const ble_uuid128_t GATT_ALL_PRIMARY_SERVICE_0x0008_0x0032 = BLE_UUID128_INIT(
    0xd0, 0x7f, 0xdf, 0x09, 0x8f, 0x11, 0x8f, 0x82,
    0xad, 0x49, 0xfe, 0x89, 0xbe, 0xe9, 0x7d, 0xab
);

// Characteristic 0x000a UUID ab7de9be-89fe-49ad-828f-118f09df7fd2 | R/N | Descriptor CCC 0x000b
static const ble_uuid128_t GATT_ALL_CHARACTERISTIC_0x000a = BLE_UUID128_INIT(
    0xd2, 0x7f, 0xdf, 0x09, 0x8f, 0x11, 0x8f, 0x82,
    0xad, 0x49, 0xfe, 0x89, 0xbe, 0xe9, 0x7d, 0xab
);

// Input Report 所有类型控制器的 0x05 HID报告
// https://github.com/ndeadly/switch2_controller_research/blob/master/hid_reports.md#input-report-0x05
// Read/Notify 0x000a ab7de9be-89fe-49ad-828f-118f09df7fd2
static uint16_t gatt_svr_chr_000a_val_handle;

// Descriptor 0x000c UUID 679d5510-5a24-4dee-9557-95df80486ecb
static const ble_uuid128_t GATT_ALL_DESCRIPTOR_0x000c = BLE_UUID128_INIT(
    0xcb, 0x6e, 0x48, 0x80, 0xdf, 0x95, 0x57, 0x95,
    0xee, 0x4d, 0x24, 0x5a, 0x10, 0x55, 0x9d, 0x67
);

// Characteristic 0x000e UUID 7492866c-ec3e-4619-8258-32755ffcc0f8 | R/N | Descriptor CCC 0x000f
static const ble_uuid128_t GATT_PRO2_CHARACTERISTIC_0x000e = BLE_UUID128_INIT(
    0xf8, 0xc0, 0xfc, 0x5f, 0x75, 0x32, 0x58, 0x82,
    0x19, 0x46, 0x3e, 0xec, 0x6c, 0x86, 0x92, 0x74
);

// Info Report Pro2手柄 0x09 HID报告
// https://github.com/ndeadly/switch2_controller_research/blob/master/hid_reports.md#input-report-0x09
// Read/Notify 0x000e 7492866c-ec3e-4619-8258-32755ffcc0f8
static uint16_t gatt_svr_chr_000e_val_handle;

// Descriptor 0x0010 UUID 679d5510-5a24-4dee-9557-95df80486ecb
static const ble_uuid128_t GATT_PRO2_DESCRIPTOR_0x0010 = BLE_UUID128_INIT(
    0xcb, 0x6e, 0x48, 0x80, 0xdf, 0x95, 0x57, 0x95,
    0xee, 0x4d, 0x24, 0x5a, 0x10, 0x55, 0x9d, 0x67
);

// Characteristic 0x0012 UUID cc483f51-9258-427d-a939-630c31f72b05 | WNoRSP Vibration
static const ble_uuid128_t GATT_PRO2_CHARACTERISTIC_0x0012 = BLE_UUID128_INIT(
    0x05, 0x2b, 0xf7, 0x31, 0x0c, 0x63, 0x39, 0xa9,
    0x7d, 0x42, 0x58, 0x92, 0x51, 0x3f, 0x48, 0xcc
);

static uint16_t gatt_svr_chr_0012_val_handle;

// Characteristic 0x0014 UUID 649d4ac9-8eb7-4e6c-af44-1ea54fe5f005 | WNoRSP Command
static const ble_uuid128_t GATT_PRO2_CHARACTERISTIC_0x0014 = BLE_UUID128_INIT(
    0x05, 0xf0, 0xe5, 0x4f, 0xa5, 0x1e, 0x44, 0xaf,
    0x6c, 0x4e, 0xb7, 0x8e, 0xc9, 0x4a, 0x9d, 0x64
);

// Characteristic 0x0016 UUID 3dacbc7e-6955-40b5-8eaf-6f9809e8b379 | WNoRSP V+C
static const ble_uuid128_t GATT_PRO2_CHARACTERISTIC_0x0016 = BLE_UUID128_INIT(
    0x79, 0xb3, 0xe8, 0x09, 0x98, 0x6f, 0xaf, 0x8e,
    0xb5, 0x40, 0x55, 0x69, 0x7e, 0xbc, 0xac, 0x3d
);

// 关键设备初始化句柄
static uint16_t gatt_svr_chr_0016_val_handle;

// Characteristic 0x0018 UUID 4147423d-fdae-4df7-a4f7-d23e5df59f8d | WNoRSP FirmwareUpdate 
static const ble_uuid128_t GATT_PRO2_CHARACTERISTIC_0x0018 = BLE_UUID128_INIT(
    0x8d, 0x9f, 0xf5, 0x5d, 0x3e, 0xd2, 0xf7, 0xa4,
    0xf7, 0x4d, 0xae, 0xfd, 0x3d, 0x42, 0x47, 0x41
);

// Characteristic 0x001a UUID c765a961-d9d8-4d36-a20a-5315b111836a | N | Descriptor CCC 0x001b
static const ble_uuid128_t GATT_ALL_CHARACTERISTIC_0x001a = BLE_UUID128_INIT(
    0x6a, 0x83, 0x11, 0xb1, 0x15, 0x53, 0x0a, 0xa2,
    0x36, 0x4d, 0xd8, 0xd9, 0x61, 0xa9, 0x65, 0xc7
);

// Notify c765a961-d9d8-4d36-a20a-5315b111836a
static uint16_t gatt_svr_chr_001a_val_handle;

// Descriptor 0x001c UUID b746df8c-f358-495b-9cd2-e3bbeda4f979
static const ble_uuid128_t GATT_ALL_DESCRIPTOR_0x001c = BLE_UUID128_INIT(
    0x79, 0xf9, 0xa4, 0xed, 0xbb, 0xe3, 0xd2, 0x9c,
    0x5b, 0x49, 0x58, 0xf3, 0x8c, 0xdf, 0x46, 0xb7
);

// Characteristic 0x001e UUID 506d9f7d-4278-4e95-a549-326ba77657e0 | N | Descriptor CCC 0x001f
static const ble_uuid128_t GATT_PRO2_CHARACTERISTIC_0x001e = BLE_UUID128_INIT(
    0xe0, 0x57, 0x76, 0xa7, 0x6b, 0x32, 0x49, 0xa5,
    0x95, 0x4e, 0x78, 0x42, 0x7d, 0x9f, 0x6d, 0x50
);

// Notify 506d9f7d-4278-4e95-a549-326ba77657e0
static uint16_t gatt_svr_chr_001e_val_handle;

// Descriptor 0x0020 UUID b746df8c-f358-495b-9cd2-e3bbeda4f979
static const ble_uuid128_t GATT_ALL_DESCRIPTOR_0x0020 = BLE_UUID128_INIT(
    0x79, 0xf9, 0xa4, 0xed, 0xbb, 0xe3, 0xd2, 0x9c,
    0x5b, 0x49, 0x58, 0xf3, 0x8c, 0xdf, 0x46, 0xb7
);

// Characteristic 0x0022 UUID d3bd69d2-841c-4241-ab15-f86f406d2a80 | N | Descriptor CCC 0x0023
static const ble_uuid128_t GATT_PRO2_CHARACTERISTIC_0x0022 = BLE_UUID128_INIT(
    0x80, 0x2a, 0x6d, 0x40, 0x6f, 0xf8, 0x15, 0xab,
    0x41, 0x42, 0x1c, 0x84, 0xd2, 0x69, 0xbd, 0xd3
);

// Notify d3bd69d2-841c-4241-ab15-f86f406d2a80
static uint16_t gatt_svr_chr_0022_val_handle;

// Descriptor 0x0024 UUID b746df8c-f358-495b-9cd2-e3bbeda4f979
static const ble_uuid128_t GATT_ALL_DESCRIPTOR_0x0024 = BLE_UUID128_INIT(
    0x79, 0xf9, 0xa4, 0xed, 0xbb, 0xe3, 0xd2, 0x9c,
    0x5b, 0x49, 0x58, 0xf3, 0x8c, 0xdf, 0x46, 0xb7
);

// Characteristic 0x0026 UUID ab7de9be-89fe-49ad-828f-118f09df7fde | R/N | Descriptor CCC 0x0027
static const ble_uuid128_t GATT_ALL_CHARACTERISTIC_0x0026 = BLE_UUID128_INIT(
    0xde, 0x7f, 0xdf, 0x09, 0x8f, 0x11, 0x8f, 0x82,
    0xad, 0x49, 0xfe, 0x89, 0xbe, 0xe9, 0x7d, 0xab
);

// Read/Notify ab7de9be-89fe-49ad-828f-118f09df7fde
static uint16_t gatt_svr_chr_0026_val_handle;

// Descriptor 0x0028 UUID 679d5510-5a24-4dee-9557-95df80486ecb
static const ble_uuid128_t GATT_ALL_DESCRIPTOR_0x0028 = BLE_UUID128_INIT(
    0xcb, 0x6e, 0x48, 0x80, 0xdf, 0x95, 0x57, 0x95,
    0xee, 0x4d, 0x24, 0x5a, 0x10, 0x55, 0x9d, 0x67
);

// Characteristic 0x002a UUID ab7de9be-89fe-49ad-828f-118f09df7fdf | WNoRSP Unknown
static const ble_uuid128_t GATT_ALL_CHARACTERISTIC_0x002a = BLE_UUID128_INIT(
    0xdf, 0x7f, 0xdf, 0x09, 0x8f, 0x11, 0x8f, 0x82,
    0xad, 0x49, 0xfe, 0x89, 0xbe, 0xe9, 0x7d, 0xab
);

// Characteristic 0x002c UUID cc483f51-9258-427d-a939-630c31f72b06 | WNoRSP HeadsetAudio
static const ble_uuid128_t GATT_PRO2_CHARACTERISTIC_0x002c = BLE_UUID128_INIT(
    0x06, 0x2b, 0xf7, 0x31, 0x0c, 0x63, 0x39, 0xa9,
    0x7d, 0x42, 0x58, 0x92, 0x51, 0x3f, 0x48, 0xcc
);

// Characteristic 0x002e UUID 7492866c-ec3e-4619-8258-32755ffcc05f | R/N | Descriptor CCC 0x002f
static const ble_uuid128_t GATT_PRO2_CHARACTERISTIC_0x002e = BLE_UUID128_INIT(
    0xf9, 0xc0, 0xfc, 0x5f, 0x75, 0x32, 0x58, 0x82,
    0x19, 0x46, 0x3e, 0xec, 0x6c, 0x86, 0x92, 0x74
);

// Read/Notify 7492866c-ec3e-4619-8258-32755ffcc05f
static uint16_t pro2_0107_gatt_svr_chr_002e_val_handle;

// Descriptor 0x0030 UUID 679d5510-5a24-4dee-9557-95df80486ecb
static const ble_uuid128_t GATT_PRO2_DESCRIPTOR_0x0030 = BLE_UUID128_INIT(
    0xcb, 0x6e, 0x48, 0x80, 0xdf, 0x95, 0x57, 0x95,
    0xee, 0x4d, 0x24, 0x5a, 0x10, 0x55, 0x9d, 0x67
);

// Characteristic 0x0032 UUID 3dacbc7e-6955-40b5-8eaf-6f9809e8b380 | WNoRSP Unknown
static const ble_uuid128_t GATT_PRO2_CHARACTERISTIC_0x0032 = BLE_UUID128_INIT(
    0x80, 0xb3, 0xe8, 0x09, 0x98, 0x6f, 0xaf, 0x8e,
    0xb5, 0x40, 0x55, 0x69, 0x7e, 0xbc, 0xac, 0x3d
);

// #endregion

static int gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
  struct ble_gatt_access_ctxt* ctxt, void* arg){
    ESP_LOGD(LOG_APP, 
      "GATT service access callback, handle=%d, opcode=%d", attr_handle, ctxt->op);
    switch (ctxt->op) {
      case BLE_GATT_ACCESS_OP_READ_CHR:
      case BLE_GATT_ACCESS_OP_WRITE_CHR:
      case BLE_GATT_ACCESS_OP_WRITE_DSC:
      case BLE_GATT_ACCESS_OP_READ_DSC:
      default:
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_dsc_access(uint16_t conn_handle, uint16_t attr_handle,
  struct ble_gatt_access_ctxt* ctxt, void* arg) {
  uint8_t opcode = ctxt->op;
    ESP_LOGD(LOG_APP, 
      "GATT dsc access callback, handle=%d, opcode=%d", attr_handle, ctxt->op);
  // TODO handle descriptor read/write
  if (opcode == BLE_GATT_ACCESS_OP_WRITE_DSC) {
  } else if (opcode == BLE_GATT_ACCESS_OP_READ_DSC) {
  }
  // always success
  return 0;
}

static int gatt_svc_0x01_07_access(uint16_t conn_handle, uint16_t attr_handle,
  struct ble_gatt_access_ctxt* ctxt, void* arg) {
    int rc;
    ESP_LOGI(LOG_APP, 
      "GATT 0x01_0x07 service access callback, handle=%d, opcode=%d", attr_handle, ctxt->op);
    switch(ctxt->op) {
      case BLE_GATT_ACCESS_OP_READ_CHR:
        if (attr_handle == gatt_svr_chr_0003_val_handle) {
          // handle def_handle 0x0002 val_handle 0x0003
          // rsp 04000500010100 Unknown
          rc = os_mbuf_append(ctxt->om, 
            &gatt_svr_chr_0003_val, sizeof(gatt_svr_chr_0003_val));
          return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (attr_handle == gatt_svr_chr_0007_val_handle) {
          // handle def_handle 0x0006 val_handle 0x0007
          // TODO test rsp 368074eebb3d8e13 Unknown
          rc = os_mbuf_append(ctxt->om, 
            &gatt_svr_chr_0007_val, sizeof(gatt_svr_chr_0007_val));
          return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        break;
      case BLE_GATT_ACCESS_OP_WRITE_CHR:
        // cmd write 0100
        // 0x0005
        uint8_t write_data[2];
        rc = os_mbuf_copydata(ctxt->om, 0, sizeof(write_data), write_data);
        if (rc != 0) {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        // always success
        return 0;
      default:
        break;
    }
    assert(0);
    // TODO LOG not handled
    return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svc_write_no_rsp_access(uint16_t conn_handle, uint16_t attr_handle,
  struct ble_gatt_access_ctxt* ctxt, void* arg) {
  uint8_t opcode = ctxt->op;
  int rc;
  ESP_LOGD(LOG_APP, "Write no rsp access callback, handle=0x00%02x, opcode=0x00%02x, mtu=%d", 
    attr_handle, opcode, ble_att_mtu(conn_handle));
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    if (attr_handle == gatt_svr_chr_0016_val_handle) {
      // 0x0016 handle no rsp
      uint16_t data_len = os_mbuf_len(ctxt->om);
      ESP_LOGD(LOG_APP, "Received data length: %d", data_len);
      if (data_len < 8 + NS2_DATA_EMPTY_LEN) {
        ESP_LOGE(LOG_APP, "Invalid data length: %d (expected >= 8)", data_len);
        return BLE_ATT_ERR_UNLIKELY;
      }

      uint8_t *data_buf = (uint8_t*) malloc((data_len - NS2_DATA_EMPTY_LEN) * sizeof(uint8_t));
      if (data_buf == NULL) {
        ESP_LOGE(LOG_APP, "Failed to allocate memory for data buffer");
        return BLE_ATT_ERR_INSUFFICIENT_RES;
      }

      rc = os_mbuf_copydata(ctxt->om, NS2_DATA_EMPTY_LEN, data_len - NS2_DATA_EMPTY_LEN, data_buf);
      if (rc != 0) {
        ESP_LOGE(LOG_APP, "Failed to copy data from mbuf");
        free(data_buf);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
      }

      pro2_gatt_rsp_t* rsp = malloc(sizeof(pro2_gatt_rsp_t));
      if (rsp == NULL) {
        ESP_LOGE(LOG_APP, "Failed to allocate memory for response structure");
        free(data_buf);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      // Initialize rsp_data to NULL to prevent free on uninitialized pointer
      rsp->rsp_data = NULL;
      rsp->rsp_len = 0;

      ESP_LOGD(LOG_APP, "Received data head: %02x%02x%02x%02x%02x%02x%02x%02x",
        data_buf[0], data_buf[1], data_buf[2], data_buf[3], data_buf[4], data_buf[5], data_buf[6], data_buf[7]);
      rsp->cmd = data_buf[0];
      rsp->subcmd = data_buf[3];
      rc = cmd_process(rsp, data_buf, data_len);
      if (rc != 0) {
        ESP_LOGE(LOG_APP, "commands process failed: 0x%02x", rc);
      } else {
        // send notify use 0x001e
        rc = gatt_notify(conn_handle, gatt_svr_chr_001e_val_handle, rsp->rsp_data, rsp->rsp_len);
      }

      // Cleanup allocated resources
      if (rsp->rsp_data != NULL) {
        free(rsp->rsp_data);
      }
      free(rsp);
      free(data_buf);
      return rc;
    } else if (attr_handle == gatt_svr_chr_0012_val_handle) {
      // 0x0012 handle no rsp
      // Vibration?
      // TODO send hid report 0x000e

      return 0;
    }
    ESP_LOGE(LOG_APP, "Invalid attribute handle: 0x00%02x", attr_handle);
    return BLE_ATT_ERR_UNLIKELY;
  } else {
    return BLE_ATT_ERR_UNLIKELY;
  }
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
  {
    /** PrimaryService 0x0001-0x0007 */
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &GATT_ALL_PRIMARY_SERVICE_0x0001_0x0007.u,
    .characteristics = (struct ble_gatt_chr_def[]) {
      {
        .uuid = &GATT_ALL_CHARACTERISTIC_0x0003.u,
        .flags = BLE_GATT_CHR_F_READ,
        .val_handle = &gatt_svr_chr_0003_val_handle,
        .access_cb = gatt_svc_0x01_07_access,
      },
      {
        .uuid = &GATT_ALL_CHARACTERISTIC_0x0005.u,
        .flags = BLE_GATT_CHR_F_WRITE,
        .val_handle = &gatt_svr_chr_0005_val_handle,
        .access_cb = gatt_svc_0x01_07_access,
      },
      {
        .uuid = &GATT_ALL_CHARACTERISTIC_0x0007.u,
        .flags = BLE_GATT_CHR_F_READ,
        .val_handle = &gatt_svr_chr_0007_val_handle,
        .access_cb = gatt_svc_0x01_07_access,
      },
      {
        0, // No more characteristics in this service.
      }
    }
  }, 
  {
    /** PrimaryService 0x0008~ */
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &GATT_ALL_PRIMARY_SERVICE_0x0008_0x0032.u,
    .characteristics = (struct ble_gatt_chr_def[]) {
      {
        .uuid = &GATT_ALL_CHARACTERISTIC_0x000a.u,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &gatt_svr_chr_000a_val_handle,
        .access_cb = gatt_svc_access,
        .descriptors = (struct ble_gatt_dsc_def[]) {
          {
            .uuid = &GATT_ALL_DESCRIPTOR_0x000c.u,
            .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
            .access_cb = gatt_dsc_access,
          },
          {
            0, // No more descriptors in this characteristic.
          }
        },
      },
      {
        .uuid = &GATT_PRO2_CHARACTERISTIC_0x000e.u,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &gatt_svr_chr_000e_val_handle,
        .access_cb = gatt_svc_access,
        .descriptors = (struct ble_gatt_dsc_def[]) {
          {
            .uuid = &GATT_PRO2_DESCRIPTOR_0x0010.u,
            .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
            .access_cb = gatt_dsc_access,
          },
          {
            0, // No more descriptors in this characteristic.
          }
        },
      },
      {
        .uuid = &GATT_PRO2_CHARACTERISTIC_0x0012.u,
        .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        .val_handle = &gatt_svr_chr_0012_val_handle,
        .access_cb = gatt_svc_write_no_rsp_access,
      },
      {
        .uuid = &GATT_PRO2_CHARACTERISTIC_0x0014.u,
        .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        .access_cb = gatt_svc_write_no_rsp_access,
      },
      {
        .uuid = &GATT_PRO2_CHARACTERISTIC_0x0016.u,
        .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        .val_handle = &gatt_svr_chr_0016_val_handle,
        .access_cb = gatt_svc_write_no_rsp_access,
      },
      {
        .uuid = &GATT_PRO2_CHARACTERISTIC_0x0018.u,
        .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        .access_cb = gatt_svc_write_no_rsp_access,
      },
      {
        .uuid = &GATT_ALL_CHARACTERISTIC_0x001a.u,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &gatt_svr_chr_001a_val_handle,
        .access_cb = gatt_svc_access,
        .descriptors = (struct ble_gatt_dsc_def[]) {
          {
            .uuid = &GATT_ALL_DESCRIPTOR_0x001c.u,
            .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
            .access_cb = gatt_dsc_access,
          },
          {
            0, // No more descriptors in this characteristic.
          }
        },
      },
      {
        .uuid = &GATT_PRO2_CHARACTERISTIC_0x001e.u,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &gatt_svr_chr_001e_val_handle,
        .access_cb = gatt_svc_access,
        .descriptors = (struct ble_gatt_dsc_def[]) {
          {
            .uuid = &GATT_ALL_DESCRIPTOR_0x0020.u,
            .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
            .access_cb = gatt_dsc_access,
          },
          {
            0, // No more descriptors in this characteristic.
          }
        },
      },
      {
        .uuid = &GATT_PRO2_CHARACTERISTIC_0x0022.u,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &gatt_svr_chr_0022_val_handle,
        .access_cb = gatt_svc_access,
        .descriptors = (struct ble_gatt_dsc_def[]) {
          {
            .uuid = &GATT_ALL_DESCRIPTOR_0x0024.u,
            .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
            .access_cb = gatt_dsc_access,
          },
          {
            0, // No more descriptors in this characteristic.
          }
        },
      },
      {
        .uuid = &GATT_ALL_CHARACTERISTIC_0x0026.u,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &gatt_svr_chr_0026_val_handle,
        .access_cb = gatt_svc_access,
        .descriptors = (struct ble_gatt_dsc_def[]) {
          {
            .uuid = &GATT_ALL_DESCRIPTOR_0x0028.u,
            .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
            .access_cb = gatt_dsc_access,
          },
          {
            0, // No more descriptors in this characteristic.
          }
        },
      },
      {
        .uuid = &GATT_ALL_CHARACTERISTIC_0x002a.u,
        .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        .access_cb = gatt_svc_write_no_rsp_access,
      },
      {
        .uuid = &GATT_PRO2_CHARACTERISTIC_0x002c.u,
        .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        .access_cb = gatt_svc_write_no_rsp_access,
      },
      {
        .uuid = &GATT_PRO2_CHARACTERISTIC_0x002e.u,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &pro2_0107_gatt_svr_chr_002e_val_handle,
        .access_cb = gatt_svc_access,
        .descriptors = (struct ble_gatt_dsc_def[]) {
          {
            .uuid = &GATT_PRO2_DESCRIPTOR_0x0030.u,
            .att_flags = BLE_ATT_F_READ,
            .access_cb = gatt_dsc_access,
          },
          {
            0, // No more descriptors in this characteristic.
          }
        },
      },
      {
        .uuid = &GATT_PRO2_CHARACTERISTIC_0x0032.u,
        .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        .access_cb = gatt_svc_write_no_rsp_access,
      },
      {
        0, // No more characteristics in this service.
      }
    }
  }, 
  // {
  //   .type = BLE_GATT_SVC_TYPE_PRIMARY,
  //   .uuid = &GATT_PRI_SERVICE_UUID_1800,
  //   .characteristics = (struct ble_gatt_chr_def[]) {
  //     {
  //       .uuid = &GATT_CHARACTERISTIC_UUID_2a00.u,
  //       .flags = BLE_GATT_CHR_F_READ,
  //       .access_cb = gatt_other_access,
  //     },
  //     {
  //       .uuid = &GATT_CHARACTERISTIC_UUID_2a01.u,
  //       .flags = BLE_GATT_CHR_F_READ,
  //       .access_cb = gatt_other_access,
  //     },
  //     {
  //       0, // No more characteristics in this service.
  //     }
  //   }
  // }, 
  // {
  //   .type = BLE_GATT_SVC_TYPE_PRIMARY,
  //   .uuid = &GATT_PRI_SERVICE_UUID_1801,
  //   .characteristics = (struct ble_gatt_chr_def[]) {
  //     {
  //       0, // No more characteristics in this service.
  //     }
  //   }
  // },
  {
    0, // No more services.
  }
};

void device_gatt_svr_register_cb(struct ble_gatt_register_ctxt* ctxt, void* arg) {
  char buf[BLE_UUID_STR_LEN];

  switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(LOG_APP, "registered %-16s %s - %-10s = 0x00%02x", "service", 
        ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), 
        "handle", ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(LOG_APP, "registered %-16s %s - %-10s = 0x00%02x %-10s = 0x00%02x", "characteristic",
        ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf), 
        "def_handle", ctxt->chr.def_handle, 
        "val_handle", ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(LOG_APP, "registered %-16s %s - %-10s = 0x00%02x", "descriptor",
        ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
        "handle", ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
  }
}

int gatt_notify(uint16_t conn_handle, uint16_t chr_val_handle,
                const uint8_t* data, const size_t data_len) {
    struct os_mbuf *om;
    int rc;

    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGE(LOG_BLE_GATT, "Invalid connection handle");
        return BLE_HS_ENOTCONN;
    }

    // init mbuf
    om = ble_hs_mbuf_from_flat(data, data_len);
    if (om == NULL) {
        ESP_LOGE(LOG_BLE_GATT, "Failed to allocate mbuf");
        return BLE_HS_ENOMEM;
    }


    // send notification
    rc = ble_gatts_notify_custom(conn_handle, chr_val_handle, om);
    if (rc != 0) {
        ESP_LOGE(LOG_BLE_GATT, "ble_gatts_notify failed: %d", rc);
        os_mbuf_free_chain(om);
        return rc;
    }

    ESP_LOGD(LOG_BLE_GATT, "Notification sent, handle=0x%04x, len=%d",
             chr_val_handle, data_len);
    return 0;
}

int device_gatt_svr_init(void) {
  int rc;
  // !disable default services
  // ble_svc_gap_init();
  // ble_svc_gatt_init();
  // ble_svc_ans_init();
  
  // Initialize command system
  if (cmd_system_init() != 0) {
      ESP_LOGE(LOG_APP, "Failed to initialize command system");
  } else {
      ESP_LOGI(LOG_APP, "Command system initialized");
  }

  rc = ble_gatts_count_cfg(gatt_svr_svcs);
  if (rc != 0) {
    ESP_LOGE(LOG_BLE_GATT, "ble_gatts_count_cfg() failed");
    return rc;
  }

  rc = ble_gatts_add_svcs(gatt_svr_svcs);
  if (rc != 0) {
    ESP_LOGE(LOG_BLE_GATT, "ble_gatts_add_svcs() failed");
    return rc;
  }

  return 0;
}
