#include "sx_auth_mbedtls.h"
#include "sx_envelope.h"
#include <mbedtls/md.h>
static int constant_time_equal(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t diff = 0u; size_t i;
    for (i = 0u; i < n; ++i) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0u;
}
sx_proto_status_t sx_hmac_sha256_compute(const uint8_t *key, size_t key_len,
                                         const uint8_t *message, size_t message_len,
                                         uint8_t out_tag[32])
{
    const mbedtls_md_info_t *info;
    int rc;
    if ((key == NULL) || (message == NULL) || (out_tag == NULL) || (key_len != 32u)) return SX_PROTO_ERR_ARGUMENT;
    info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (info == NULL) return SX_PROTO_ERR_FIELD;
    rc = mbedtls_md_hmac(info, key, key_len, message, message_len, out_tag);
    return (rc == 0) ? SX_PROTO_OK : SX_PROTO_ERR_FIELD;
}
int sx_hmac_sha256_verify_envelope(const uint8_t *key, size_t key_len,
                                   const uint8_t *envelope, size_t envelope_len)
{
    sx_envelope_view_t view; uint8_t expected[32];
    if ((key == NULL) || (key_len != 32u)) return 0;
    if (sx_envelope_parse(envelope, envelope_len, &view) != SX_PROTO_OK) return 0;
    if (sx_hmac_sha256_compute(key, key_len, view.authenticated_bytes, view.authenticated_len, expected) != SX_PROTO_OK) return 0;
    return constant_time_equal(expected, view.tag, sizeof(expected));
}
