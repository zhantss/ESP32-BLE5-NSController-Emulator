#include "device.h"
#include "pro2.h"
#include "ns2_codec.h"
#include "controller/controller.h"
#include "controller/hid_controller_pro2.h"
#include "utils.h"

#include "host/ble_att.h"

/**
 * @brief FLASH MEMORY SIM, Necessary for the BLE stack to work
 * @warning DO NOT MODIFY!!! All Empty values will be filled with 0xff
 */

// Device Info
static uint8_t flash_mem_013000[] = {
    // start flag
    0x01, 0x00,
    // serial number, HEJ71001123456
    0x48, 0x45, 0x4A, 0x37, 0x31, 0x30, 0x30, 0x31, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x00, 0x00,
    // Vendor ID, Nintendo
    0x7E, 0x05, 
    // Product ID, Pro2 Controller
    0x69, 0x20, 
    // Unknow, Version/Edition?
    0x01, 0x06, 0x01,
    // Body Color
    0x23, 0x23, 0x23,
    // Buttons Color
    // 0xA0, 0xA0, 0xA0,    // basic color
    0x63, 0xB9, 0X7A,       // pokemon za green
    // Highlight Color
    0xE6, 0xE6, 0xE6,
    // Grip Color
    0x32, 0x32, 0x32
};

// Unknown
static const uint8_t flash_mem_013040[] = {
    // 3B E0 D3 41 C6 60 6A BC 4D D7 A2 BB 71 1E DD 37
    0x3B, 0xE0, 0xD3, 0x41, 0xC6, 0x60, 0x6A, 0xBC, 
    0x4D, 0xD7, 0xA2, 0xBB, 0x71, 0x1E, 0xDD, 0x37
};

// Unknown
static const uint8_t flash_mem_013060[] = {
    
};

// Unknow, About Paring, DO NOT MODIFY!!!
static const uint8_t flash_mem_013080[] = {
    // 01 AD D9 9A 55 56 65 A0 00 0A 
    // A0 00 0A E2 20 0E E2 20 0E 9A 
    // AD D9 9A AD D9 0A A5 50 0A A5 
    // 50 2F F6 62 2F F6 62 0A FF FF
    0x01, 0xAD, 0xD9, 0x9A, 0x55, 0x56, 0x65, 0xA0, 0x00, 0x0A, 
    0xA0, 0x00, 0x0A, 0xE2, 0x20, 0x0E, 0xE2, 0x20, 0x0E, 0x9A, 
    0xAD, 0xD9, 0x9A, 0xAD, 0xD9, 0x0A, 0xA5, 0x50, 0x0A, 0xA5, 
    0x50, 0x2F, 0xF6, 0x62, 0x2F, 0xF6, 0x62, 0x0A, 0xFF, 0xFF
};

// TODO Temporary value, will be changed to a special value for easier calculation once the algorithm is improved
// Primary analog stick calibration, Used for calculating the Left Stick
static const uint8_t flash_mem_0130A8[] = {
    // B3 67 83 2E 66 5E 3A 06 5F
    0xB3, 0x67, 0x83, 0x2E, 0x66, 0x5E, 0x3A, 0x06, 0x5F
};

// Unknow, Similar to 0x013080, DO NOT MODIFY!!!
static const uint8_t flash_mem_0130C0[] = {
    // 01 AD D9 9A 55 56 65 A0 00 0A 
    // A0 00 0A E2 20 0E E2 20 0E 9A 
    // AD D9 9A AD D9 0A A5 50 0A A5 
    // 50 2F F6 62 2F F6 62 0A FF FF
    0x01, 0xAD, 0xD9, 0x9A, 0x55, 0x56, 0x65, 0xA0, 0x00, 0x0A, 
    0xA0, 0x00, 0x0A, 0xE2, 0x20, 0x0E, 0xE2, 0x20, 0x0E, 0x9A, 
    0xAD, 0xD9, 0x9A, 0xAD, 0xD9, 0x0A, 0xA5, 0x50, 0x0A, 0xA5, 
    0x50, 0x2F, 0xF6, 0x62, 0x2F, 0xF6, 0x62, 0x0A, 0xFF, 0xFF
};

