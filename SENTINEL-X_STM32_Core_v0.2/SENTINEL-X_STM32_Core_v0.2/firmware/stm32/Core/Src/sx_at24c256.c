#include "sx_at24c256.h"

#include <string.h>

static bool sx_range_valid(uint16_t address, size_t length)
{
    if (length > SX_AT24C256_CAPACITY_BYTES) {
        return false;
    }
    return ((uint32_t)address + (uint32_t)length) <= SX_AT24C256_CAPACITY_BYTES;
}

bool sx_at24c256_init(sx_at24c256_t *device, const sx_at24c256_io_t *io)
{
    if ((device == NULL) || (io == NULL) || (io->read == NULL) || (io->write == NULL)) {
        return false;
    }
    (void)memset(device, 0, sizeof(*device));
    device->io = *io;
    device->initialized = true;
    return true;
}

bool sx_at24c256_read(sx_at24c256_t *device, uint16_t address,
                      uint8_t *data, size_t length)
{
    size_t offset = 0u;

    if ((device == NULL) || !device->initialized ||
        ((data == NULL) && (length != 0u)) || !sx_range_valid(address, length)) {
        return false;
    }

    while (offset < length) {
        size_t remaining = length - offset;
        uint16_t chunk = (uint16_t)((remaining > SX_AT24C256_MAX_READ_CHUNK)
                                        ? SX_AT24C256_MAX_READ_CHUNK : remaining);
        if (!device->io.read(device->io.context,
                             (uint16_t)((uint32_t)address + (uint32_t)offset),
                             &data[offset], chunk)) {
            return false;
        }
        offset += chunk;
    }
    return true;
}

bool sx_at24c256_write(sx_at24c256_t *device, uint16_t address,
                       const uint8_t *data, size_t length)
{
    size_t offset = 0u;

    if ((device == NULL) || !device->initialized ||
        ((data == NULL) && (length != 0u)) || !sx_range_valid(address, length)) {
        return false;
    }

    while (offset < length) {
        uint32_t current = (uint32_t)address + (uint32_t)offset;
        uint16_t page_remaining = (uint16_t)(SX_AT24C256_PAGE_SIZE -
                                              (current % SX_AT24C256_PAGE_SIZE));
        size_t remaining = length - offset;
        uint16_t chunk = (uint16_t)((remaining > page_remaining) ? page_remaining : remaining);

        if (!device->io.write(device->io.context, (uint16_t)current,
                              &data[offset], chunk)) {
            return false;
        }
        offset += chunk;
    }
    return true;
}
