#ifndef SX_RING_BUFFER_H
#define SX_RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#define SX_UART_RX_RING_CAPACITY 512u

typedef struct {
    uint8_t bytes[SX_UART_RX_RING_CAPACITY];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile bool overflow;
} sx_byte_ring_t;

void sx_byte_ring_init(sx_byte_ring_t *ring);
bool sx_byte_ring_push_isr(sx_byte_ring_t *ring, uint8_t byte);
bool sx_byte_ring_pop(sx_byte_ring_t *ring, uint8_t *byte);
bool sx_byte_ring_take_overflow(sx_byte_ring_t *ring);

#endif /* SX_RING_BUFFER_H */
