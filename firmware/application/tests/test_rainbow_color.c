#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "rainbow_color.h"

static void assert_color(uint16_t phase, uint8_t brightness,
                         uint8_t red, uint8_t green, uint8_t blue) {
    rainbow_rgb_t color = rainbow_color_at(phase, brightness);
    assert(color.red == red);
    assert(color.green == green);
    assert(color.blue == blue);
}

static void assert_rgb(rainbow_rgb_t color,
                       uint8_t red, uint8_t green, uint8_t blue) {
    assert(color.red == red);
    assert(color.green == green);
    assert(color.blue == blue);
}

int main(void) {
    assert_color(0, 255, 255, 0, 0);
    assert_color(256, 255, 180, 180, 0);
    assert_color(512, 255, 0, 255, 0);
    assert_color(768, 255, 0, 180, 180);
    assert_color(1024, 255, 0, 0, 255);
    assert_color(1280, 255, 180, 0, 180);
    assert_color(RAINBOW_PHASE_CYCLE, 255, 255, 0, 0);
    assert_color(256, 128, 90, 90, 0);

    for (uint16_t phase = 0; phase < RAINBOW_PHASE_CYCLE; phase += 16) {
        rainbow_rgb_t color = rainbow_color_at(phase, 180);
        uint32_t energy = (uint32_t)color.red * color.red +
                          (uint32_t)color.green * color.green +
                          (uint32_t)color.blue * color.blue;
        assert(energy >= 32000U);
        assert(energy <= 32800U);
    }
    rainbow_rgb_t first = rainbow_color_at(0, 180);
    rainbow_rgb_t last = rainbow_color_at(
        RAINBOW_PHASE_CYCLE - (RAINBOW_PHASE_CYCLE / 96U),
        180
    );
    assert((uint16_t)(first.red - last.red) + last.blue <= 16U);

    assert(rainbow_pwm_compare(0, 1000) == (0x8000U | 1000U));
    assert(rainbow_pwm_compare(255, 1000) == 0x8000U);
    assert(rainbow_pwm_compare(128, 1000) == (0x8000U | 748U));

    assert_rgb(battery_level_color(0, 180), 180, 0, 0);
    assert_rgb(battery_level_color(25, 180), 180, 90, 0);
    assert_rgb(battery_level_color(50, 180), 180, 180, 0);
    assert_rgb(battery_level_color(75, 180), 90, 180, 0);
    assert_rgb(battery_level_color(100, 180), 0, 180, 0);
    assert_rgb(battery_level_color(255, 180), 0, 180, 0);

    const uint8_t expected_bounce[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0,
    };
    for (uint8_t step = 0; step < sizeof(expected_bounce); step++) {
        assert(led_bounce_position(step, 8) == expected_bounce[step]);
    }
    assert(led_bounce_position(37, 1) == 0);
    assert(led_bounce_position(37, 0) == 0);
    assert(led_bounce_position(254, 8) == 2);
    assert(led_bounce_position(255, 8) == 3);
    assert(led_bounce_position(256, 8) == 4);

    puts("rainbow color tests passed");
    return 0;
}
