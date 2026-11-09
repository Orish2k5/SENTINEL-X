# SENTINEL-X | Interface Contracts v1.0

**Purpose:** authoritative shared contract for STM32, ESP32-S3, Edge backend and PC Control Center.
**Protocol baseline:** `EnvelopeV1`, binary HMAC-SHA-256 envelope, UART frame, MQTT topics, REST API, RBAC, error registry and test vector.
**Change rule:** any incompatible change requires an explicit version/ADR plus updated tests in `shared/protocol/test-vectors/`.

## Mục lục

3.1. Thuật ngữ bắt buộc
3.2. Byte order, string, ID
3.3. EnvelopeV1 layout
3.4. Message type và operation code
3.5. Command payload schemas
3.6. ACK payload
3.7. Event payload
3.8. Status payload
3.9. PolicyV1 payload
3.10. HMAC, key provisioning và replay protection
3.11. UART framing
3.12. MQTT contract
3.13. REST API contract
3.14. Authentication and RBAC
3.15. Error/result code registry
3.16. Modbus RTU/RS-485 extension
3.17. Protocol test vector

## 3. Interface Contracts v1.0

**Trạng thái:** các giá trị trong chương này là baseline của repository. Mọi implementation phải tuân thủ. Thay đổi envelope/protocol/API phải tạo version hoặc ADR và cập nhật test vector.

### 3.1. Thuật ngữ bắt buộc

- **SHALL / MUST:** yêu cầu bắt buộc.
- **SHOULD:** nên thực hiện; ngoại lệ phải có lý do.
- **MAY:** tùy chọn.
- **Command:** yêu cầu điều khiển từ Edge xuống MCU.
- **Envelope:** binary message được HMAC bảo vệ.
- **Sequence:** số thứ tự tăng đơn điệu riêng cho mỗi chiều truyền.
- **Message ID:** UUID 128-bit dùng correlation/dedup; không thay thế sequence.
- **Policy version:** số phiên bản policy tăng đơn điệu, độc lập với sequence.

### 3.2. Byte order, string, ID

- Integer nhiều byte dùng **big-endian/network byte order**.
- `device_id` là chuỗi ASCII in hoa, chỉ ký tự `[A-Z0-9_-]`, tối đa 16 byte; mã hóa vào field fixed 16 byte, padding cuối bằng `0x00`. Baseline ID: `SX-001`.
- `message_id` là 16 byte UUID theo thứ tự byte chuẩn RFC 4122/network order; không truyền chuỗi UUID ASCII trong envelope.
- Payload là byte-array theo schema riêng của từng `message_type`/`operation_code`; không có JSON bên trong envelope.
- Giá trị dự trữ phải bằng 0 trong version 1; nếu không đúng, reject.
- Max payload: 128 byte. Max envelope: 49-byte header + 128 payload + 32-byte tag = **209 byte**.

### 3.3. EnvelopeV1 — layout chính thức

HMAC được tính trên toàn bộ header + payload, bỏ qua trường tag vì tag nằm cuối. Header luôn dài 49 byte.

| Offset | Size | Field | Quy tắc |
|---:|---:|---|---|
| 0 | 2 | `magic` | ASCII `SX` = `0x53 0x58` |
| 2 | 1 | `protocol_version` | `0x01` |
| 3 | 1 | `message_type` | CMD `0x01`; ACK `0x02`; EVENT `0x03`; POLICY_UPDATE `0x04`; STATUS `0x05` |
| 4 | 16 | `device_id` | ASCII zero-padded |
| 20 | 1 | `key_id` | `0x01` cho baseline |
| 21 | 8 | `sequence` | uint64 big-endian; tăng đơn điệu theo chiều gửi |
| 29 | 16 | `message_id` | UUID bytes |
| 45 | 2 | `operation_code` | uint16 big-endian; tra bảng code registry |
| 47 | 2 | `payload_len` | uint16 big-endian, 0..128 |
| 49 | N | `payload` | đúng `payload_len` byte |
| 49+N | 32 | `tag` | HMAC-SHA-256 đầy đủ 32 byte |

