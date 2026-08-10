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

int main(void) {
    assert_color(0, 255, 255, 0, 0);
    assert_color(256, 255, 255, 255, 0);
    assert_color(512, 255, 0, 255, 0);
    assert_color(768, 255, 0, 255, 255);
    assert_color(1024, 255, 0, 0, 255);
    assert_color(1280, 255, 255, 0, 255);
    assert_color(RAINBOW_PHASE_CYCLE, 255, 255, 0, 0);
    assert_color(256, 128, 128, 128, 0);

    assert(rainbow_pwm_compare(0, 1000) == (0x8000U | 1000U));
    assert(rainbow_pwm_compare(255, 1000) == 0x8000U);
    assert(rainbow_pwm_compare(128, 1000) == (0x8000U | 748U));

    puts("rainbow color tests passed");
    return 0;
}
