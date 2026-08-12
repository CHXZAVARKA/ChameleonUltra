#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "rainbow_color.h"

static void assert_color(uint16_t phase, uint8_t red, uint8_t green, uint8_t blue) {
    rainbow_rgb_t color = rainbow_color_at(phase, 255U);
    assert(color.red == red);
    assert(color.green == green);
    assert(color.blue == blue);
}

int main(void) {
    assert_color(0U, 255U, 0U, 0U);
    assert_color(256U, 180U, 180U, 0U);
    assert_color(512U, 0U, 255U, 0U);
    assert_color(768U, 0U, 180U, 180U);
    assert_color(1024U, 0U, 0U, 255U);
    assert_color(1280U, 180U, 0U, 180U);

    static const uint8_t from_slot_1[] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 6U, 5U, 4U, 3U, 2U, 1U, 0U};
    assert(boot_trail_frame_count(0U, 8U) == sizeof(from_slot_1));
    for (uint8_t frame = 0U; frame < sizeof(from_slot_1); frame++) {
        assert(boot_trail_position(frame, 0U, 8U) == from_slot_1[frame]);
    }

    static const uint8_t from_slot_6[] = {5U, 4U, 3U, 2U, 1U, 0U, 1U, 2U, 3U, 4U, 5U};
    assert(boot_trail_frame_count(5U, 8U) == sizeof(from_slot_6));
    for (uint8_t frame = 0U; frame < sizeof(from_slot_6); frame++) {
        assert(boot_trail_position(frame, 5U, 8U) == from_slot_6[frame]);
    }

    static const uint8_t forward_levels[] = {22U, 42U, 68U, 99U, 0U, 0U, 0U, 0U};
    static const uint8_t reversal_levels[] = {0U, 0U, 0U, 0U, 22U, 42U, 68U, 99U};
    static const uint8_t return_levels[] = {99U, 68U, 42U, 22U, 0U, 0U, 0U, 0U};
    for (uint8_t position = 0U; position < 8U; position++) {
        assert(boot_trail_level(3U, position, 0U, 8U) == forward_levels[position]);
        assert(boot_trail_level(7U, position, 0U, 8U) == reversal_levels[position]);
        assert(boot_trail_level(14U, position, 0U, 8U) == return_levels[position]);
    }

    assert(rainbow_pwm_compare(0U, 1000U) == (0x8000U | 1000U));
    assert(rainbow_pwm_compare(255U, 1000U) == 0x8000U);
    puts("boot rainbow model tests passed");
    return 0;
}
