#ifndef _POR2_H
#define _POR2_H

#include <stdint.h>

#include "nvs_flash.h"

// Pro2 Device Init
int pro2_device_init(nvs_handle_t nvs_handle);

// Pro2 Pairing Info save
int pro2_pairing_info_save();

int pro2_inject_pairing_info_to_ble_context();

#endif // _POR2_H