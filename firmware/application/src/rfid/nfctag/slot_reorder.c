#include "slot_reorder.h"

#include <string.h>

uint8_t tag_slot_config_storage_slot(const tag_slot_config_t *config, uint8_t logical_slot) {
    if (config == NULL || logical_slot >= TAG_MAX_SLOT_NUM) {
        return logical_slot;
    }
    return config->slots[logical_slot].storage_slot;
}

void tag_slot_config_initialize_storage_map(tag_slot_config_t *config) {
    if (config == NULL) {
        return;
    }
    for (uint8_t slot = 0; slot < TAG_MAX_SLOT_NUM; slot++) {
        config->slots[slot].storage_slot = slot;
    }
}

void tag_slot_config_migrate_v8_to_current(tag_slot_config_t *config) {
    if (config == NULL) {
        return;
    }
    tag_slot_config_initialize_storage_map(config);
    config->version = TAG_SLOT_CONFIG_CURRENT_VERSION;
}

bool tag_slot_config_swap_transaction(
    tag_slot_config_t *live_config,
    uint8_t source,
    uint8_t target,
    tag_slot_config_commit_t commit,
    void *context
) {
    if (live_config == NULL || commit == NULL ||
            source >= TAG_MAX_SLOT_NUM || target >= TAG_MAX_SLOT_NUM) {
        return false;
    }
    if (source == target) {
        return true;
    }

    tag_slot_config_t candidate = *live_config;
    uint8_t slot_buffer[sizeof(candidate.slots[0])];
    memcpy(slot_buffer, &candidate.slots[source], sizeof(slot_buffer));
    memcpy(&candidate.slots[source], &candidate.slots[target], sizeof(slot_buffer));
    memcpy(&candidate.slots[target], slot_buffer, sizeof(slot_buffer));

    if (candidate.active_slot == source) {
        candidate.active_slot = target;
    } else if (candidate.active_slot == target) {
        candidate.active_slot = source;
    }

    if (!commit(&candidate, context)) {
        return false;
    }

    *live_config = candidate;
    return true;
}