// TODO Temporary value, will be changed to a special value for easier calculation once the algorithm is improved
// Secondary analog stick calibration, Used for calculating the Right Stick
static const uint8_t flash_mem_0130E8[] = {
    // 2C 08 84 D1 65 63 2A 26 62
    0x2C, 0x08, 0x84, 0xD1, 0x65, 0x63, 0x2A, 0x26, 0x62
};

// Unknow
static const uint8_t flash_mem_013100[] = {
    // 00 00 00 00 00 00 00 00 00 00 00 00 A6 F2 62 BD A8 00 08 3D 2F ED 20 41
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xA6, 0xF2, 0x62, 0xBD,
    0xA8, 0x00, 0x08, 0x3D, 0x2F, 0xED, 0x20, 0x41
};

// 0x1FC000 Motion Calibration Data
// 0x1FC040 Primary Analog Stick Calibration Data
// 0x1FC060 Secondary Analog Stick Calibration Data
// !!!Uninitialized unless a user calibration has been performed.
static const uint8_t flash_mem_1FC040[] = {
    
};

static const mem_sim_t flash_mem_list[] = {
    {0x013000, sizeof(flash_mem_013000), flash_mem_013000},
    {0x013040, sizeof(flash_mem_013040), flash_mem_013040},
    {0x013060, sizeof(flash_mem_013060), flash_mem_013060},
    {0x013080, sizeof(flash_mem_013080), flash_mem_013080},
    {0x0130A8, sizeof(flash_mem_0130A8), flash_mem_0130A8},
    {0x0130C0, sizeof(flash_mem_0130C0), flash_mem_0130C0},
    {0x0130E8, sizeof(flash_mem_0130E8), flash_mem_0130E8},
    {0x013100, sizeof(flash_mem_013100), flash_mem_013100},
    {0x001FC040, sizeof(flash_mem_1FC040), flash_mem_1FC040},
};

static const size_t flash_mem_list_len = sizeof(flash_mem_list) / sizeof(mem_sim_t);

void set_controller_specific(uint16_t product_id,
                               const uint8_t *serial, size_t serial_len,
                               const uint8_t version[3],
                               const uint8_t body_color[3],
                               const uint8_t buttons_color[3],
                               const uint8_t highlight_color[3],
                               const uint8_t grip_color[3]) {
    // start flag
    flash_mem_013000[0] = 0x01;
    flash_mem_013000[1] = 0x00;

    // serial number (16 bytes, null-padded)
    memset(&flash_mem_013000[2], 0, 16);
    if (serial != NULL && serial_len > 0) {
        size_t copy_len = serial_len > 16 ? 16 : serial_len;
        memcpy(&flash_mem_013000[2], serial, copy_len);
    }

    // Vendor ID, Nintendo
    flash_mem_013000[18] = 0x7E;
    flash_mem_013000[19] = 0x05;

    // Product ID (little-endian)
    flash_mem_013000[20] = product_id & 0xFF;
    flash_mem_013000[21] = (product_id >> 8) & 0xFF;

    // Version/Edition
    memcpy(&flash_mem_013000[22], version, 3);

    // Colors
    memcpy(&flash_mem_013000[25], body_color, 3);
    memcpy(&flash_mem_013000[28], buttons_color, 3);
    memcpy(&flash_mem_013000[31], highlight_color, 3);
    memcpy(&flash_mem_013000[34], grip_color, 3);
}

