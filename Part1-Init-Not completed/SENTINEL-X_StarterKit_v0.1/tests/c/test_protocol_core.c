#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sx_crc16.h"
#include "sx_uart_frame.h"
#include "sx_envelope.h"
#include "sx_policy.h"
static size_t hex_decode(const char *hex, uint8_t *out, size_t cap)
{
    size_t n = strlen(hex) / 2u, i;
    assert((strlen(hex) % 2u) == 0u && n <= cap);
    for (i = 0u; i < n; ++i) { unsigned v = 0u; assert(sscanf(hex + i * 2u, "%2x", &v) == 1); out[i] = (uint8_t)v; }
    return n;
}
int main(void)
{
    static const char vector_hex[] =
      "5358010153582d30303100000000000000000000010000000000000001"
      "123e4567e89b12d3a456426614174000000100020101"
      "96a09046cc1f69b877c97df4ea4b58bab89eae59560a4b92fffd374f91e570ea";
    static const uint8_t check[] = "123456789";
    static const uint8_t policy_payload[29] = {
        0x00,0x00,0x00,0x01, 0x00,0x05, 0x03,0x01,
        0x00,0x01,0x01, 0x00,0x00, 0x00,0x01,
        0x00,0x02,0x01, 0x00,0x00, 0x03,0xE8,
        0x00,0x03,0x01, 0x00,0x00, 0x00,0x00
    };
    uint8_t env[209], frame[216]; size_t env_len, frame_len;
    sx_policy_t policy;
    sx_envelope_view_t ev; sx_uart_frame_view_t fv;
    assert(sx_crc16_ccitt_false(check, 9u) == 0x29B1u);
    env_len = hex_decode(vector_hex, env, sizeof(env)); assert(env_len == 83u);
    assert(sx_envelope_parse(env, env_len, &ev) == SX_PROTO_OK);
    assert(ev.message_type == SX_MSG_CMD && ev.sequence == 1u && ev.operation_code == SX_OP_SET_OUTPUT);
    assert(ev.payload_len == 2u && ev.payload[0] == 1u && ev.payload[1] == 1u);
    assert(sx_envelope_device_id_matches(&ev, "SX-001") == 1);
    assert(sx_envelope_device_id_matches(&ev, "SX-002") == 0);
    frame_len = sx_uart_frame_build(SX_UART_FRAME_AUTH_ENVELOPE, env, (uint16_t)env_len, frame, sizeof(frame));
    assert(frame_len == 90u);
    assert(sx_uart_frame_parse(frame, frame_len, &fv) == SX_PROTO_OK);
    assert(fv.frame_type == SX_UART_FRAME_AUTH_ENVELOPE && fv.payload_len == env_len);
    assert(sx_policy_parse_validate(policy_payload, sizeof(policy_payload), 0u, &policy) == SX_PROTO_OK);
    assert(policy.policy_version == 1u && policy.entry_count == 3u);
    assert(sx_policy_parse_validate(policy_payload, sizeof(policy_payload), 1u, &policy) == SX_PROTO_ERR_FIELD);
    frame[12] ^= 1u;
    assert(sx_uart_frame_parse(frame, frame_len, &fv) == SX_PROTO_ERR_CRC);
    puts("PASS C protocol core: CRC16, EnvelopeV1 structure, UART framing, PolicyV1 validation");
    puts("NOTE: Mbed TLS HMAC adapter is compiled in STM32 project after Mbed TLS is configured.");
    return 0;
}
