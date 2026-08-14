#ifndef BATTERY_INDICATOR_H
#define BATTERY_INDICATOR_H

#include <stdint.h>

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} battery_indicator_rgb_t;

battery_indicator_rgb_t battery_indicator_color(uint8_t percentage);
uint8_t battery_indicator_lit_count(uint8_t percentage, uint8_t position_count);
uint8_t battery_indicator_hardware_index(
    uint8_t left_to_right_position,
    uint8_t position_count
);

#endif