int read_memory(uint32_t addr, size_t read_len, uint8_t* out_buffer) {
    if (read_len == 0 || out_buffer == NULL) {
        return -1;
    }
    // set placeholder 0xff
    memset(out_buffer, 0xFF, read_len);

    uint32_t addr_end = addr + read_len;
    int has_block = 0;

    for (size_t i = 0; i < flash_mem_list_len; i++) {
        const mem_sim_t* cur = &flash_mem_list[i];
        uint32_t block_start = cur->start_addr;
        uint32_t block_end = cur->start_addr + cur->block_len;
        
        if (block_end < addr || block_start > addr_end) {
            continue;
        }

        if (cur->block_len == 0) {
            // no copy, use default 0xff
            has_block = 1;
            continue;
        }

        uint32_t overlap_start = (addr > block_start) ? addr : block_start;
        uint32_t overlap_end = (addr_end < block_end) ? addr_end : block_end;
        size_t overlap_len = overlap_end - overlap_start;

        // cal offset
        size_t buffer_offset = overlap_start - addr;
        size_t block_offset = overlap_start - block_start;

        // copy mem
        memcpy(out_buffer + buffer_offset, cur->data + block_offset, overlap_len);
        has_block = 1;
    }
    if (!has_block) {
        return -1;
    }
    return 0;
}

// ******** COMMAND HANDLERS ********

g_cmd_handler_entry_t* g_cmd_handlers = NULL;

/**
 * @brief NFC cmd handler
 */
static uint8_t cmd_0x01_handler(const uint8_t subcmd, const uint16_t payload_len, 
    const uint8_t* data_in, uint8_t* data_out) {
    switch(subcmd) {
        case 0x0c:
            // 61 12 50 10
            data_out[0] = 0x61;
            data_out[1] = 0x12;
            data_out[2] = 0x50;
            data_out[3] = 0x10;
            return 0x04;
        default:
            break;
    }
    return 0x00;
}

/**
 * @brief Flash Memory cmd handler 
 */
static uint8_t cmd_0x02_handler(const uint8_t subcmd, const uint16_t payload_len,
    const uint8_t* data_in, uint8_t* data_out) {
    switch(subcmd) {
        // read
        // cmd struct: 02 xx xx subcmd | magic | read_len 7e 00 00 | flash address(4 bytes little endian)
        case 0x04:
            uint32_t mem_addr = (data_in[14] << 16) | (data_in[13] << 8) | data_in[12];
            size_t read_len = data_in[8];
            if (read_len > 0x78) {
                ESP_LOGE(LOG_APP, "read_len is too large");
                return 0x00;
            }
            // init response magic
            memcpy(data_out, data_in + 8, 8);
            data_out[1] = 0x00;
            // read flash
            int read_ok = read_memory(mem_addr, read_len, data_out + 8);
            if (read_ok == 0) {
                return read_len + 8;
            }
            ESP_LOGE(LOG_APP, "Not implemented memory address: 0x%08X, len: 0x%02X", mem_addr, read_len);
            return 0x00;
        // TODO 0x05 write, 0x03 erase, 0x01 read block, 0x02 write block, 0x06 unknow
        default:
            break;
    }
    return 0x00;
}

/**
 * @brief Initialisation cmd handler
 */
static uint8_t cmd_0x03_handler(const uint8_t subcmd, const uint16_t payload_len,
    const uint8_t* data_in, uint8_t* data_out) {
    switch(subcmd) {
        case 0x07:
            #ifdef CONFIG_MCU_DEBUG
                ESP_LOGD(LOG_APP, "0x03 0x07 cmd, ns2 send ltk");
                // head (8 bytes) | addr (4 bytes) | ltk (16 bytes)
                log_print_ltk_hex("LTK", data_in + 8 + 6);
            #endif
            // no rsp data
            return 0x00;
        default:
            break;
    }
    return 0x00;
}

/**
 * @brief Unknow Init cmd handler
 */
static uint8_t cmd_0x07_handler(const uint8_t subcmd, const uint16_t payload_len,
    const uint8_t* data_in, uint8_t* data_out) {
    switch(subcmd) {
        case 0x01:
            data_out[0] = 0x00;
            return 0x01;
        case 0x02:
        default:
            break;
    }
    return 0x00;
}

