#ifndef SLOT_LED_POLICY_H
#define SLOT_LED_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SLOT_LED_STATE_EMPTY,
    SLOT_LED_STATE_HF,
    SLOT_LED_STATE_LF,
    SLOT_LED_STATE_DUAL,
} slot_led_state_t;

static inline slot_led_state_t slot_led_state(bool hf_usable, bool lf_usable) {
    if (hf_usable && lf_usable) return SLOT_LED_STATE_DUAL;
    if (hf_usable) return SLOT_LED_STATE_HF;
    if (lf_usable) return SLOT_LED_STATE_LF;
    return SLOT_LED_STATE_EMPTY;
}

static inline uint8_t slot_cycle_adjacent(
    uint8_t current,
    bool previous,
    uint8_t slot_count
) {
    if (slot_count <= 1) return current;
    if (previous) return current == 0 ? slot_count - 1 : current - 1;
    return current + 1 == slot_count ? 0 : current + 1;
}

#endif