`tag = HMAC-SHA-256(K_device, envelope[0 : 49 + payload_len])`.

- So sánh tag phải constant-time.
- Không cắt tag trong baseline.
- Kiểm tra `payload_len` và tổng độ dài thực tế trước khi truy cập trường.
- HMAC không mã hóa payload; bí mật nội dung không phải mục tiêu của HMAC.
- ESP32 SHALL forward envelope nguyên byte và không sửa/re-serialize.

### 3.4. Message type và operation code

| Message type | Hướng | Code / payload |
|---|---|---|
| `CMD (0x01)` | Edge → STM32 | `0x0001 SET_OUTPUT`; `0x0002 SET_LEVEL`; `0x0003 GET_STATUS` |
| `ACK (0x02)` | STM32 → Edge | `operation_code` echo command code đã xử lý; payload là ACK schema |
| `EVENT (0x03)` | STM32 → Edge | `operation_code` là event code (`0x2001` trở đi); payload event schema |
| `POLICY_UPDATE (0x04)` | Edge → STM32 | `operation_code = 0x0100`; payload PolicyV1 |
| `STATUS (0x05)` | STM32 → Edge | `operation_code = 0`; payload StatusV1 |

**Quy tắc hướng:** STM32 chỉ chấp nhận inbound `CMD` và `POLICY_UPDATE` có HMAC hợp lệ. `ACK`, `EVENT`, `STATUS` do STM32 phát; Edge kiểm tra HMAC trước khi lưu/hiển thị như sự kiện đáng tin cậy.

### 3.5. Command payload schemas

| Command | Code | Payload | Validation |
|---|---:|---|---|
| `SET_OUTPUT` | `0x0001` | byte 0 `output_id`; byte 1 `state` | length=2; baseline `output_id=1`; `state ∈ {0,1}` |
| `SET_LEVEL` | `0x0002` | byte 0 `channel_id`; bytes 1..2 `level_permille` uint16 BE | length=3; baseline `channel_id=1`; `level_permille ∈ [0,1000]` |
| `GET_STATUS` | `0x0003` | empty | length=0; chỉ đọc trạng thái; được phép cả trong `SAFE_MODE` nếu frame hợp lệ |

Mọi output command SHALL có kiểm tra hard-coded absolute bounds trong firmware. Policy động có thể thu hẹp quyền/giới hạn; không được mở rộng vượt absolute bounds.

### 3.6. ACK payload — chính xác 34 byte

| Offset trong payload | Size | Field | Quy tắc |
|---:|---:|---|---|
| 0 | 16 | `correlation_message_id` | UUID lệnh ban đầu |
| 16 | 8 | `correlation_sequence` | sequence lệnh ban đầu |
| 24 | 2 | `result_code` | bảng Result/Error Code |
| 26 | 1 | `execution_state` | `0=REJECTED`; `1=COMPLETED`; `2=UNKNOWN/INDETERMINATE` |
| 27 | 4 | `active_policy_version` | uint32 BE |
| 31 | 1 | `output_state` | `0=OFF`; `1=ON`; `255=UNKNOWN` |
| 32 | 2 | `output_level_permille` | 0..1000; `65535=UNKNOWN` |

ACK có `sequence` riêng của STM32→Edge; không dùng lại sequence của command trong field envelope. `correlation_sequence` và `correlation_message_id` dùng để đối chiếu request.

Nếu mất nguồn sau khi sequence đã commit nhưng trước khi actuation/ACK hoàn tất, kết quả có thể `UNKNOWN/INDETERMINATE`. Edge SHALL không tự gửi lại cùng command như một command mới. Người dùng phải đọc trạng thái và quyết định thao tác tiếp theo.

### 3.7. Event payload — 24 byte baseline

| Offset | Size | Field | Quy tắc |
|---:|---:|---|---|
| 0 | 1 | `severity` | `0=INFO`; `1=WARNING`; `2=SECURITY`; `3=ERROR` |
| 1 | 16 | `related_message_id` | UUID liên quan hoặc 16 byte zero nếu không có |
| 17 | 4 | `active_policy_version` | uint32 BE |
| 21 | 1 | `output_state` | 0 OFF; 1 ON; 255 UNKNOWN |
| 22 | 2 | `output_level_permille` | 0..1000; 65535 UNKNOWN |