/**
 * @brief encode feature info
 * @warning only support pro2 for now
 * 
 * @param flags feature flags
 * @param type device type
 * @param out feature encode buffer
 */
static void encode_feature_info(uint8_t flags, controller_type_t type, uint8_t out[8]) {
    // reset 0x00
    memset(out, 0x00, 8);

    if (flags & FEATURE_0C_01_BUTTON_STATE) {
        out[0] = 0x07;
    }

    if (flags & FEATURE_0C_02_ANALOG_STICKS) {
        out[1] = 0x07;
    }

    if (flags & FEATURE_0C_04_IMU) {
        out[2] = (type == CONTROLLER_TYPE_JOYCON) ? 0x03U : 0x01U;
    }

    if (flags & FEATURE_0C_08_UNUSED) {
        out[3] = (type == CONTROLLER_TYPE_JOYCON) ? 0x03U : 0x01U;
    }

    if (flags & FEATURE_0C_10_MOUSE_DATA) {
        out[4] = (type == CONTROLLER_TYPE_JOYCON) ? 0x03U : 0x01U;
    }

    if (flags & FEATURE_0C_20_RUMBLE) {
        out[5] = 0x03U;
    }

    // TODO Other Feature
}

/**
 * @brief Feature Select cmd handler.
 * These commands are used for configuring which features are enabled in the HID reports sent by the controller.
 */
static uint8_t cmd_0x0c_handler(const uint8_t subcmd, const uint16_t payload_len,
    const uint8_t* data_in, uint8_t* data_out) {
    switch(subcmd) {
        // get feature info
        case 0x01:
            // 00 00 00 00 | feature encode buffer (8 bytes)
            uint8_t feature_info[8] = { 0x00 };
            encode_feature_info(0x07, g_controller_firmware.type, feature_info);
            memset(data_out, 0x00, 0x04);
            memcpy(data_out + 4, feature_info, 0x08);
            return 0x0c;
        // set feature mask
        case 0x02:
        // clear feature mask
        case 0x03:
        // enable features
        case 0x04:
        // disable features
        case 0x05:
            // all response 
            // 00 00 00 00
            memset(data_out, 0x00, 0x04);
            return 0x04;
        default:
            break;
    }
    return 0x00;
}

/**
 * @brief Firmware Info cmd handler
 */
static uint8_t cmd_0x10_handler(const uint8_t subcmd, const uint16_t payload_len,
    const uint8_t* data_in, uint8_t* data_out) {
    switch(subcmd) {
        // https://github.com/ndeadly/switch2_controller_research/blob/master/commands.md#command-0x10---firmware-info
        // major minor micro(firmware version) | type(JC-L0x00 JC-R0x01 Pro20x02) | mmm(bluetooth patch version) | padding | mmm(DSP firmware version) | padding
        // 01 00 0e 02 0c 00 00 00 ff ff ff ff
        case 0x01:
            if (g_controller_firmware.type == CONTROLLER_TYPE_PRO2) {
                memcpy(data_out, pro2_firmware_info, PRO2_FIRMWARE_INFO_SIZE);
                return PRO2_FIRMWARE_INFO_SIZE;
            } else if (g_controller_firmware.type == CONTROLLER_TYPE_JOYCON) {
                // TODO Joycon firmware info
            }
            // default pro2
            memcpy(data_out, pro2_firmware_info, PRO2_FIRMWARE_INFO_SIZE);
            return PRO2_FIRMWARE_INFO_SIZE;
        default:
            break;
    }
    return 0x00;
}


static const uint8_t cmd_unknow_rsp_11_03[] = {
    // 01 20 03 00 00 0a e8 1c 3b 79 7d 8b 3a 0a e8 9c 42 58 a0 0b 42 0a e8 9c 41 58 a0 0b 41
    0x01, 0x20, 0x03, 0x00, 0x00, 0x0a, 0xe8, 0x1c, 
    0x3b, 0x79, 0x7d, 0x8b, 0x3a, 0x0a, 0xe8, 0x9c,
    0x42, 0x58, 0xa0, 0x0b, 0x42, 0x0a, 0xe8, 0x9c, 
    0x41, 0x58, 0xa0, 0x0b, 0x41
};
static const uint8_t cmd_unknow_rsp_11_03_len = sizeof(cmd_unknow_rsp_11_03);

