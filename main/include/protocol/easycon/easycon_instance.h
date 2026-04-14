#ifndef EASYCON_INSTANCE_H
#define EASYCON_INSTANCE_H

#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_PROTOCOL_LAYER_EASYCON
/**
 * @brief Pre-configured EasyCon protocol instance.
 *
 * Contains all EasyCon parsers registered in the recommended order:
 * hello -> short cmd -> simple cmd -> slice -> hid.
 */
extern protocol_instance_t easycon_protocol_instance;
#endif // CONFIG_PROTOCOL_LAYER_EASYCON

#ifdef __cplusplus
}
#endif

#endif // EASYCON_INSTANCE_H
