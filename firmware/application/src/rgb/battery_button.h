#ifndef BATTERY_BUTTON_H
#define BATTERY_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BATTERY_BUTTON_IDLE,
    BATTERY_BUTTON_SHOW,
    BATTERY_BUTTON_RESTORE,
} battery_button_action_t;

typedef struct {
    bool active;
} battery_button_state_t;

battery_button_action_t battery_button_update(
    battery_button_state_t *state,
    bool pressed,
    bool released,
    bool long_press_configured,
    uint32_t held_ticks,
    uint32_t long_press_ticks
);
bool battery_button_input_enabled(bool short_action_enabled, bool long_action_enabled);

#endif
