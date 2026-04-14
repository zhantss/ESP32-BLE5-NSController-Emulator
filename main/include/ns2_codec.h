#ifndef _NS2_CODEC_H_
#define _NS2_CODEC_H_

/**
 * @brief Controller BLE Protocol
 * @defgroup Controller BLE Protocol
 * @{  
 */

#include <stdint.h>
#include <stddef.h>

#include "uthash.h"

#ifdef __cplusplus
extern "C" {
#endif

// Special Data Features

#define PRO2_DATA_EMPTY_LEN     0x0e    // notify 0e/1e 0x00 len 14
#define NS2_DATA_EMPTY_LEN      0x21    // write 0x0016 0x00 len 33

/**
 * @defgroup Command Handlers
 * @{
 */

/**
 * @brief gatt response
 */
typedef struct {
    uint8_t cmd;
    uint8_t subcmd;
    uint8_t* rsp_data;
    uint16_t rsp_len;
} pro2_gatt_rsp_t;

/**
 * @brief command handler function prototype, the handler return response data length
 */
typedef uint8_t (*cmd_handler)(const uint8_t subcmd, const uint16_t payload_len, 
    const uint8_t* data_in, uint8_t* data_out);

/**
 * @brief command handler entry
 */
typedef struct {
    uint8_t cmd;
    cmd_handler handler;
    UT_hash_handle hh;
} g_cmd_handler_entry_t;

/**
 * @brief command handler hashmap
 */
extern g_cmd_handler_entry_t* g_cmd_handlers;

/**
 * @brief register a command handler
 * 
 * @param cmd command types
 * @param handler command handler function
 * @return int 0->success, -1->failed
 */
int cmd_handler_register(uint8_t cmd, cmd_handler handler);

/**
 * @brief find a command handler
 * 
 * @param cmd command types
 * @return cmd_handler command handler function, if NULL, not handler matched
 */
cmd_handler cmd_handler_find(uint8_t cmd);

/**
 * @brief init command system, register all statically suported command handlers
 * 
 * @return int 0->success, <0->failed
 */
int cmd_system_init();

/**
 * @brief process received command payload
 * 
 * @param rsp gatt response
 * @param data_in received data
 * @param payload_len received data length
 * @return int 0->success, -1->failed
 */
int cmd_process(pro2_gatt_rsp_t* rsp, uint8_t* data_in, uint16_t payload_len);

/** @} */

/**
 * @defgroup Feature Flags
 * @{
 */
#define FEATURE_0C_01_BUTTON_STATE          (0x01U)
#define FEATURE_0C_02_ANALOG_STICKS         (0x02U)
#define FEATURE_0C_04_IMU                   (0x04U)
#define FEATURE_0C_08_UNUSED                (0x08U)

// JoyCon Only
#define FEATURE_0C_10_MOUSE_DATA            (0x10U)
#define FEATURE_0C_20_RUMBLE                (0x20U)
#define FEATURE_0C_40_UNUSED                (0x40U)

#define FEATURE_0C_80_MAGNETOMETER          (0x80U)

/** @} */

/**
 * @defgroup Flash Memory Simulation
 * @{
 */

/**
 * @brief flash memory simulation structure
 */
typedef struct {
    uint32_t start_addr;
    size_t block_len;
    const uint8_t* data;
} mem_sim_t;

/**
 * @brief Set device info into the simulated 0x013000 flash memory block.
 */
void set_controller_specific(uint16_t product_id,
                               const uint8_t *serial, size_t serial_len,
                               const uint8_t version[3],
                               const uint8_t body_color[3],
                               const uint8_t buttons_color[3],
                               const uint8_t highlight_color[3],
                               const uint8_t grip_color[3]);

/**
 * @brief Simulated Memory Reading Function
 *
 * @param addr memory address
 * @param read_len reading data length
 * @param out_buffer reading data buffer
 * @return int 0->read ok, -1->failed
 */
int read_memory(uint32_t addr, size_t read_len, uint8_t* out_buffer);

/** @} */

#ifdef __cplusplus
}
#endif

/** @} */

#endif // _NS2_CODEC_H_