Baseline event codes: `0x2001 AUTH_FAILURE`; `0x2002 REPLAY_REJECT`; `0x2003 POLICY_DENY`; `0x2004 RATE_LIMIT`; `0x2005 LINK_LOST`; `0x2006 STORAGE_FAIL`; `0x2007 POLICY_APPLIED`; `0x2008 TAMPER`; `0x2009 PARAM_REJECT`; `0x200A SAFE_MODE_ENTERED`. Event queue có giới hạn; khi đầy, không được ảnh hưởng quyết định bảo mật, phải tăng `dropped_event_count` và giữ lại event an ninh quan trọng theo policy.

### 3.8. Status payload — 31 byte baseline

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | `fw_major` |
| 1 | 1 | `fw_minor` |
| 2 | 1 | `fw_patch` |
| 3 | 1 | `flags`: bit0 SAFE_MODE; bit1 UART_LINK_UP; bit2 EDGE/WIFI_UP (theo link status cuối nhận); bit3 EVENT_QUEUE_NONEMPTY |
| 4 | 4 | `active_policy_version` uint32 BE |
| 8 | 8 | `last_rx_sequence` uint64 BE |
| 16 | 4 | `uptime_seconds` uint32 BE |
| 20 | 1 | `output_state` |
| 21 | 2 | `output_level_permille` uint16 BE |
| 23 | 4 | `accepted_count` uint32 BE |
| 27 | 4 | `rejected_count` uint32 BE |

### 3.9. PolicyV1 payload

- Byte 0..3: `policy_version` uint32 BE; MUST lớn hơn `active_policy_version`.
- Byte 4..5: `max_commands_per_second` uint16 BE; baseline range 1..20, default 5.
- Byte 6: `entry_count` uint8; range 1..8.
- Byte 7: `flags`; bit0 = `FORCE_OFF_ON_LINK_LOSS`; bit1 reserved=0; bits khác reserved=0.
- Sau đó là `entry_count` entries, mỗi entry 7 byte: `command_code` uint16 BE, `allowed` uint8 (0/1), `min_value` uint16 BE, `max_value` uint16 BE.
- Total payload length = `8 + 7 × entry_count`, không vượt 64 byte trong baseline.
- Không được có command code trùng; `min_value ≤ max_value`; values không được vượt hard-coded absolute range của command; `GET_STATUS` có min=max=0.
- Chính sách chỉ chuyển sang ACTIVE khi STM32 kiểm tra hợp lệ, lưu bền vững và trả ACK `SUCCESS` kèm version mới. Pi hiển thị `PENDING` cho đến khi ACK; không được hiển thị `ACTIVE` ngay lúc nhấn Save.
- Nếu policy lỗi, giữ policy cũ. Nếu không tồn tại policy hợp lệ, vào SAFE_MODE và chỉ chấp nhận status/commissioning được xác thực.

### 3.10. HMAC, key provisioning và replay protection

**Cơ chế baseline:** HMAC-SHA-256 với khóa ngẫu nhiên 32 byte riêng cho mỗi device. Quyết định chọn HMAC thay vì ECDSA P-256 được đưa ra vì lịch 8 tuần và rủi ro tích hợp; HMAC đòi hỏi Pi và MCU đều giữ cùng một khóa. Đây là điểm yếu cần ghi rõ: nếu Pi/key bị chiếm thì attacker có thể ký lệnh hợp lệ. HMAC không cung cấp non-repudiation.