/**
 * @brief Unknown 0x11 cmd handler
 */
static uint8_t cmd_0x11_handler(const uint8_t subcmd, const uint16_t payload_len,
    const uint8_t* data_in, uint8_t* data_out) {
    switch(subcmd) {
        case 0x01:
            // 01 00 00 00
            data_out[0] = 0x01;
            data_out[1] = 0x00;
            data_out[2] = 0x00;
            data_out[3] = 0x00;
            return 0x04;
        case 0x03:
            // 01 20 03 00 00 0a e8 1c 3b 79 7d 8b 3a 0a e8 9c 42 58 a0 0b 42 0a e8 9c 41 58 a0 0b 41
            memcpy(data_out, cmd_unknow_rsp_11_03, cmd_unknow_rsp_11_03_len);
            return cmd_unknow_rsp_11_03_len;
        default:
            break;
    }
    return 0x00;
}

/**
 * @brief Bluetooth Pairing cmd handler
 */
static uint8_t cmd_0x15_handler(const uint8_t subcmd, const uint16_t payload_len,
    const uint8_t* data_in, uint8_t* data_out) {
    switch(subcmd) {
        // exchange mac address
        case 0x01:
            // header(8 bytes) | 00 02 | addr (little endian) | addr-1 (little endian)
            uint8_t mac_addr[ESP_BD_ADDR_LEN] = { 0x00 };
            memcpy(mac_addr, data_in + 10, ESP_BD_ADDR_LEN);
            // ns2 mac addr check
            if (memcmp(g_console_ns2.ble_addr.val, mac_addr, ESP_BD_ADDR_LEN) == 0) {
                data_out[0] = 0x01;     // fixed
                data_out[1] = 0x04;     // magic
                data_out[2] = 0x01;     // size?
                // response controller mac addr (little endian)
                memcpy(data_out + 3, g_controller_firmware.addr_re, ESP_BD_ADDR_LEN);
                return ESP_BD_ADDR_LEN + 3;
            } else {
                ESP_LOGW(LOG_APP, "remote NS2 address:");
                log_print_addr(mac_addr);
                ESP_LOGW(LOG_APP, "local NS2 address:");
                log_print_addr(g_console_ns2.ble_addr.val);
                ESP_LOGE(LOG_APP, "NS2 address mismatch");
            }
            return 0x00;
        // exchange ltk
        case 0x04:
            // 00 | ltk (little endian) 
            uint8_t ltk_A1[LTK_KEY_SIZE] = { 0x00 };
            memcpy(ltk_A1, data_in + 9, LTK_KEY_SIZE);

            log_print_ltk_hex("LTK A1:", ltk_A1);

            // generate ltk
            for (int i = 0; i < LTK_KEY_SIZE; i++) {
                g_controller_firmware.ltk[i] = ltk_A1[LTK_KEY_SIZE - i - 1] ^ g_controller_firmware.ltk_key_b1[LTK_KEY_SIZE - i - 1];
            }

            // always response B1
            data_out[0] = 0x01;
            memcpy(data_out + 1, g_controller_firmware.ltk_key_b1, LTK_KEY_SIZE);
            return LTK_KEY_SIZE + 1;
        // confirm ltk
        case 0x02:
            // 00 | ltk (little endian)
            uint8_t data_A2[LTK_KEY_SIZE] = { 0x00 };
            uint8_t data_A2_re[LTK_KEY_SIZE] = { 0x00 };
            uint8_t data_B2[LTK_KEY_SIZE] = { 0x00 };
            uint8_t data_B2_re[LTK_KEY_SIZE] = { 0x00 };
            memcpy(data_A2_re, data_in + 9, LTK_KEY_SIZE);
            reverse_bytes(data_A2_re, data_A2, LTK_KEY_SIZE);

            log_print_ltk_hex("LTK", g_controller_firmware.ltk);
            log_print_ltk_hex("A2 RE", data_A2_re);

            int rc = aes128_ecb(g_controller_firmware.ltk, data_A2, data_B2);
            if (rc == 0) {
                reverse_bytes(data_B2, data_B2_re, LTK_KEY_SIZE);
                log_print_ltk_hex("B2", data_B2);
                log_print_ltk_hex("B2 RE", data_B2_re);

                reverse_bytes(g_controller_firmware.ltk, g_controller_firmware.ltk_re, LTK_KEY_SIZE);
                
                data_out[0] = 0x01;
                memcpy(data_out + 1, data_B2, LTK_KEY_SIZE);
                return LTK_KEY_SIZE + 1;
            }
            ESP_LOGE(LOG_APP, "confirm ltk failed");
            return 0x00;
        // paring finished
        case 0x03:
            // 00
            if (data_in[9] == 0x00) {
                rc = controller_pairing_info_save();
                rc += inject_pairing_info_to_ble_ctx();

                if (rc == 0) {
                    data_out[0] = 0x01;
                    // is_enc = true;
                    return 0x01;
                }
            }
            return 0x00;
        default:
            break;
    }
    return 0x00;
}

