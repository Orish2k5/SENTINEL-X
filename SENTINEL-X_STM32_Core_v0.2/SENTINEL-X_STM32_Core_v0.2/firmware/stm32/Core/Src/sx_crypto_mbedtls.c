#include "sx_crypto_mbedtls.h"

#include <stddef.h>
#include <stdint.h>

#include "mbedtls/md.h"

static void sx_secure_zero(void *memory, size_t length)
{
    volatile uint8_t *p = (volatile uint8_t *)memory;
    while (length > 0u) {
        *p = 0u;
        ++p;
        --length;
    }
}

static int sx_constant_time_equal_32(const uint8_t a[32], const uint8_t b[32])
{
    uint8_t difference = 0u;
    size_t i;

    for (i = 0u; i < 32u; ++i) {
        difference |= (uint8_t)(a[i] ^ b[i]);
    }
    return difference == 0u ? 1 : 0;
}

int sx_crypto_verify_hmac_sha256(const uint8_t *key, size_t key_len,
                                 const uint8_t *authenticated_bytes, size_t authenticated_len,
                                 const uint8_t received_tag[32])
{
    const mbedtls_md_info_t *md_info;
    uint8_t calculated_tag[32];
    int valid = 0;

    if ((key == NULL) || (authenticated_bytes == NULL) || (received_tag == NULL) ||
        (key_len != 32u)) {
        return 0;
    }

    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        return 0;
    }

    if (mbedtls_md_hmac(md_info, key, key_len, authenticated_bytes,
                        authenticated_len, calculated_tag) == 0) {
        valid = sx_constant_time_equal_32(calculated_tag, received_tag);
    }

    sx_secure_zero(calculated_tag, sizeof(calculated_tag));
    return valid;
}

int sx_crypto_mbedtls_verify_callback(void *context,
                                      const uint8_t key[32],
                                      const uint8_t *authenticated_bytes,
                                      size_t authenticated_len,
                                      const uint8_t received_tag[32])
{
    (void)context;
    return sx_crypto_verify_hmac_sha256(key, 32u, authenticated_bytes,
                                        authenticated_len, received_tag);
}