**Key handling:**
- Sinh khóa từ CSPRNG trên host quản trị (`secrets.token_bytes(32)` hoặc công cụ tương đương); không dùng khóa mặc định trong bản chạy.
- Key được provisioning riêng cho MCU và dịch vụ Edge; không truyền key qua MQTT/UART sau khi commissioning; không commit key, không log key, không đưa key vào README/test report.
- File secret trên Pi phải thuộc tài khoản service và quyền truy cập tối thiểu; file generated header/config có key MCU phải nằm trong `.gitignore`.
- Test vector dùng khóa công khai trong tài liệu chỉ dành cho kiểm thử; tuyệt đối không dùng làm deployment key.
- Baseline `key_id=1`; key rotation/provisioning ceremony là P2, nhưng key replacement thủ công phải được ghi lại và thiết bị phải fail-closed nếu key ID không biết.

**Sequence và phục hồi lỗi:**
- Hai chiều có sequence độc lập: `Edge → MCU` và `MCU → Edge`.
- Edge cấp sequence outbound trong transaction SQLite (`BEGIN IMMEDIATE`), tăng và commit trước khi publish; sequence gap được phép, sequence không được tái sử dụng.
- STM32 kiểm tra HMAC trước khi chấp nhận sequence. Nếu hợp lệ và `sequence > last_rx_sequence`, phải ghi high-water mark bền vững **trước khi điều khiển đầu ra**. Sequence cũ/lặp bị reject.
- Sequence hợp lệ được tiêu thụ sau khi HMAC/header/device hợp lệ, kể cả khi command bị policy từ chối; các sequence gap được phép.
- Persist high-water qua FRAM journal hai slot có generation + CRC + commit marker. Nếu cả hai slot hỏng hoặc không đọc/ghi được, không đặt counter về 0; vào SAFE_MODE và yêu cầu quy trình commissioning/recovery.
- MCU outbound sequence/event dedup cũng phải được lưu bền vững hoặc dùng storage journal tương đương để event replay sau reboot không tạo duplicate audit.
- Nếu SQLite bị restore từ backup cũ và sequence thấp hơn MCU, MCU từ chối sequence; Edge ghi `SEQUENCE_STATE_MISMATCH`, ngừng publish commands và yêu cầu recovery/commissioning có kiểm soát. Không tự giảm counter MCU.
- Mỗi outbound command có UUID `message_id`; MQTT QoS 1 có thể giao trùng. Duplicate envelope cùng sequence bị từ chối ở MCU; Edge deduplicate ACK/event theo `(device_id, direction, sequence, message_id)`.

### 3.11. UART framing ESP32 ↔ STM32

UART truyền **binary frame**, không truyền JSON. UART là 8 data bits, no parity, 1 stop bit (8N1), baseline 115200 baud. MCU sử dụng interrupt/DMA để thu byte; ISR chỉ ghi buffer/đánh dấu frame, không làm HMAC hoặc điều khiển actuator.

| Field | Size | Quy tắc |
|---|---:|---|
| SOF | 2 | `0xA5 0x5A` |
| frame_type | 1 | `0x01=AUTH_ENVELOPE`; `0x02=LINK_STATUS` |
| payload_len | 2 | uint16 BE, số byte payload theo sau |
| payload | N | `AUTH_ENVELOPE`: 81..209 byte; `LINK_STATUS`: đúng 7 byte |
| crc16 | 2 | CRC-16/CCITT-FALSE, init `0xFFFF`, poly `0x1021`, xorout `0x0000`, tính trên `frame_type + payload_len bytes + payload`; lưu big-endian |

Max frame = 2 + 1 + 2 + 209 + 2 = **216 byte**. Parser SHALL kiểm tra length giới hạn trước khi ghi buffer, CRC trước khi chuyển payload cho HMAC, timeout frame và resync theo SOF. Khi UART framing/CRC sai, không thực thi, tăng counter và ghi event nếu có thể.

`LINK_STATUS` payload 7 byte: `wifi_state u8` (0 offline/1 online), `mqtt_state u8` (0 offline/1 online), `reserved u8=0`, `esp32_uptime_ms u32 BE`. ESP32 gửi khi trạng thái đổi và heartbeat mỗi 500 ms. Frame này dùng CRC nhưng không HMAC; nó chỉ báo liveness và có thể làm trạng thái an toàn nghiêm hơn, **không bao giờ cấp quyền cho command**. MCU vào offline safe-state nếu trạng thái được báo offline hoặc link-status vắng > 3000 ms. Rủi ro spoof link status trên UART vật lý được coi là ngoài threat model prototype và phải được ghi trong báo cáo.

