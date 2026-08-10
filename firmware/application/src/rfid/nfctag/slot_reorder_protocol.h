#ifndef SLOT_REORDER_PROTOCOL_H
#define SLOT_REORDER_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

typedef bool (*tag_slot_swap_callback_t)(uint8_t source, uint8_t target, void *context);

typedef struct {
    uint16_t command;
    uint16_t status;
} tag_slot_swap_protocol_response_t;

/**
 * Validate and execute the whole-slot swap command without transport or
 * framebuffer dependencies. The callback is invoked exactly once for a valid
 * request and is responsible for the atomic configuration commit.
 */
tag_slot_swap_protocol_response_t tag_slot_swap_protocol_process(
    uint16_t command,
    uint16_t length,
    const uint8_t *data,
    tag_slot_swap_callback_t callback,
    void *context
);

#endif
