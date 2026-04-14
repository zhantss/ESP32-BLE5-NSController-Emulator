#include "controller/hid_controller.h"
#include "controller/hid_controller_pro2.h"

#include "esp_log.h"

const uint8_t pro2_firmware_info[12] = {
  // 1.0.14(firmware version) | Pro2 | 12.0.0 (Bluetooth patch version)
  // 0x00 | DSP firmware version, Only present on Pro Controller with updated firmware | 0xff
  0x01, 0x00, 0x0e, 0x02, 0x0c, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff
};

static void pro2_set_button(controller_hid_report_t *report, uint16_t btn_id, bool pressed) {
  if (report == NULL) return;
  hid_report_pro2_t *hid_report = (hid_report_pro2_t *)report->report;
  btns_pro2 btn = LClick;   // default left stick click
  // TODO handle other buttons
  if (btn_id <= C) {
    btn = (btns_pro2)btn_id;
  }
  uint8_t *btn_bytes = (uint8_t*)&hid_report->buttons;
  switch (btn) {
    case A:      pressed ? (btn_bytes[0] |= 0x02) : (btn_bytes[0] &= ~0x02); break;
    case B:      pressed ? (btn_bytes[0] |= 0x01) : (btn_bytes[0] &= ~0x01); break;
    case X:      pressed ? (btn_bytes[0] |= 0x08) : (btn_bytes[0] &= ~0x08); break;
    case Y:      pressed ? (btn_bytes[0] |= 0x04) : (btn_bytes[0] &= ~0x04); break;
    case R:      pressed ? (btn_bytes[0] |= 0x10) : (btn_bytes[0] &= ~0x10); break;
    case ZR:     pressed ? (btn_bytes[0] |= 0x20) : (btn_bytes[0] &= ~0x20); break;
    case Plus:   pressed ? (btn_bytes[0] |= 0x40) : (btn_bytes[0] &= ~0x40); break;
    case RClick: pressed ? (btn_bytes[0] |= 0x80) : (btn_bytes[0] &= ~0x80); break;
    case Down:   pressed ? (btn_bytes[1] |= 0x01) : (btn_bytes[1] &= ~0x01); break;
    case Right:  pressed ? (btn_bytes[1] |= 0x02) : (btn_bytes[1] &= ~0x02); break;
    case Left:   pressed ? (btn_bytes[1] |= 0x04) : (btn_bytes[1] &= ~0x04); break;
    case Up:     pressed ? (btn_bytes[1] |= 0x08) : (btn_bytes[1] &= ~0x08); break;
    case L:      pressed ? (btn_bytes[1] |= 0x10) : (btn_bytes[1] &= ~0x10); break;
    case ZL:     pressed ? (btn_bytes[1] |= 0x20) : (btn_bytes[1] &= ~0x20); break;
    case Minus:  pressed ? (btn_bytes[1] |= 0x40) : (btn_bytes[1] &= ~0x40); break;
    case LClick: pressed ? (btn_bytes[1] |= 0x80) : (btn_bytes[1] &= ~0x80); break;
    case Home:   pressed ? (btn_bytes[2] |= 0x01) : (btn_bytes[2] &= ~0x01); break;
    case Capture:pressed ? (btn_bytes[2] |= 0x02) : (btn_bytes[2] &= ~0x02); break;
    case GR:     pressed ? (btn_bytes[2] |= 0x04) : (btn_bytes[2] &= ~0x04); break;
    case GL:     pressed ? (btn_bytes[2] |= 0x08) : (btn_bytes[2] &= ~0x08); break;
    case C:      pressed ? (btn_bytes[2] |= 0x10) : (btn_bytes[2] &= ~0x10); break;
    default: break;
  }
}

static void pro2_set_left_stick(controller_hid_report_t *report, uint16_t x, uint16_t y) {
  if (report == NULL) return;
  hid_report_pro2_t *hid_report = (hid_report_pro2_t *)report->report;
  pack_stick_data(hid_report->left_stick, x, y);
}

static void pro2_set_right_stick(controller_hid_report_t *report, uint16_t x, uint16_t y) {
  if (report == NULL) return;
  hid_report_pro2_t *hid_report = (hid_report_pro2_t *)report->report;
  pack_stick_data(hid_report->right_stick, x, y);
}

static void pro2_report_init(controller_hid_report_t *report) {
  if (report == NULL) return;

  report->type = DEVICE_TYPE_PRO2;
  report->report = (hid_report_pro2_t *)malloc(sizeof(hid_report_pro2_t));
  if (report->report == NULL) {
    ESP_LOGE(LOG_HID, "Failed to allocate memory for pro2 controller report");
    return;
  }
  memset(report->report, 0, sizeof(hid_report_pro2_t));

  hid_report_pro2_t *hid_report = (hid_report_pro2_t *)report->report;
  hid_report->counter = 0;
  hid_report->power_info = 0x20;
  hid_report->unknown_0x0b = 0x38;
  hid_report->unknown_0x0c = 0x00;
  hid_report->headset_flag = 0x00;
  hid_report->motion_data_len = 0x28;
  pro2_set_left_stick(report, PRO2_STICK_CENTER, PRO2_STICK_CENTER);
  pro2_set_right_stick(report, PRO2_STICK_CENTER, PRO2_STICK_CENTER);
}

static void pro2_set_button_custom(controller_hid_report_t *report, uint8_t *data, size_t len) {
  if (report == NULL) return;
  if (len != 3) return;
  hid_report_pro2_t *hid_report = (hid_report_pro2_t *)report->report;
  memcpy(&hid_report->buttons, data, len);
}

static uint8_t* pro2_next_report(controller_hid_report_t *report) {
  if (report == NULL) return NULL;
  hid_report_pro2_t *hid_report = (hid_report_pro2_t *)report->report;
  hid_report->counter++;
  return (uint8_t*)report->report;
}

static size_t pro2_report_size() {
  return sizeof(hid_report_pro2_t);
}

controller_hid_ops_t controller_pro2_ops = {
  .report_init = pro2_report_init,
  .set_button = pro2_set_button,
  .set_left_stick = pro2_set_left_stick,
  .set_right_stick = pro2_set_right_stick,
  .set_button_custom = pro2_set_button_custom,
  .next_report = pro2_next_report,
  .report_size = pro2_report_size,
};
