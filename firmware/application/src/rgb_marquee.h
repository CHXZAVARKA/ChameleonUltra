#ifndef RGB_MARQUEE_H
#define RGB_MARQUEE_H

#include <stdint.h>
#include "nrf_drv_pwm.h"

typedef enum {
    RGB_MARQUEE_RF_SOURCE_HF = 1U << 0,
    RGB_MARQUEE_RF_SOURCE_LF = 1U << 1,
} rgb_marquee_rf_source_t;

void rgb_marquee_init(void);
void rgb_marquee_stop(void);
void rgb_marquee_reset(void);
bool rgb_marquee_is_enabled(void);
void rgb_marquee_request_rf_ownership(rgb_marquee_rf_source_t source);
void rgb_marquee_release_rf_ownership(rgb_marquee_rf_source_t source);
bool rgb_marquee_rf_owns_leds(void);
bool rgb_marquee_rf_source_owns_leds(rgb_marquee_rf_source_t source);
bool rgb_marquee_rf_ownership_pending(void);
bool rgb_marquee_complete_rf_handoff(void);
void rgb_marquee_transition_rainbow_start(void);
bool rgb_marquee_transition_rainbow_is_active(void);
bool rgb_marquee_transition_rainbow_poll(void);
void rgb_marquee_usb_open_sweep(uint8_t color, uint8_t dir);
void rgb_marquee_usb_open_symmetric(uint8_t color);
void rgb_marquee_sweep_to(uint8_t color, uint8_t dir, uint8_t end);
void rgb_marquee_slot_switch(uint8_t led_down, uint8_t color_led_down, uint8_t led_up, uint8_t color_led_up);
void rgb_marquee_sweep_fade(uint8_t color, uint8_t dir, uint8_t end, uint8_t start_light, uint8_t stop_light);
void rgb_marquee_sweep_from_to(uint8_t color, uint8_t start, uint8_t stop);
void rgb_marquee_usb_idle(void);
bool rgb_marquee_show_battery_level(uint8_t percentage);
bool rgb_marquee_show_battery_segments(uint8_t count);
void rgb_marquee_symmetric_out(uint8_t color, uint8_t slot);
void rgb_marquee_symmetric_in(uint8_t color, uint8_t slot);

#endif
