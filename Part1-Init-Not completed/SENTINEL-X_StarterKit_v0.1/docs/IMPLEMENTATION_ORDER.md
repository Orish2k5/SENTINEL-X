# Thứ tự triển khai code SENTINEL-X

## A. Những file đã có trên nhánh `develop`

| File | Đã có gì | Chưa có gì |
|---|---|---|
| `protocol/c/sentinel_x_protocol_v1.h` | Hằng số, offset, message type, opcode, UART/CRC constants | Parser, serializer, HMAC, state machine |
| `protocol/envelope_v1/test-vectors/envelope_v1_hmac.json` | Vector Envelope/HMAC công khai | Chỉ dùng test, không phải provisioning key |
| `tools/scripts/verify_vectors.py` | Kiểm tra HMAC vector, mutation âm tính, CRC known-answer và length | Chỉ là host verifier, không test C firmware hay phần cứng |
| `protocol/uart/uart_framing_v1.md` | Tóm tắt UART format | Chưa phải code parser/driver |

## B. Code nên viết đầu tiên

### P0 – protocol core, có thể kiểm thử độc lập trên PC

1. `protocol/python/sentinel_x_protocol/envelope.py`: Python reference cho EnvelopeV1 và HMAC.
2. `protocol/python/sentinel_x_protocol/uart.py`: UART framing, CRC16 và LINK_STATUS.
3. `protocol/python/sentinel_x_protocol/commands.py`: payload của SET_OUTPUT, SET_LEVEL, GET_STATUS.
4. `protocol/python/sentinel_x_protocol/policy.py`: encode/decode và validate PolicyV1.
5. `firmware/stm32/Core/Src/sx_crc16.c` + `Core/Inc/sx_crc16.h`.
6. `firmware/stm32/Core/Src/sx_uart_frame.c` + `Core/Inc/sx_uart_frame.h`.
7. `firmware/stm32/Core/Src/sx_envelope.c` + `Core/Inc/sx_envelope.h`.
8. `firmware/stm32/Core/Src/sx_policy.c` + `Core/Inc/sx_policy.h`.
9. `tests/python/test_protocol.py`, `tests/c/test_protocol_core.c`.

Lý do: trước khi ESP32, Edge và STM32 tích hợp, các phía phải hiểu đúng cùng một chuỗi byte. Test vector là điểm kiểm tra tương thích đầu tiên.

### P0 – tích hợp firmware STM32 sau khi protocol core đúng

10. Tạo project STM32CubeIDE/CubeMX cho đúng NUCLEO-F411RE.
11. Khởi tạo output OFF đầu tiên khi boot; sau đó mới bật UART/network path.
12. Cấu hình USART1 PB6/PB7 115200 8N1 và nhận byte bằng interrupt/DMA; ISR chỉ thu byte/đánh dấu frame.
13. `sx_auth_mbedtls.c`: HMAC-SHA-256 và constant-time compare; phải bật module Mbed TLS tương ứng và kiểm tra footprint Flash/RAM.
14. FRAM driver + two-slot journal cho inbound/outbound high-water sequence và active policy version.
15. `anti_replay_store`, `policy_engine`, `command_dispatch`, `actuator_service`, `link_monitor`, `event_store`, ACK/EVENT/STATUS output.

`sx_actuator_hal_example.c` trong gói là ví dụ dùng PB0/TIM3_CH3 theo pinout Proposed, chưa thay thế cấu hình `.ioc` và chưa chứng minh phần cứng.

### P0/P1 – sau khi STM32 protocol path đã chạy

16. ESP32-S3: Wi-Fi manager, MQTT client/TLS+ACL, UART transport, command forwarder, LINK_STATUS heartbeat và bounded telemetry queue.
17. Edge: database/migrations, auth/RBAC, secret loading, command sequence transaction, Envelope/HMAC, MQTT bridge, ACK/EVENT verification, audit.
18. PC Control Center: API client bất đồng bộ, login, device dashboard, command lifecycle, policy page, event timeline.
19. End-to-end test runner và evidence collection.

## C. Các việc không implement trong gói đầu tiên

- `StatusV1` flags: đang khác nhau giữa Blueprint v1.1 và Interface Contracts v1.0.
- REST idempotency: Blueprint dùng header `Idempotency-Key`, Contract v1.0 dùng `request_id` trong body.
- FRAM journal thật: chưa chốt model, dung lượng, địa chỉ, endurance và driver.
- UART streaming parser/DMA ring buffer: cần lựa chọn cách dùng HAL/interrupt/DMA trong `.ioc`.
- MQTT TLS certificates, credentials, Wi-Fi provisioning và Pi deployment: cần cấu hình lab thực tế.

Không tự suy đoán các mục này; ghi ADR/contract update trước khi implement.
