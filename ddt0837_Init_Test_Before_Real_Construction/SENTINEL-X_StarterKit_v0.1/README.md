# SENTINEL-X – Starter Kit v0.1

Gói code khởi đầu cho SENTINEL-X, bám theo các phần **đang thống nhất** giữa Blueprint v1.1 và Interface Contracts v1.0 trong repository `Orish2k5/SENTINEL-X`.

## 1. Phạm vi gói này

Gói này cố ý bắt đầu từ protocol core để có thể kiểm thử độc lập trước khi nối phần cứng:

- Python protocol reference: EnvelopeV1, HMAC-SHA-256, command payload, UART frame/CRC16 và PolicyV1.
- C protocol core dùng cho STM32: CRC16, UART frame parser/builder, EnvelopeV1 parser, PolicyV1 validator.
- Adapter HMAC tùy chọn cho Mbed TLS.
- Một ví dụ HAL riêng cho output PWM PB0 / TIM3_CH3 theo pinout **Proposed** trong Blueprint.
- Unit tests Python và C chạy trên máy host.

**Đây chưa phải firmware/ứng dụng hoàn chỉnh.** Gói chưa có project `.ioc`/CubeIDE đã cấu hình, project ESP-IDF, Edge Backend, PC GUI, FRAM driver thực tế, TLS certificate, provisioning key hay deployment config.

## 2. Thứ tự khuyến nghị

1. Chạy Python test vector và unit tests.
2. Chạy C host tests để kiểm tra các phần C portable.
3. Đưa các module C vào STM32CubeIDE, cấu hình include path và build cho đúng NUCLEO-F411RE.
4. Xác nhận PB0/TIM3_CH3, USART1 PB6/PB7 và FRAM trên đúng phần cứng; chỉ sau khi bench-test mới đổi trạng thái pinout từ Proposed.
5. Tích hợp Mbed TLS HMAC vào firmware và chạy lại test vector ở cả Python/C.
6. Chọn model FRAM, triển khai journal hai slot và kiểm thử power-cut trước khi bật xử lý command thực.
7. Khi protocol core ổn định mới viết ESP32-S3 bridge, Edge Backend và PC Control Center.

## 3. Cấu trúc

```text
protocol/
  c/sentinel_x_protocol_v1.h        # constants tương thích header trong repo
  python/sentinel_x_protocol/       # Python reference implementation
  vectors/envelope_v1_hmac.json    # public test vector, key chỉ dùng test
firmware/stm32/
  Core/Inc/                         # C headers
  Core/Src/                         # C implementations portable
  App/                              # ví dụ HAL output; pinout Proposed
 tests/
  python/                            # unittest
  c/                                 # host C tests
 tools/
  verify_test_vector.py
  run_c_tests.sh
```

## 4. Chạy kiểm thử

Yêu cầu Python 3.10+ (chỉ dùng standard library):

```bash
python tools/verify_test_vector.py
python -m unittest discover -s tests/python -v
```

Yêu cầu GCC/Clang để chạy C host tests:

```bash
bash tools/run_c_tests.sh
```

Windows PowerShell có thể chạy Python tests trực tiếp; C tests cần MinGW-w64 GCC, MSYS2 hoặc WSL có `gcc`.

## 5. Cách đưa vào repository hiện tại

Copy các thư mục `protocol/python`, `protocol/vectors`, `firmware/stm32/Core/Inc`, `firmware/stm32/Core/Src`, `firmware/stm32/App`, `tests`, `tools` vào repository. Header `protocol/c/sentinel_x_protocol_v1.h` trong gói này tương ứng với file cùng tên hiện có trên `develop`; so sánh trước khi thay thế.

Không copy đè những file hiện có nếu nhóm đã cập nhật chúng sau thời điểm gói này được tạo.

## 6. Các điểm chưa được tự ý chốt

Blueprint v1.1 và Interface Contracts v1.0 có một số khác biệt. Gói này chỉ implement các phần không bị ảnh hưởng trực tiếp: EnvelopeV1 layout, HMAC test vector, UART framing, command payload cơ bản và PolicyV1. Chưa implement `StatusV1` flags hoặc REST idempotency để tránh chọn ngầm một trong hai cách đặc tả.

Cần có decision/ADR hoặc Interface Contracts phiên bản mới để thống nhất tối thiểu:

- Bit layout của `StatusV1` (Blueprint: WIFI bit 2, MQTT bit 3, event queue bit 4; Contract v1.0: EDGE/WIFI bit 2, event queue bit 3).
- REST idempotency (`Idempotency-Key` HTTP header trong Blueprint so với `request_id` trong JSON body của Contract v1.0).
- Namespace mã lỗi MCU so với lỗi Edge/API.
- Cấu trúc repository chính thức và đường dẫn master contract.

## 7. An toàn và bảo mật

- Public key trong test vector **không được dùng làm deployment key**.
- Không commit HMAC key thật, Wi-Fi password, MQTT credential, bearer token hoặc private key.
- HMAC không mã hóa payload.
- CRC16 chỉ phát hiện lỗi truyền, không xác thực thiết bị.
- Mọi thao tác output thật chỉ nên được bật sau khi kiểm thử negative cases và safe-state trên tải điện áp thấp.
- Pinout PB0/TIM3_CH3 là Proposed theo Blueprint; không nối tải điện lưới.
