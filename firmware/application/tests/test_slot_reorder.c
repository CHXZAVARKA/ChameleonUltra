#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "data_cmd.h"
#include "app_status.h"
#include "slot_reorder.h"
#include "slot_reorder_protocol.h"
#include "tag_persistence.h"

typedef struct {
    bool result;
    int calls;
    tag_slot_config_t saved;
} commit_spy_t;

typedef struct {
    bool result;
    int calls;
    uint8_t source;
    uint8_t target;
} swap_spy_t;

static tag_slot_config_t persistence_config;

uint8_t tag_emulation_get_storage_slot(uint8_t slot) {
    return tag_slot_config_storage_slot(&persistence_config, slot);
}

static bool commit_spy(const tag_slot_config_t *config, void *context) {
    commit_spy_t *spy = context;
    spy->calls++;
    spy->saved = *config;
    return spy->result;
}

static bool swap_spy(uint8_t source, uint8_t target, void *context) {
    swap_spy_t *spy = context;
    spy->calls++;
    spy->source = source;
    spy->target = target;
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

static void assert_record_map(uint8_t logical_slot, tag_sense_type_t sense_type,
                              uint16_t dump_id, uint16_t nickname_id) {
    fds_slot_record_map_t dump_map;
    fds_slot_record_map_t nickname_map;
    get_fds_map_by_slot_sense_type_for_dump(logical_slot, sense_type, &dump_map);
    get_fds_map_by_slot_sense_type_for_nick(logical_slot, sense_type, &nickname_map);
    assert(dump_map.id == dump_id);
    assert(nickname_map.id == nickname_id);
    assert(dump_map.key == (uint16_t)sense_type);
    assert(nickname_map.key == (uint16_t)sense_type);
}

static void test_real_persistence_mapping_moves_whole_hf_lf_bundle(void) {
    persistence_config = make_config(1);
    persistence_config.slots[1].tag_hf = TAG_TYPE_SEOS;
    persistence_config.slots[1].tag_lf = TAG_TYPE_IDTECK;
    persistence_config.slots[6].tag_hf = TAG_TYPE_NTAG_216;
    persistence_config.slots[6].tag_lf = TAG_TYPE_EM410X;
    commit_spy_t spy = {.result = true};

    assert_record_map(1, TAG_SENSE_HF, 0x1101, 0x1201);
    assert_record_map(1, TAG_SENSE_LF, 0x1101, 0x1201);
    assert_record_map(6, TAG_SENSE_HF, 0x1106, 0x1206);
    assert_record_map(6, TAG_SENSE_LF, 0x1106, 0x1206);

    assert(tag_slot_config_swap_transaction(&persistence_config, 1, 6, commit_spy, &spy));

    assert_record_map(1, TAG_SENSE_HF, 0x1106, 0x1206);
    assert_record_map(1, TAG_SENSE_LF, 0x1106, 0x1206);
    assert_record_map(6, TAG_SENSE_HF, 0x1101, 0x1201);
    assert_record_map(6, TAG_SENSE_LF, 0x1101, 0x1201);
    assert(persistence_config.slots[1].tag_hf == TAG_TYPE_NTAG_216);
    assert(persistence_config.slots[1].tag_lf == TAG_TYPE_EM410X);
    assert(persistence_config.slots[6].tag_hf == TAG_TYPE_SEOS);
    assert(persistence_config.slots[6].tag_lf == TAG_TYPE_IDTECK);
}

static void test_protocol_command_1041_success_and_callback(void) {
    uint8_t payload[] = {0, TAG_MAX_SLOT_NUM - 1};
    swap_spy_t spy = {.result = true};

    tag_slot_swap_protocol_response_t response = tag_slot_swap_protocol_process(
        DATA_CMD_SWAP_SLOTS, sizeof(payload), payload, swap_spy, &spy);

    assert(response.command == 1041);
    assert(response.status == STATUS_SUCCESS);
    assert(spy.calls == 1);
    assert(spy.source == 0);
    assert(spy.target == TAG_MAX_SLOT_NUM - 1);
}

static void test_protocol_rejects_invalid_lengths_and_indices(void) {
    uint8_t payload[] = {1, 6, 7};
    swap_spy_t spy = {.result = true};
    static const uint16_t invalid_lengths[] = {0, 1, 3};

    for (size_t i = 0; i < sizeof(invalid_lengths) / sizeof(invalid_lengths[0]); i++) {
        tag_slot_swap_protocol_response_t response = tag_slot_swap_protocol_process(
            DATA_CMD_SWAP_SLOTS, invalid_lengths[i], payload, swap_spy, &spy);
        assert(response.status == STATUS_PAR_ERR);
    }

    payload[0] = TAG_MAX_SLOT_NUM;
    assert(tag_slot_swap_protocol_process(DATA_CMD_SWAP_SLOTS, 2, payload, swap_spy, &spy).status == STATUS_PAR_ERR);
    payload[0] = 1;
    payload[1] = TAG_MAX_SLOT_NUM;
    assert(tag_slot_swap_protocol_process(DATA_CMD_SWAP_SLOTS, 2, payload, swap_spy, &spy).status == STATUS_PAR_ERR);
    assert(tag_slot_swap_protocol_process(DATA_CMD_SWAP_SLOTS, 2, NULL, swap_spy, &spy).status == STATUS_PAR_ERR);
    assert(spy.calls == 0);
}

static void test_protocol_reports_flash_failure(void) {
    uint8_t payload[] = {2, 5};
    swap_spy_t spy = {.result = false};

    tag_slot_swap_protocol_response_t response = tag_slot_swap_protocol_process(
        DATA_CMD_SWAP_SLOTS, sizeof(payload), payload, swap_spy, &spy);

    assert(response.status == STATUS_FLASH_WRITE_FAIL);
    assert(spy.calls == 1);
    assert(spy.source == 2);
    assert(spy.target == 5);
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
    test_real_persistence_mapping_moves_whole_hf_lf_bundle();
    test_protocol_command_1041_success_and_callback();
    test_protocol_rejects_invalid_lengths_and_indices();
    test_protocol_reports_flash_failure();
    return 0;
}
