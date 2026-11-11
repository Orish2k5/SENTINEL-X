#ifndef SX_CRYPTO_MBEDTLS_H
#define SX_CRYPTO_MBEDTLS_H

#include <stddef.h>
#include <stdint.h>

/* Returns 1 only when the complete 32-byte HMAC-SHA-256 tag is valid. */
int sx_crypto_verify_hmac_sha256(const uint8_t *key, size_t key_len,
                                 const uint8_t *authenticated_bytes, size_t authenticated_len,
                                 const uint8_t received_tag[32]);
int sx_crypto_mbedtls_verify_callback(void *context,
                                      const uint8_t key[32],
                                      const uint8_t *authenticated_bytes,
                                      size_t authenticated_len,
                                      const uint8_t received_tag[32]);

#endif /* SX_CRYPTO_MBEDTLS_H */
