#include "device.h"
#include "hid_pro2.h"

#include "esp_log.h"

static void pro2_set_button(hid_device_report_t *report, uint16_t btn_id, bool pressed) {
  if (report == NULL) return;
  pro2_hid_report_t *pro2_report = (pro2_hid_report_t *)report->report;
  pro2_btns btn = LClick;   // default left stick click
  // TODO handle other buttons
  if (btn_id <= C) {
    btn = (pro2_btns)btn_id;
  }
  uint8_t *btn_bytes = (uint8_t*)&pro2_report->buttons;
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

static void pro2_set_left_stick(hid_device_report_t *report, uint16_t x, uint16_t y) {
  if (report == NULL) return;
  pro2_hid_report_t *pro2_report = (pro2_hid_report_t *)report->report;
  pack_stick_data(pro2_report->left_stick, x, y);
}

static void pro2_set_right_stick(hid_device_report_t *report, uint16_t x, uint16_t y) {
  if (report == NULL) return;
  pro2_hid_report_t *pro2_report = (pro2_hid_report_t *)report->report;
  pack_stick_data(pro2_report->right_stick, x, y);
}

static void pro2_report_init(hid_device_report_t *report) {
  if (report == NULL) return;

  report->type = DEVICE_TYPE_PRO2;
  report->report = (pro2_hid_report_t *)malloc(sizeof(pro2_hid_report_t));
  if (report->report == NULL) {
    ESP_LOGE(LOG_HID, "Failed to allocate memory for pro2 hid report");
    return;
  }
  memset(report->report, 0, sizeof(pro2_hid_report_t));

  pro2_hid_report_t *pro2_report = (pro2_hid_report_t *)report->report;
  pro2_report->counter = 0;
  pro2_report->power_info = 0x42;
  pro2_report->unknown_0x0b = 0x38;
  pro2_report->unknown_0x0c = 0x00;
  pro2_report->headset_flag = 0x00;
  pro2_report->motion_data_len = 0x28;
  pro2_set_left_stick(report, PRO2_STICK_CENTER, PRO2_STICK_CENTER);
  pro2_set_right_stick(report, PRO2_STICK_CENTER, PRO2_STICK_CENTER);
}

static void set_button_custom(hid_device_report_t *report, uint8_t *data, size_t len) {
  if (report == NULL) return;
  if (len != 3) return;
  pro2_hid_report_t *pro2_report = (pro2_hid_report_t *)report->report;
  memcpy(&pro2_report->buttons, data, len);
}

static uint8_t* pro2_next_report(hid_device_report_t *report) {
  if (report == NULL) return NULL;
  pro2_hid_report_t *pro2_report = (pro2_hid_report_t *)report->report;
  pro2_report->counter++;
  return (uint8_t*)report->report;
}

static size_t pro2_report_size() {
  return sizeof(pro2_hid_report_t);
}

hid_device_ops_t pro2_hid_ops = {
  .report_init = pro2_report_init,
  .set_button = pro2_set_button,
  .set_left_stick = pro2_set_left_stick,
  .set_right_stick = pro2_set_right_stick,
  .set_button_custom = set_button_custom,
  .next_report = pro2_next_report,
  .report_size = pro2_report_size,
};