### 3.12. MQTT contract

- Protocol: MQTT 3.1.1; QoS 1 cho command/ack/event/status; mọi client phải xử lý duplicate delivery.
- MQTT payload trên command/ack/event/status là raw bytes của `EnvelopeV1`, không phải JSON và không base64.
- `retain=false` cho command, ack, event; retained status không dùng trong baseline để tránh dùng nhầm trạng thái cũ như trạng thái hiện tại.
- Không bật anonymous access; broker ACL mặc định deny.
- ESP32 client ID: `sentinel-esp32-SX-001`.
- Broker user chỉ được subscribe `sentinel/v1/devices/SX-001/cmd` và publish `.../ack`, `.../events`, `.../status`, `.../connectivity`; không được publish `cmd`.
- TLS với certificate validation là baseline triển khai trên LAN khi CA/cert đã được dựng; self-signed certificate được phép trong lab nếu ESP32 xác minh đúng CA/fingerprint theo cấu hình. Không tắt certificate verification chỉ để làm demo chạy.

| Topic | Publisher | Subscriber | Payload |
|---|---|---|---|
| `sentinel/v1/devices/{id}/cmd` | Edge command service | ESP32 | Envelope command/policy bytes |
| `sentinel/v1/devices/{id}/ack` | ESP32 forwarder | Edge event consumer | Envelope ACK bytes |
| `sentinel/v1/devices/{id}/events` | ESP32 forwarder | Edge event consumer | Envelope EVENT bytes |
| `sentinel/v1/devices/{id}/status` | ESP32 forwarder | Edge event consumer | Envelope STATUS bytes |
| `sentinel/v1/devices/{id}/connectivity` | ESP32 | Edge/UI service | Small JSON transport-state notice; not a security decision |

MQTT QoS 1 đảm bảo giao nhận ít nhất một lần trong phạm vi giao thức, không đảm bảo actuator thực thi đúng một lần. Sequence + message ID + command status làm cơ chế duplicate/retry tại application layer.

### 3.13. REST API contract

Base path: `/api/v1`. JSON UTF-8; timestamps ở Edge dùng RFC 3339 UTC. Command status bất đồng bộ: API trả `202 Accepted`, UI theo dõi trạng thái bằng `GET`.

| Method / path | Role | Request / result |
|---|---|---|
| `POST /auth/login` | Public, rate-limited | `{username,password}` → `{access_token,token_type,expires_at,user}` |
| `POST /auth/logout` | Authenticated | revoke server-side session; token hết hiệu lực ngay |
| `GET /auth/me` | Authenticated | user ID, display name, role, expiry |
| `GET /devices` | Any authenticated | danh sách device được cấp quyền xem |
| `GET /devices/{id}/status` | Any authenticated | last known status + `observed_at` + `is_stale` |
| `POST /devices/{id}/commands` | Operator/Admin | tạo command, trả `202` + `command_id` + `status=QUEUED` |
| `GET /commands/{command_id}` | Owner/authorized role | queued/published/accepted/rejected/completed/timeout/indeterminate |
| `GET /policies/devices/{id}` | Any authenticated; detail may be restricted | active policy, desired version, device-confirmed version |
| `PUT /policies/devices/{id}` | Admin | update policy with `expected_version`; response pending until device ACK |
| `GET /events` | Authenticated, filtered by role | paginated filters for time/device/severity/category |
| `GET /reports/events.csv` | Viewer/Auditor/Admin | CSV export under role policy; audit export action |
| `POST /security-tests/runs` | Operator/Admin, test device only | predefined safe test IDs; no arbitrary target IP/network |
| `GET /security-tests/runs/{run_id}` | Authorized role | case results, timestamps, counts, evidence references |
| `POST /users` / `PATCH /users/{id}` | Admin | create/disable account, assign allowed role; all changes audited |

