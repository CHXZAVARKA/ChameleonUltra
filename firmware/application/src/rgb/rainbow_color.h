#ifndef RAINBOW_COLOR_H
#define RAINBOW_COLOR_H

#include <stdint.h>

#define RAINBOW_PHASE_SEGMENT 256U
#define RAINBOW_PHASE_CYCLE (RAINBOW_PHASE_SEGMENT * 6U)
#define BOOT_TRAIL_LENGTH 4U
#define RAINBOW_DISPLAY_BRIGHTNESS 255U
#define RAINBOW_PWM_BASE_CLOCK_HZ 16000000UL
#define RAINBOW_PWM_COUNTER_TOP 1000U
#define RAINBOW_PWM_TICKS_PER_MS \
    (RAINBOW_PWM_BASE_CLOCK_HZ / (RAINBOW_PWM_COUNTER_TOP * 1000UL))
#define CHARGING_PARTICLE_FRAME_MS 120U

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rainbow_rgb_t;

rainbow_rgb_t rainbow_color_at(uint16_t phase, uint8_t brightness);
uint16_t rainbow_pwm_compare(uint8_t intensity, uint16_t pwm_top);
uint32_t rainbow_pwm_repeats(uint16_t frame_ms);
uint16_t stock_trail_pwm_compare(uint8_t age);
uint8_t boot_trail_frame_count(uint8_t start, uint8_t position_count);
uint8_t boot_trail_position(uint8_t frame, uint8_t start, uint8_t position_count);
int8_t boot_trail_age(uint8_t frame, uint8_t position, uint8_t start, uint8_t position_count);
uint8_t stock_sweep_frame_count(uint8_t end);
int8_t stock_sweep_position(uint8_t frame, uint8_t channel, uint8_t dir, uint8_t end);
int8_t stock_sweep_channel(uint8_t frame, uint8_t position, uint8_t dir, uint8_t end);
uint8_t stock_full_startup_frame_count(uint8_t slot, uint8_t position_count);
int8_t stock_full_startup_channel(
    uint8_t frame,
    uint8_t position,
    uint8_t slot,
    uint8_t position_count
);
uint8_t stock_linear_frame_count(uint8_t start, uint8_t stop);
uint8_t stock_linear_position(uint8_t frame, uint8_t start, uint8_t stop);
uint8_t stock_fade_frame_count(uint8_t end);
int8_t stock_fade_position(uint8_t frame, uint8_t channel, uint8_t dir, uint8_t end);
int8_t stock_fade_channel(uint8_t frame, uint8_t position, uint8_t dir, uint8_t end);
uint8_t stock_fade_level(
    uint8_t frame,
    uint8_t channel,
    uint8_t end,
    uint8_t start_light,
    uint8_t stop_light
);
uint8_t charging_filled_count(uint8_t percentage, uint8_t position_count);
uint8_t charging_particle_cycle_frames(uint8_t percentage, uint8_t position_count);
int8_t charging_particle_position(uint32_t frame, uint8_t percentage, uint8_t position_count);
int8_t charging_particle_age(
    uint32_t frame,
    uint8_t position,
    uint8_t percentage,
    uint8_t position_count
);
#endif
