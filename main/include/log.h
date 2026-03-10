#define _LOG_H_
#ifndef _LOG_H_

#include <stdint.h>
#include <stddef.h>

#include "esp_log.h"

// Print LTK hex data
void log_print_ltk_hex(const char* data_name, const uint8_t *data);

// Print Addr (little endian)
void log_print_addr(const void *addr);

#endif

