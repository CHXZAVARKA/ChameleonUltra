#ifndef RAINBOW_COLOR_H
#define RAINBOW_COLOR_H

#include <stdint.h>

#define RAINBOW_PHASE_SEGMENT 256U
#define RAINBOW_PHASE_CYCLE (RAINBOW_PHASE_SEGMENT * 6U)
#define BOOT_TRAIL_LENGTH 4U

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rainbow_rgb_t;

rainbow_rgb_t rainbow_color_at(uint16_t phase, uint8_t brightness);
uint16_t rainbow_pwm_compare(uint8_t intensity, uint16_t pwm_top);
uint8_t boot_trail_frame_count(uint8_t start, uint8_t position_count);
uint8_t boot_trail_position(uint8_t frame, uint8_t start, uint8_t position_count);
uint8_t boot_trail_level(uint8_t frame, uint8_t position, uint8_t start, uint8_t position_count);
uint8_t charging_filled_count(uint8_t percentage, uint8_t position_count);
uint8_t charging_particle_cycle_frames(uint8_t percentage, uint8_t position_count);
int8_t charging_particle_position(uint8_t frame, uint8_t percentage, uint8_t position_count);
uint8_t charging_particle_level(
    uint8_t frame,
    uint8_t position,
    uint8_t percentage,
    uint8_t position_count
);

#endif