**Command request**
```json
{
  "request_id": "123e4567-e89b-12d3-a456-426614174000",
  "command": "SET_OUTPUT",
  "params": {"output_id": 1, "state": 1}
}
```

`request_id` là khóa idempotency do PC cung cấp; với cùng `(user_id, request_id)`, server phải trả lại command record hiện có thay vì tạo thao tác thứ hai. `command_id` định danh bản ghi lệnh trong REST/DB; `request_id` là khóa idempotency phía client; Edge tạo một `message_id` riêng cho mỗi binary envelope được phát đi. Backend phải lưu mapping giữa cả ba trong command record. Các giá trị này không phải khóa HMAC hay sequence.

**202 response**
```json
{
  "command_id": "123e4567-e89b-12d3-a456-426614174000",
  "device_id": "SX-001",
  "status": "QUEUED",
  "created_at": "2026-10-17T03:00:00Z"
}
```

**Command status enum:** `QUEUED`, `PUBLISHED`, `ACKNOWLEDGED`, `COMPLETED`, `REJECTED`, `TIMEOUT`, `INDETERMINATE`. Do not show `COMPLETED` until a valid device ACK confirms it. A network timeout is not evidence that the physical action failed; UI must show `TIMEOUT/INDETERMINATE` distinctly and must not automatically resend as a new command.

**Error response**
```json
{
  "error": {
    "code": "ROLE_FORBIDDEN",
    "message": "This role cannot execute the requested operation.",
    "request_id": "..."
  }
}
```

Use HTTP: 200 success query/update, 201 created, 202 queued command/test, 400 malformed request, 401 missing/expired session, 403 role forbidden, 404 unknown resource, 409 version/idempotency conflict, 422 schema/value invalid, 429 rate limited, 503 dependency unavailable. Do not return secret, HMAC key or internal exception trace to the client.

### 3.14. Authentication and RBAC

- Password hashing: Argon2id using a maintained library; no plaintext password storage.
- Access token: 32 random bytes encoded Base64URL; store only hash of token in SQLite. Session idle timeout 15 minutes; absolute lifetime 8 hours; logout revokes token server-side.
- Admin bootstrap must be created by a one-time CLI/setup process with no default public password; disable or rotate setup mechanism after provisioning.
- Login endpoints rate-limited; record success/failure without logging passwords/tokens.
- In production/test LAN, GUI calls API over HTTPS; PC trusts configured lab CA. No plaintext password/token on an untrusted network.

| Capability | Administrator | Operator | Viewer/Auditor |
|---|---|---|---|
| Dashboard/device status | Yes | Yes | Yes |
| Execute allowlisted operations | Yes | Yes, within policy | No |
| Manage users/roles | Yes | No | No |
| Change/publish policy | Yes | No | No |
| Device enrollment/key management | Restricted Admin workflow | No | No |
| View event timeline | Full | Operational subset | Read-only audit subset |
| Export reports | Yes | Limited test/output reports | Audit CSV allowed per configuration |
| Run approved security tests | Yes | Yes, only `test_only` device | View results only |
| Change system config | Yes | No | No |

The GUI may hide unavailable actions for usability, but backend MUST enforce RBAC. STM32 cannot know the human role from HMAC alone; it enforces device-side operation/policy bounds. Backend role check, server authorization and MCU policy enforcement are separate controls.

### 3.15. Error/result code registry

