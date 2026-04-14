#ifndef _CONTROLLER_CONTROLLER_H_
#define _CONTROLLER_CONTROLLER_H_

#include <stdint.h>

#define ESP_BD_ADDR_LEN          6

// LTK Length
#define LTK_LEN                 16

// LTK Key Size
#define LTK_KEY_SIZE            16

// Manufacturer Data Length
#define MANUFACTURER_DATA_LEN   26

typedef enum {
    CONTROLLER_TYPE_PRO2,
    CONTROLLER_TYPE_JOYCON,
} controller_type_t;

typedef struct {
    uint8_t addr[ESP_BD_ADDR_LEN];
    uint8_t addr_re[ESP_BD_ADDR_LEN];
    uint8_t ltk[LTK_LEN];
    uint8_t ltk_re[LTK_LEN];
    uint8_t ltk_key_b1[LTK_KEY_SIZE];
    uint8_t manufacturer_data[MANUFACTURER_DATA_LEN];
    controller_type_t type;
} controller_firmware_t;

extern controller_firmware_t g_controller_firmware;

int inject_pairing_info_to_ble_ctx();

#endif // _CONTROLLER_CONTROLLER_H_
