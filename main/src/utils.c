#include "utils.h"

#include "aes/esp_aes.h"

void reverse_bytes(const uint8_t* in, uint8_t* out, size_t len) {
  for (size_t i = 0; i < len; i++) {
    out[i] = in[len - 1 - i];
  }
}

static int aes128_ecb_pri(esp_aes_context aes_ctx, uint8_t* key, uint8_t* in, uint8_t* out) {
  int rc = 0;
  rc = esp_aes_setkey(&aes_ctx, key, 128);
  if (rc != 0) {
    return rc;
  }
  return esp_aes_crypt_ecb(&aes_ctx, ESP_AES_ENCRYPT, in, out);
}

int aes128_ecb(uint8_t* key, uint8_t* in, uint8_t* out) {
  esp_aes_context aes_ctx;
  int rc = 0;
  esp_aes_init(&aes_ctx);
  rc = aes128_ecb_pri(aes_ctx, key, in, out);
  esp_aes_free(&aes_ctx);
  return rc;
}

uint8_t peek_byte(uint8_t *head, uint32_t head_len,
                  uint8_t *wrap, uint32_t wrap_len,
                  uint32_t idx)
{
  if (idx < head_len) {
    return head[idx];
  }
  idx -= head_len;
  if (idx < wrap_len) {
    return wrap[idx];
  }
  return 0;
}
