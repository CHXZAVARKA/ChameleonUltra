#ifndef SLOT_REORDER_H
#define SLOT_REORDER_H

#include <stdbool.h>
#include <stdint.h>

#include "tag_emulation.h"

typedef bool (*tag_slot_config_commit_t)(const tag_slot_config_t *config, void *context);

uint8_t tag_slot_config_storage_slot(const tag_slot_config_t *config, uint8_t logical_slot);
void tag_slot_config_initialize_storage_map(tag_slot_config_t *config);

/**
 * Atomically publish a whole-slot swap through the supplied commit function.
 *
 * The live configuration is not changed unless commit returns true. A source
 * equal to target is a successful no-op and does not call commit.
 */
bool tag_slot_config_swap_transaction(
    tag_slot_config_t *live_config,
    uint8_t source,
    uint8_t target,
    tag_slot_config_commit_t commit,
    void *context
);

#endif
