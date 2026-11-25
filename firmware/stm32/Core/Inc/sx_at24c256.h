#ifndef SX_AT24C256_H
#define SX_AT24C256_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SX_AT24C256_CAPACITY_BYTES 32768u
#define SX_AT24C256_PAGE_SIZE 64u
#define SX_AT24C256_MAX_READ_CHUNK 256u

typedef bool (*sx_at24c256_read_fn)(void *context, uint16_t address,
                                    uint8_t *data, uint16_t length);
typedef bool (*sx_at24c256_write_fn)(void *context, uint16_t address,
                                     const uint8_t *data, uint16_t length);

typedef struct {
    void *context;
    sx_at24c256_read_fn read;
    sx_at24c256_write_fn write;
} sx_at24c256_io_t;

typedef struct {
    sx_at24c256_io_t io;
    bool initialized;
} sx_at24c256_t;

bool sx_at24c256_init(sx_at24c256_t *device, const sx_at24c256_io_t *io);
bool sx_at24c256_read(sx_at24c256_t *device, uint16_t address,
                      uint8_t *data, size_t length);
/* Writes are split so no backend write crosses a 64-byte page boundary. */
bool sx_at24c256_write(sx_at24c256_t *device, uint16_t address,
                       const uint8_t *data, size_t length);

#endif /* SX_AT24C256_H */
