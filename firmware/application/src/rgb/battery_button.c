#include "battery_button.h"

battery_button_action_t battery_button_update(
    battery_button_state_t *state,
    bool pressed,
    bool released,
    bool long_press_configured,
    uint32_t held_ticks,
    uint32_t long_press_ticks
) {
    if (state->active && released) {
        state->active = false;
        return BATTERY_BUTTON_RESTORE;
    }
    if (!long_press_configured) {
        state->active = false;
        return BATTERY_BUTTON_IDLE;
    }
    if (pressed && held_ticks > long_press_ticks) {
        state->active = true;
        return BATTERY_BUTTON_SHOW;
    }
    return BATTERY_BUTTON_IDLE;
}

bool battery_button_input_enabled(bool short_action_enabled, bool long_action_enabled) {
    return short_action_enabled || long_action_enabled;
}
