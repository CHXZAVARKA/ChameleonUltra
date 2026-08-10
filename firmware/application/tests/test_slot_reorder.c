#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "data_cmd.h"
#include "slot_reorder.h"

typedef struct {
    bool result;
    int calls;
    tag_slot_config_t saved;
} commit_spy_t;

static bool commit_spy(const tag_slot_config_t *config, void *context) {
    commit_spy_t *spy = context;
    spy->calls++;
    spy->saved = *config;
    return spy->result;
}

static tag_slot_config_t make_config(uint8_t active_slot) {
    tag_slot_config_t config = {
        .version = TAG_SLOT_CONFIG_CURRENT_VERSION,
        .active_slot = active_slot,
        .storage_slots = {0, 1, 2, 3, 4, 5, 6, 7},
    };
    for (uint8_t i = 0; i < TAG_MAX_SLOT_NUM; i++) {
        config.slots[i].enabled_hf = (i & 1u) != 0;
        config.slots[i].enabled_lf = (i & 2u) != 0;
        config.slots[i].tag_hf = (tag_specific_type_t)(TAG_TYPE_MIFARE_1024 + i);
        config.slots[i].tag_lf = (tag_specific_type_t)(TAG_TYPE_EM410X + i);
    }
    return config;
}

static void assert_slot_equal(const tag_slot_config_t *left, uint8_t left_slot,
                              const tag_slot_config_t *right, uint8_t right_slot) {
    assert(memcmp(&left->slots[left_slot], &right->slots[right_slot],
                  sizeof(left->slots[0])) == 0);
    assert(left->storage_slots[left_slot] == right->storage_slots[right_slot]);
}

static void test_complete_bundle_and_source_active_follow_swap(void) {
    tag_slot_config_t before = make_config(1);
    tag_slot_config_t live = before;
    commit_spy_t spy = {.result = true};

    assert(tag_slot_config_swap_transaction(&live, 1, 6, commit_spy, &spy));
    assert(spy.calls == 1);
    assert_slot_equal(&live, 1, &before, 6);
    assert_slot_equal(&live, 6, &before, 1);
    assert(live.active_slot == 6);
    assert(memcmp(&live, &spy.saved, sizeof(live)) == 0);
}

static void test_every_supported_tag_family_uses_the_same_bundle_mapping(void) {
    static const tag_specific_type_t hf_types[] = {TAG_SPECIFIC_TYPE_HF_VALUES};
    static const tag_specific_type_t lf_types[] = {TAG_SPECIFIC_TYPE_LF_VALUES};

    for (size_t hf = 0; hf < sizeof(hf_types) / sizeof(hf_types[0]); hf++) {
        for (size_t lf = 0; lf < sizeof(lf_types) / sizeof(lf_types[0]); lf++) {
            tag_slot_config_t live = make_config(0);
            live.slots[0].tag_hf = hf_types[hf];
            live.slots[0].tag_lf = lf_types[lf];
            commit_spy_t spy = {.result = true};

            assert(tag_slot_config_swap_transaction(&live, 0, 7, commit_spy, &spy));
            assert(live.slots[7].tag_hf == hf_types[hf]);
            assert(live.slots[7].tag_lf == lf_types[lf]);
            assert(tag_slot_config_storage_slot(&live, 7) == 0);
        }
    }
}

static void test_v8_migration_initializes_identity_storage_map(void) {
    tag_slot_config_t config = {0};
    memset(config.storage_slots, 0xff, sizeof(config.storage_slots));

    tag_slot_config_initialize_storage_map(&config);

    for (uint8_t slot = 0; slot < TAG_MAX_SLOT_NUM; slot++) {
        assert(tag_slot_config_storage_slot(&config, slot) == slot);
    }
}

static void test_target_active_follows_swap(void) {
    tag_slot_config_t live = make_config(6);
    commit_spy_t spy = {.result = true};

    assert(tag_slot_config_swap_transaction(&live, 1, 6, commit_spy, &spy));
    assert(live.active_slot == 1);
}

static void test_unrelated_active_slot_is_unchanged(void) {
    tag_slot_config_t live = make_config(3);
    commit_spy_t spy = {.result = true};

    assert(tag_slot_config_swap_transaction(&live, 1, 6, commit_spy, &spy));
    assert(live.active_slot == 3);
}

static void test_same_slot_is_noop_without_persistence(void) {
    tag_slot_config_t before = make_config(2);
    tag_slot_config_t live = before;
    commit_spy_t spy = {.result = false};

    assert(tag_slot_config_swap_transaction(&live, 2, 2, commit_spy, &spy));
    assert(spy.calls == 0);
    assert(memcmp(&live, &before, sizeof(live)) == 0);
}

static void test_invalid_slots_change_nothing(void) {
    tag_slot_config_t before = make_config(2);
    tag_slot_config_t live = before;
    commit_spy_t spy = {.result = true};

    assert(!tag_slot_config_swap_transaction(&live, TAG_MAX_SLOT_NUM, 2, commit_spy, &spy));
    assert(!tag_slot_config_swap_transaction(&live, 2, TAG_MAX_SLOT_NUM, commit_spy, &spy));
    assert(spy.calls == 0);
    assert(memcmp(&live, &before, sizeof(live)) == 0);
}

static void test_persistence_failure_rolls_back_runtime_and_active_slot(void) {
    tag_slot_config_t before = make_config(1);
    tag_slot_config_t live = before;
    commit_spy_t spy = {.result = false};

    assert(!tag_slot_config_swap_transaction(&live, 1, 6, commit_spy, &spy));
    assert(spy.calls == 1);
    assert(spy.saved.active_slot == 6);
    assert(memcmp(&live, &before, sizeof(live)) == 0);
}

int main(void) {
    _Static_assert(DATA_CMD_SWAP_SLOTS == 1041, "slot swap protocol command changed");
    test_complete_bundle_and_source_active_follow_swap();
    test_every_supported_tag_family_uses_the_same_bundle_mapping();
    test_v8_migration_initializes_identity_storage_map();
    test_target_active_follows_swap();
    test_unrelated_active_slot_is_unchanged();
    test_same_slot_is_noop_without_persistence();
    test_invalid_slots_change_nothing();
    test_persistence_failure_rolls_back_runtime_and_active_slot();
    return 0;
}
