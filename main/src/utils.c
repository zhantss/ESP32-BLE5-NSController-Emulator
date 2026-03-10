#include "utils.h"

#include "mbedtls/aes.h"

void reverse_bytes(const uint8_t* in, uint8_t* out, size_t len) {
  for (size_t i = 0; i < len; i++) {
    out[i] = in[len - 1 - i];
  }
}

static int aes128_ecb_pri(mbedtls_aes_context aes_ctx, uint8_t* key, uint8_t* in, uint8_t* out) {
  int rc = 0;
  rc = mbedtls_aes_setkey_enc(&aes_ctx, key, 128);
  if (rc != 0) {
    return rc;
  }
  return mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, in, out);
}

int aes128_ecb(uint8_t* key, uint8_t* in, uint8_t* out) {
  mbedtls_aes_context aes_ctx;
  int rc = 0;
  mbedtls_aes_init(&aes_ctx);
  rc = aes128_ecb_pri(aes_ctx, key, in, out);
  mbedtls_aes_free(&aes_ctx);
  return rc;
}
