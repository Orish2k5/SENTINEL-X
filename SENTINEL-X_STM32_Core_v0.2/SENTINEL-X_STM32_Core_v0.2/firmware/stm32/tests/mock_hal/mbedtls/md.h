#ifndef SX_TEST_MBEDTLS_MD_H
#define SX_TEST_MBEDTLS_MD_H
#include <stddef.h>
#include <stdint.h>
typedef struct mbedtls_md_info_t mbedtls_md_info_t;
typedef enum { MBEDTLS_MD_SHA256 = 6 } mbedtls_md_type_t;
const mbedtls_md_info_t *mbedtls_md_info_from_type(mbedtls_md_type_t md_type);
int mbedtls_md_hmac(const mbedtls_md_info_t *md_info, const unsigned char *key,
                    size_t keylen, const unsigned char *input, size_t ilen,
                    unsigned char *output);
#endif
