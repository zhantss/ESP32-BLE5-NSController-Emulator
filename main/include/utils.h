#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>
#include <stddef.h>

// Reverse the order of bytes in an array.
void reverse_bytes(const uint8_t* in, uint8_t* out, size_t len);

// Encrypt data using AES-128 in ECB mode.
int aes128_ecb(uint8_t* key, uint8_t* in, uint8_t* out);

#endif // _UTILS_H_