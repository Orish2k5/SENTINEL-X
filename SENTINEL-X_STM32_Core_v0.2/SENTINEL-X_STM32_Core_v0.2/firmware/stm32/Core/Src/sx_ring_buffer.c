#include "sx_ring_buffer.h"

#include <string.h>

#define SX_RING_MASK (SX_UART_RX_RING_CAPACITY - 1u)

#if ((SX_UART_RX_RING_CAPACITY & (SX_UART_RX_RING_CAPACITY - 1u)) != 0u)
#error "SX_UART_RX_RING_CAPACITY must be a power of two"
#endif

void sx_byte_ring_init(sx_byte_ring_t *ring)
{
    if (ring != NULL) {
        (void)memset(ring, 0, sizeof(*ring));
    }
}

bool sx_byte_ring_push_isr(sx_byte_ring_t *ring, uint8_t byte)
{
    uint16_t next;

    if (ring == NULL) {
        return false;
    }
    next = (uint16_t)((ring->head + 1u) & SX_RING_MASK);
    if (next == ring->tail) {
        ring->overflow = true;
        return false;
    }
    ring->bytes[ring->head] = byte;
    /* Publish the byte before publishing the new head index. */
#if defined(__GNUC__)
    __atomic_thread_fence(__ATOMIC_RELEASE);
#endif
    ring->head = next;
    return true;
}

bool sx_byte_ring_pop(sx_byte_ring_t *ring, uint8_t *byte)
{
    if ((ring == NULL) || (byte == NULL) || (ring->tail == ring->head)) {
        return false;
    }
#if defined(__GNUC__)
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif
    *byte = ring->bytes[ring->tail];
#if defined(__GNUC__)
    __atomic_thread_fence(__ATOMIC_RELEASE);
#endif
    ring->tail = (uint16_t)((ring->tail + 1u) & SX_RING_MASK);
    return true;
}

bool sx_byte_ring_take_overflow(sx_byte_ring_t *ring)
{
    bool overflow;
    if (ring == NULL) {
        return false;
    }
    overflow = ring->overflow;
    ring->overflow = false;
    return overflow;
}
