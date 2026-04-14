#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>
#include <stddef.h>

// Reverse the order of bytes in an array.
void reverse_bytes(const uint8_t* in, uint8_t* out, size_t len);

// Encrypt data using AES-128 in ECB mode.
int aes128_ecb(uint8_t* key, uint8_t* in, uint8_t* out);

// Print LTK hex data
void log_print_ltk_hex(const char* data_name, const uint8_t *data);

// Print Addr (little endian)
void log_print_addr(const void *addr);

/**
 * @brief Peek a byte from a two-segment buffer (head + wrap-around).
 *
 * @param head      Pointer to the first contiguous segment.
 * @param head_len  Length of the first segment.
 * @param wrap      Pointer to the second (wrap-around) segment, may be NULL.
 * @param wrap_len  Length of the second segment.
 * @param idx       Index of the byte to fetch.
 * @return The byte at index idx, or 0 if out of bounds.
 */
uint8_t peek_byte(uint8_t *head, uint32_t head_len,
                  uint8_t *wrap, uint32_t wrap_len,
                  uint32_t idx);

#endif // _UTILS_H_