/**
 * @brief Unknown 0x16 cmd handler
 */
static uint8_t cmd_0x16_handler(const uint8_t subcmd, const uint16_t payload_len,
    const uint8_t* data_in, uint8_t* data_out) {
    if (subcmd == 0x01) {
        // len 0x18 all 0x00
        uint8_t res_data[0x18] = { 0x00 };
        memcpy(data_out, res_data, 0x18);
        // TODO Test
        // if (g_adv_opcode == 0x81) {
        //     // IF Already paired, save ltk_sec to ble context
        //     inject_pairing_info_to_ble_ctx();
        // }
        return 0x18;
    }
    return 0x00;
}

/**
 * @brief Unknown 0x18 cmd handler
 */
static uint8_t cmd_0x18_handler(const uint8_t subcmd, const uint16_t payload_len,
    const uint8_t* data_in, uint8_t* data_out) {
    switch(subcmd) {
        case 0x01:
            // 00 00 40 f0 00 00 60 00
            data_out[0] = 0x00;
            data_out[1] = 0x00;
            data_out[2] = 0x40;
            data_out[3] = 0xf0;
            data_out[4] = 0x00;
            data_out[5] = 0x00;
            data_out[6] = 0x60;
            data_out[7] = 0x00;
            return 0x08;
        default:
            break;
    }
    return 0x00;
}

int cmd_system_init() {
    int ret = 0;
    ret += cmd_handler_register(0x01, cmd_0x01_handler);
    ret += cmd_handler_register(0x02, cmd_0x02_handler);
    ret += cmd_handler_register(0x03, cmd_0x03_handler);
    ret += cmd_handler_register(0x07, cmd_0x07_handler);
    ret += cmd_handler_register(0x0c, cmd_0x0c_handler);
    ret += cmd_handler_register(0x10, cmd_0x10_handler);
    ret += cmd_handler_register(0x11, cmd_0x11_handler);
    ret += cmd_handler_register(0x15, cmd_0x15_handler);
    ret += cmd_handler_register(0x16, cmd_0x16_handler);
    ret += cmd_handler_register(0x18, cmd_0x18_handler);
    return ret;
}

