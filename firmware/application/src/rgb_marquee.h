#ifndef RGB_MARQUEE_H
#define RGB_MARQUEE_H

#include <stdint.h>
#include "nrf_drv_pwm.h"

typedef enum {
    RGB_LED_OWNER_HF = 1U << 0U,
    RGB_LED_OWNER_LF = 1U << 1U,
    RGB_LED_OWNER_FIELD_GENERATOR = 1U << 2U,
    RGB_LED_OWNER_USB_REMOVED = 1U << 3U,
} rgb_led_owner_t;

void rgb_marquee_init(void);
void rgb_marquee_stop(void);
void rgb_marquee_reset(void);
bool rgb_marquee_is_enabled(void);
void rgb_marquee_boot_rainbow_trail(uint8_t slot, uint8_t final_color);
void rgb_marquee_show_battery(uint8_t battery_percentage);
void rgb_marquee_full_startup_rainbow(uint8_t slot, uint8_t final_color);
bool rgb_marquee_full_shutdown_begin(uint8_t slot);
bool rgb_marquee_full_shutdown_move_to_edge(uint8_t start, uint8_t stop);
bool rgb_marquee_full_shutdown_fade(
    uint8_t dir,
    uint8_t end,
    uint8_t start_light,
    uint8_t stop_light
);
void rgb_marquee_usb_open_sweep(uint8_t color, uint8_t dir);
void rgb_marquee_usb_open_symmetric(uint8_t color);
void rgb_marquee_sweep_to(uint8_t color, uint8_t dir, uint8_t end);
void rgb_marquee_slot_switch(uint8_t led_down, uint8_t color_led_down, uint8_t led_up, uint8_t color_led_up);
void rgb_marquee_sweep_fade(uint8_t color, uint8_t dir, uint8_t end, uint8_t start_light, uint8_t stop_light);
void rgb_marquee_sweep_from_to(uint8_t color, uint8_t start, uint8_t stop);
void rgb_marquee_usb_idle(uint8_t battery_percentage);
void rgb_marquee_usb_suspend(rgb_led_owner_t owner);
void rgb_marquee_usb_resume(rgb_led_owner_t owner);
bool rgb_marquee_usb_is_suspended(void);
bool rgb_marquee_usb_owner_is_active(rgb_led_owner_t owner);
void rgb_marquee_symmetric_out(uint8_t color, uint8_t slot);
void rgb_marquee_symmetric_in(uint8_t color, uint8_t slot);

#endif
