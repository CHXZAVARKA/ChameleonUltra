#include <assert.h>

#include "slot_led_policy.h"

int main(void) {
    assert(slot_led_state(false, false) == SLOT_LED_STATE_EMPTY);
    assert(slot_led_state(true, false) == SLOT_LED_STATE_HF);
    assert(slot_led_state(false, true) == SLOT_LED_STATE_LF);
    assert(slot_led_state(true, true) == SLOT_LED_STATE_DUAL);

    assert(slot_cycle_adjacent(0, false, 8) == 1);
    assert(slot_cycle_adjacent(7, false, 8) == 0);
    assert(slot_cycle_adjacent(0, true, 8) == 7);
    assert(slot_cycle_adjacent(4, true, 8) == 3);
    assert(slot_cycle_adjacent(0, false, 1) == 0);
    return 0;
}
