#include <assert.h>

#include "battery_button.h"

int main(void) {
    battery_button_state_t state = {false};

    assert(battery_button_input_enabled(false, true));
    assert(battery_button_input_enabled(true, false));
    assert(!battery_button_input_enabled(false, false));

    assert(battery_button_update(&state, true, false, true, 1000U, 1000U) == BATTERY_BUTTON_IDLE);
    assert(!state.active);
    assert(battery_button_update(&state, true, false, true, 1001U, 1000U) == BATTERY_BUTTON_SHOW);
    assert(state.active);
    assert(battery_button_update(&state, true, false, true, 1200U, 1000U) == BATTERY_BUTTON_SHOW);
    assert(state.active);
    assert(battery_button_update(&state, false, true, true, 1200U, 1000U) == BATTERY_BUTTON_RESTORE);
    assert(!state.active);

    assert(battery_button_update(&state, true, false, false, 1500U, 1000U) == BATTERY_BUTTON_IDLE);
    assert(!state.active);
    return 0;
}