int cmd_handler_register(uint8_t cmd, cmd_handler handler) {
    if (handler == NULL) return -1;
    g_cmd_handler_entry_t* cur = NULL;
    HASH_FIND(hh, g_cmd_handlers, &cmd, sizeof(uint8_t), cur);
    if (cur != NULL) {
        ESP_LOGW(LOG_APP, "cmd handler already registered, cmd: 0x%02x", cmd);
        ESP_LOGW(LOG_APP, "will overwrite the handler");
        cur->handler = handler;
        return 0;
    }

    cur = (g_cmd_handler_entry_t*)malloc(sizeof(g_cmd_handler_entry_t));
    if (cur == NULL) {
        ESP_LOGE(LOG_APP, "Failed to allocate memory for cmd handler entry, cmd: 0x%02x", cmd);
        return -1;
    }

    cur->cmd = cmd;
    cur->handler = handler;
    HASH_ADD(hh, g_cmd_handlers, cmd, sizeof(uint8_t), cur);
    return 0;
}

cmd_handler cmd_handler_find(uint8_t cmd) {
    g_cmd_handler_entry_t* cur = NULL;
    HASH_FIND(hh, g_cmd_handlers, &cmd, sizeof(uint8_t), cur);
    return cur != NULL ? cur->handler : NULL;
}

#define CMD_PROCESS_MAX_RSP_DATA_LEN 0x78

int cmd_process(pro2_gatt_rsp_t* rsp, uint8_t* data_in, uint16_t payload_len) {
    uint8_t rsp_magic[4] = { 0x10, 0x78, 0x00, 0x00 };
    uint8_t *data_out = (uint8_t*) malloc(CMD_PROCESS_MAX_RSP_DATA_LEN * sizeof(uint8_t));
    if (data_out == NULL) {
        ESP_LOGE(LOG_APP, "Failed to allocate memory for response data");
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    uint8_t out_len = 0;
    uint8_t cmd = rsp->cmd;
    uint8_t subcmd = rsp->subcmd;
    #ifdef CONFIG_MCU_DEBUG
        ESP_LOGI(LOG_APP, "process cmd 0x%02x, subcmd 0x%02x", cmd, subcmd);
    #endif

    cmd_handler handler = cmd_handler_find(cmd);
    if (handler != NULL) {
        out_len = handler(subcmd, payload_len, data_in, data_out);
    } else if(cmd == 0x09 || cmd == 0x0a) {
        ESP_LOGW(LOG_APP, "confirm cmd %02x, subcmd %02x, response default", cmd, subcmd);
        out_len = 0;
    } else {
        ESP_LOGW(LOG_APP, "unknow cmd %02x, subcmd %02x", cmd, subcmd);
        out_len = 0;
    }

    // Validate response length to prevent buffer overflow
    if (out_len > CMD_PROCESS_MAX_RSP_DATA_LEN) {
        ESP_LOGE(LOG_APP, "Response data length too large: %d, max allowed: %d", out_len, CMD_PROCESS_MAX_RSP_DATA_LEN);
        free(data_out);
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t *result = (uint8_t*) malloc((out_len + 8 + PRO2_DATA_EMPTY_LEN) * sizeof(uint8_t));
    if (result == NULL) {
        ESP_LOGE(LOG_APP, "Failed to allocate memory for response data");
        free(data_out);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    // reset rsp data
    memset(result, 0, PRO2_DATA_EMPTY_LEN * sizeof(uint8_t));
    // set rsp cmd bytes
    memcpy(result + PRO2_DATA_EMPTY_LEN, data_in, 4);
    // set rsp magic bytes
    memcpy(result + PRO2_DATA_EMPTY_LEN + 4, rsp_magic, 4);
    if (result[PRO2_DATA_EMPTY_LEN + 1] == 0x91) {
        result[PRO2_DATA_EMPTY_LEN + 1] = 0x01;
    }
    if (out_len > 0) {
        // empty bytes | cmd xx xx subcmd | magic | data
        memcpy(result + PRO2_DATA_EMPTY_LEN + 8, data_out, out_len);
    }
    rsp->rsp_data = result;
    rsp->rsp_len = 8 + PRO2_DATA_EMPTY_LEN + out_len;
    free(data_out);
    return 0;
}
