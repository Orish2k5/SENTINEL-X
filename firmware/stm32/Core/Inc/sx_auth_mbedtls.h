#ifndef SX_AUTH_MBEDTLS_H
#define SX_AUTH_MBEDTLS_H
#include <stddef.h>
#include <stdint.h>
#include "sx_uart_frame.h"
/* Requires Mbed TLS MD/HMAC module and SHA-256 enabled in its configuration. */
sx_proto_status_t sx_hmac_sha256_compute(const uint8_t *key, size_t key_len,
                                         const uint8_t *message, size_t message_len,
                                         uint8_t out_tag[32]);
int sx_hmac_sha256_verify_envelope(const uint8_t *key, size_t key_len,
                                   const uint8_t *envelope, size_t envelope_len);
#endif

