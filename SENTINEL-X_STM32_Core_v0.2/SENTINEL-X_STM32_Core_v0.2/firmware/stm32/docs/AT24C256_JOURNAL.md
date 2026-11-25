# AT24C256 persistent journal — STM32 Core v0.2

## 1. Scope

This module provides a fail-closed persistent store using an AT24C256-compatible 256-Kbit I2C EEPROM. It replaces the v0.1 FRAM placeholder for the project's current bill of materials.

The driver assumes:

- capacity: 32,768 bytes;
- page size: 64 bytes;
- 16-bit EEPROM word address;
- I2C 7-bit address in `0x50..0x57`;
- default address `0x50` only when A2/A1/A0 are all low;
- write-cycle ACK polling after every page write.

Check the actual IC marking and module jumpers. The shopping-list entry only says AT24C256 and does not identify the exact vendor suffix or breakout schematic.

## 2. Memory map

| Region | Start | Size | Purpose |
|---|---:|---:|---|
| Inbound sequence journal | `0x0000` | `0x3F00` (16,128 B) | Edge → STM32 high-water sequence |
| Outbound sequence journal | `0x3F00` | `0x3F00` (16,128 B) | STM32 → Edge high-water sequence, reserved for ACK/EVENT/STATUS work |
| Policy slot A | `0x7E00` | `0x0080` (128 B) | Previous or active PolicyV1 record |
| Policy slot B | `0x7E80` | `0x0080` (128 B) | Alternate PolicyV1 record |
| Reserved | `0x7F00` | `0x0100` (256 B) | Reserved; do not allocate without a map revision |

Each sequence journal has 504 records of 32 bytes. Records rotate through the entire region to distribute writes. The application currently binds the inbound journal to `sx_replay_guard`; the outbound journal API is available for the later MCU→Edge envelope sequence implementation.

## 3. Sequence record and commit algorithm

32-byte record layout:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | Magic `SXJR` |
| 4 | 1 | Record format version = 1 |
| 5 | 1 | Stream ID: 1 inbound, 2 outbound |
| 6 | 2 | Reserved = 0 |
| 8 | 4 | Journal generation, big-endian |
| 12 | 8 | Sequence high-water mark, big-endian |
| 20 | 4 | CRC-32/IEEE over bytes 0..19 |
| 24 | 4 | Bitwise complement of generation |
| 28 | 3 | Reserved = 0 |
| 31 | 1 | Commit marker: `0xA5` valid; `0x00` invalid/in-progress; erased `0xFF` blank |

For each append, the implementation:

1. Selects the slot after the current newest slot (circularly).
2. Writes marker `0x00` to invalidate the old contents of that destination slot.
3. Writes bytes 0..30 without crossing a 64-byte EEPROM page.
4. Reads bytes 0..30 back and compares them byte-for-byte.
5. Writes marker `0xA5` separately as the final commit action.
6. Reads the marker back and checks it before updating RAM state.

The previously newest record is not overwritten during the transaction. An interrupted write with marker `0x00` is ignored when another valid record exists. A committed marker with an invalid record/CRC causes load failure and SAFE_MODE; the code does not fall back to an older sequence on a committed-record corruption. Any ambiguous EEPROM I/O failure also latches the in-memory persistence object as faulted: subsequent reads/commits are refused until the object is reinitialized and both journals are rescanned. This covers the case where the EEPROM may have committed the final marker but the MCU did not observe the ACK/readback.

The loader scans the full log in 256-byte reads, validates records, chooses the newest generation with serial-number arithmetic, and checks that sequence values are monotonic with generation. Reinitialization after a latched I/O fault is a recovery/rescan step, not an automatic permission to actuate; application state, key availability and active policy must be revalidated before commands are accepted.

## 4. Factory provisioning

A blank EEPROM has no trusted sequence value. The normal boot path must not silently interpret a blank/corrupt journal as sequence zero.

` sx_persistence_at24c256_provision_blank()` is an explicit factory-commissioning API. It only succeeds when both sequence journals and both policy slots are erased. It writes the initial inbound and outbound sequence records. Do not call it unconditionally from `main()` or expose it to unauthenticated UART/MQTT commands.

A production-quality commissioning workflow still needs an authorized physical/service procedure and recovery documentation. If power fails between creation of the inbound and outbound initial records, this API intentionally refuses to auto-format on the next boot; manual controlled recovery is required.

## 5. Policy slots

Policy is stored in two alternating 128-byte records. The record contains `SXPL`, format version, payload length, journal generation, canonical PolicyV1 bytes, CRC-32/IEEE and a final commit marker at offset 127.

The inactive slot is invalidated, written, read back and committed last. The active policy is changed in RAM only after persistent commit succeeds. A missing policy means actuation remains denied; the application may still allow authenticated GET_STATUS when the sequence journal is healthy.

## 6. Endurance and operational limitation

The Microchip AT24C256C datasheet specifies 64-byte page writes, a self-timed write cycle up to 5 ms and 1,000,000 write cycles. This project uses a circular journal with 504 slots per sequence direction.

Each sequence-slot reuse writes the commit-marker byte twice (invalidate then commit). A rough conservative estimate for that marker cell is:

`lifetime_seconds ≈ (1,000,000 / 2) × (504 / authenticated_sequence_commits_per_second)`

At a sustained 5 commits/s this is about 1.6 years; at 20 commits/s about 0.4 years. These are theoretical estimates, not a lifetime guarantee. Temperature, exact chip/module variant, write-cycle interpretation and actual command distribution matter. Authenticated schema-valid commands consume sequence even when later denied by policy/rate limit, so a compromised Edge key could also accelerate EEPROM wear.

AT24C256 is acceptable for the low-rate academic prototype with controlled testing and wear monitoring, but FRAM remains the preferred option for high command rates or long-lived continuous operation. Do not claim industrial endurance from this implementation.

## 7. HAL integration

- I2C1 PB8=SCL and PB9=SDA are the current proposed pin assignment.
- Start at 100 kHz Standard Mode.
- Pass the 7-bit address (default `0x50`) to `SX_Platform_AT24C256_Init()`; the HAL adapter shifts it left for STM32 HAL.
- The adapter uses 16-bit memory addressing and ACK polling after writes.
- HAL I2C reads/writes run in foreground context only, never from UART ISR.
- Check that module pull-ups connect to 3.3 V and that the actual breakout does not pull SDA/SCL up to 5 V.

## 8. Current limitations

- No hardware power-cut or brown-out test has been performed.
- No EEPROM module-specific address scan is performed; address is configured explicitly.
- The current application uses the inbound sequence journal. Outbound journaling is exposed but ACK/EVENT/STATUS generation is not yet implemented.
- The device-key provider and rate limiter deliberately deny all operations until separately implemented.

## 9. Datasheet reference

The implementation constants match the AT24C256C family assumptions in Microchip's datasheet. The BOM only names “AT24C256”, so check the exact IC top marking and breakout supplier datasheet before relying on the same endurance/timing/voltage values.

- Microchip, AT24C128C/AT24C256C datasheet: https://ww1.microchip.com/downloads/aemDocuments/documents/MPD/ProductDocuments/DataSheets/AT24C128C-AT24C256C-Data-Sheet-DS20006270.pdf
- Microchip online documentation, page-write constraints: https://onlinedocs.microchip.com/oxy/GUID-FEEC2B39-7B05-43C4-AB92-C76A6F28FB70-en-US-2/GUID-A5A8BD8E-49CA-44EE-8192-4BD4D6CAF6E4.html
