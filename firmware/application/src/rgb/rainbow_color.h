#ifndef RAINBOW_COLOR_H
#define RAINBOW_COLOR_H

#include <stdint.h>

#define RAINBOW_PHASE_SEGMENT 256U
#define RAINBOW_PHASE_CYCLE (RAINBOW_PHASE_SEGMENT * 6U)

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rainbow_rgb_t;

rainbow_rgb_t rainbow_color_at(uint16_t phase, uint8_t brightness);
rainbow_rgb_t battery_level_color(uint8_t percentage, uint8_t brightness);
uint8_t led_bounce_position(uint32_t step, uint8_t position_count);
uint16_t rainbow_pwm_compare(uint8_t intensity, uint16_t pwm_top);

#endif