| Code | Name | Layer / behavior |
|---:|---|---|
| `0x0000` | `SUCCESS` | operation finished per ACK |
| `0x1001` | `FRAME_INVALID` | UART frame rejected |
| `0x1002` | `LENGTH_INVALID` | length mismatch or above max |
| `0x1003` | `CRC_INVALID` | outer UART CRC failure |
| `0x1004` | `VERSION_UNSUPPORTED` | protocol version not supported |
| `0x1005` | `DEVICE_ID_MISMATCH` | target ID mismatch |
| `0x1006` | `HMAC_INVALID` | authentication failed; no actuator change |
| `0x1007` | `REPLAY_OR_STALE_SEQUENCE` | sequence ≤ high-water mark |
| `0x1008` | `RATE_LIMITED` | token bucket/rate limit exceeded |
| `0x1009` | `COMMAND_NOT_ALLOWED` | policy/allowlist denied |
| `0x100A` | `PARAMETER_OUT_OF_RANGE` | schema or safe range denied |
| `0x100B` | `POLICY_VERSION_INVALID` | bad/rollback policy version |
| `0x100C` | `PERSISTENCE_FAILED` | unable to commit required state; safe mode |
| `0x100D` | `SAFE_MODE_ACTIVE` | command disallowed in safe mode |
| `0x100E` | `UNKNOWN_COMMAND` | operation code unsupported |
| `0x100F` | `PAYLOAD_INVALID` | bad command-specific payload |
| `0x1010` | `KEY_ID_UNKNOWN` | key identifier not provisioned |
| `0x1011` | `LINK_OFFLINE` | device is offline/safe |
| `0x1012` | `DEVICE_BUSY` | transient busy; query status; don't blindly replay |
| `0x1013` | `EXECUTION_FAILED` | validated command could not execute |
| `0x1014` | `ACK_TIMEOUT` | Edge did not receive result in time; physical result may be unknown |
| `0x1015` | `ROLE_FORBIDDEN` | backend denied user role |
| `0x1016` | `SESSION_EXPIRED` | login again |
| `0x1017` | `DEVICE_NOT_FOUND` | unknown/unregistered device |
| `0x1018` | `IDEMPOTENCY_CONFLICT` | request ID reused with different body |
| `0x1019` | `STORAGE_UNAVAILABLE` | database/persistent state unavailable |
| `0x101A` | `POLICY_PENDING` | device has not yet confirmed desired policy |

For a bad HMAC, MCU must not trust the inner command fields for an ACK. It may create a local security event and emit a separately signed `AUTH_FAILURE` event if resources/storage allow; otherwise update bounded counters and continue safely. Error telemetry must not create an unbounded response loop.

### 3.16. Modbus RTU/RS-485 contract — extension after P0

- Baseline extension configuration: 9600 baud, 8E1, slave ID 1; confirm hardware/transceiver electrical requirements before wiring.
- Only function code `0x03 Read Holding Registers` and `0x06 Write Single Register` are in allowlist.
- Register map: `0x0000 OUTPUT_STATE` R/W values 0..1; `0x0001 OUTPUT_LEVEL_PERMILLE` R/W values 0..1000; `0x0002 DEVICE_STATUS` read-only.
- Writes to all other registers, unsupported function codes, wrong slave ID, malformed length and invalid values are rejected at STM32 before RS-485 forwarding.
- Modbus CRC-16 is required by RTU; it detects serial-frame errors, not command authorization. Authorization remains at STM32.
- Tests use simulated or low-voltage slave only. Do not connect to unknown industrial equipment.

### 3.17. Protocol test vector

Use only as public interoperability test material, never as a deployment key. It covers `SET_OUTPUT(output_id=1,state=1)`, device `SX-001`, key ID 1, sequence 1 and UUID `123e4567-e89b-12d3-a456-426614174000`.

- Test key (32 bytes): `000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f`
- Envelope header + payload (hex):
```text
5358010153582d30303100000000000000000000010000000000000001123e4567e89b12d3a456426614174000000100020101
```
- Expected HMAC-SHA-256 tag (32 bytes):
```text
96a09046cc1f69b877c97df4ea4b58bab89eae59560a4b92fffd374f91e570ea
```
- Full envelope (hex):
```text
5358010153582d30303100000000000000000000010000000000000001123e4567e89b12d3a45642661417400000010002010196a09046cc1f69b877c97df4ea4b58bab89eae59560a4b92fffd374f91e570ea
```
- Expected: envelope length 83 bytes; `payload_len=2`; HMAC verification passes; command schema validates; first delivery may proceed if policy permits; byte mutation without recomputing tag must fail HMAC.
- Host script must recompute this vector independently with the selected crypto library; C firmware unit test, Python test and protocol validator must match it byte-for-byte.

