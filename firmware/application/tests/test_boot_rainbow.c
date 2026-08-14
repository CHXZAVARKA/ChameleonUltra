#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "rainbow_color.h"

#define TEST_PWM_TOP 1000U
#define PERCEIVED_RED_WEIGHT 299U
#define PERCEIVED_GREEN_WEIGHT 587U
#define PERCEIVED_BLUE_WEIGHT 114U

static uint16_t pwm_on_time(uint8_t intensity) {
    return (uint16_t)(TEST_PWM_TOP - (rainbow_pwm_compare(intensity, TEST_PWM_TOP) & 0x7FFFU));
}

static uint32_t perceived_brightness_energy(rainbow_rgb_t color) {
    return PERCEIVED_RED_WEIGHT * pwm_on_time(color.red) +
           PERCEIVED_GREEN_WEIGHT * pwm_on_time(color.green) +
           PERCEIVED_BLUE_WEIGHT * pwm_on_time(color.blue);
}

static uint8_t channel_delta(uint8_t left, uint8_t right) {
    return left > right ? (uint8_t)(left - right) : (uint8_t)(right - left);
}

static void assert_stable_perceived_brightness(uint8_t brightness) {
    uint32_t minimum = UINT32_MAX;
    uint32_t maximum = 0U;
    uint16_t minimum_phase = 0U;
    uint16_t maximum_phase = 0U;

    for (uint16_t phase = 0U; phase < RAINBOW_PHASE_CYCLE; phase++) {
        uint32_t energy = perceived_brightness_energy(rainbow_color_at(phase, brightness));
        if (energy < minimum) {
            minimum = energy;
            minimum_phase = phase;
        }
        if (energy > maximum) {
            maximum = energy;
            maximum_phase = phase;
        }
    }

    fprintf(
        stderr,
        "rainbow perceived brightness: min=%lu at phase=%u, max=%lu at phase=%u, spread=%lu\n",
        (unsigned long)minimum,
        minimum_phase,
        (unsigned long)maximum,
        maximum_phase,
        (unsigned long)(maximum - minimum)
    );
    assert(maximum - minimum <= maximum / 12U);
}

static void assert_smooth_channel_transitions(uint8_t brightness) {
    rainbow_rgb_t previous = rainbow_color_at(RAINBOW_PHASE_CYCLE - 1U, brightness);
    for (uint16_t phase = 0U; phase < RAINBOW_PHASE_CYCLE; phase++) {
        rainbow_rgb_t current = rainbow_color_at(phase, brightness);
        assert(channel_delta(previous.red, current.red) <= 2U);
        assert(channel_delta(previous.green, current.green) <= 2U);
        assert(channel_delta(previous.blue, current.blue) <= 2U);
        previous = current;
    }
}

static void assert_primary_and_secondary_hues(void) {
    rainbow_rgb_t red = rainbow_color_at(0U, 255U);
    rainbow_rgb_t yellow = rainbow_color_at(256U, 255U);
    rainbow_rgb_t green = rainbow_color_at(512U, 255U);
    rainbow_rgb_t cyan = rainbow_color_at(768U, 255U);
    rainbow_rgb_t blue = rainbow_color_at(1024U, 255U);
    rainbow_rgb_t magenta = rainbow_color_at(1280U, 255U);

    assert(red.red > 0U && red.green == 0U && red.blue == 0U);
    assert(yellow.red > 0U && yellow.green > 0U && yellow.blue == 0U);
    assert(green.red == 0U && green.green > 0U && green.blue == 0U);
    assert(cyan.red == 0U && cyan.green > 0U && cyan.blue > 0U);
    assert(blue.red == 0U && blue.green == 0U && blue.blue > 0U);
    assert(magenta.red > 0U && magenta.green == 0U && magenta.blue > 0U);
}

static void assert_visible_rainbow_profile(void) {
    assert(RAINBOW_DISPLAY_BRIGHTNESS == 255U);
    assert(RAINBOW_PWM_TICKS_PER_MS == 16U);
    assert(rainbow_pwm_repeats(40U) == 639U);
    assert(rainbow_pwm_repeats(50U) == 799U);

    rainbow_rgb_t red = rainbow_color_at(0U, RAINBOW_DISPLAY_BRIGHTNESS);
    rainbow_rgb_t green = rainbow_color_at(512U, RAINBOW_DISPLAY_BRIGHTNESS);
    rainbow_rgb_t blue = rainbow_color_at(1024U, RAINBOW_DISPLAY_BRIGHTNESS);
    assert(pwm_on_time(red.red) >= 350U);
    assert(pwm_on_time(green.green) >= 180U);
    assert(pwm_on_time(blue.blue) >= 950U);
}

static int8_t brightest_position_for_startup(uint8_t frame, uint8_t slot) {
    for (uint8_t position = 0U; position < 8U; position++) {
        if (stock_full_startup_channel(frame, position, slot, 8U) == 3) {
            return (int8_t)position;
        }
    }
    return -1;
}

static int8_t brightest_position_for_fade(uint8_t frame, uint8_t dir) {
    for (uint8_t position = 0U; position < 8U; position++) {
        if (stock_fade_channel(frame, position, dir, 7U) == 3) {
            return (int8_t)position;
        }
    }
    return -1;
}

static void assert_stock_full_startup_contract(void) {
    static const int8_t slot_1_brightest[] = {
        7, 6, 5, 4, 3, 2, 1, 0, -1, -1, -1, -1,
        0, 1, 2, 3, 4, 5, 6, 7, -1, -1, -1, -1,
        7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0,
    };
    assert(stock_full_startup_frame_count(0U, 8U) == sizeof(slot_1_brightest));
    for (uint8_t frame = 0U; frame < sizeof(slot_1_brightest); frame++) {
        assert(brightest_position_for_startup(frame, 0U) == slot_1_brightest[frame]);
    }

    static const int8_t slot_6_brightest[] = {
        0, 1, 2, 3, 4, 5, 6, 7, -1, -1, -1, -1,
        7, 6, 5, 4, 3, 2, 1, 0, -1, -1, -1, -1,
        0, 1, 2, 3, 4, 5, 5, 5, 5,
    };
    assert(stock_full_startup_frame_count(5U, 8U) == sizeof(slot_6_brightest));
    for (uint8_t frame = 0U; frame < sizeof(slot_6_brightest); frame++) {
        assert(brightest_position_for_startup(frame, 5U) == slot_6_brightest[frame]);
    }

    uint8_t final_frame = (uint8_t)(stock_full_startup_frame_count(0U, 8U) - 1U);
    for (uint8_t position = 0U; position < 8U; position++) {
        assert(stock_full_startup_channel(final_frame, position, 0U, 8U) ==
               (position == 0U ? 3 : -1));
    }

    static const int8_t forward_trail[] = {0, 1, 2, 3};
    static const int8_t edge_trail[] = {4, 5, 6, 7};
    static const int8_t leaving_edge[] = {5, 6, 7, -1};
    static const int8_t final_endpoint[] = {7, -1, -1, 7};
    for (uint8_t channel = 0U; channel < 4U; channel++) {
        assert(stock_sweep_position(3U, channel, 0U, 11U) == forward_trail[channel]);
        assert(stock_sweep_position(7U, channel, 0U, 11U) == edge_trail[channel]);
        assert(stock_sweep_position(8U, channel, 0U, 11U) == leaving_edge[channel]);
        assert(stock_sweep_position(10U, channel, 0U, 7U) == final_endpoint[channel]);
        assert(stock_sweep_position(11U, channel, 0U, 11U) == -1);
        assert(stock_sweep_position(11U, channel, 1U, 11U) == -1);
    }
}

static void assert_stock_full_shutdown_contract(void) {
    assert(stock_linear_frame_count(5U, 7U) == 3U);
    assert(stock_linear_position(0U, 5U, 7U) == 5U);
    assert(stock_linear_position(1U, 5U, 7U) == 6U);
    assert(stock_linear_position(2U, 5U, 7U) == 7U);

    assert(stock_fade_frame_count(7U) == 7U);
    for (uint8_t frame = 0U; frame < 7U; frame++) {
        assert(brightest_position_for_fade(frame, 0U) == (int8_t)frame);
        assert(brightest_position_for_fade(frame, 1U) == (int8_t)(7U - frame));
    }

    static const uint8_t head_levels[] = {98U, 94U, 91U, 87U, 84U, 81U, 77U};
    for (uint8_t frame = 0U; frame < 7U; frame++) {
        assert(stock_fade_level(frame, 3U, 7U, 99U, 75U) == head_levels[frame]);
    }
    assert(stock_fade_level(6U, 3U, 7U, 25U, 0U) == 3U);
    for (uint8_t channel = 0U; channel < 4U; channel++) {
        assert(stock_fade_position(7U, channel, 0U, 7U) == -1);
        assert(stock_fade_position(7U, channel, 1U, 7U) == -1);
    }
}

int main(void) {
    assert_stable_perceived_brightness(RAINBOW_DISPLAY_BRIGHTNESS);
    assert_stable_perceived_brightness(118U);
    assert_stable_perceived_brightness(176U);
    assert_smooth_channel_transitions(RAINBOW_DISPLAY_BRIGHTNESS);
    assert_smooth_channel_transitions(118U);
    assert_smooth_channel_transitions(176U);
    assert_primary_and_secondary_hues();
    assert_visible_rainbow_profile();
    assert_stock_full_startup_contract();
    assert_stock_full_shutdown_contract();

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

    static const int8_t forward_ages[] = {3, 2, 1, 0, -1, -1, -1, -1};
    static const int8_t reversal_ages[] = {-1, -1, -1, -1, 3, 2, 1, 0};
    static const int8_t return_ages[] = {0, 1, 2, 3, -1, -1, -1, -1};
    for (uint8_t position = 0U; position < 8U; position++) {
        assert(boot_trail_age(3U, position, 0U, 8U) == forward_ages[position]);
        assert(boot_trail_age(7U, position, 0U, 8U) == reversal_ages[position]);
        assert(boot_trail_age(14U, position, 0U, 8U) == return_ages[position]);
    }

    assert(rainbow_pwm_compare(0U, 1000U) == (0x8000U | 1000U));
    assert(rainbow_pwm_compare(255U, 1000U) == 0x8000U);

    assert(charging_filled_count(0U, 8U) == 0U);
    assert(charging_filled_count(12U, 8U) == 0U);
    assert(charging_filled_count(13U, 8U) == 1U);
    assert(charging_filled_count(65U, 8U) == 5U);
    assert(charging_filled_count(99U, 8U) == 7U);
    assert(charging_filled_count(100U, 8U) == 8U);

    assert(CHARGING_PARTICLE_FRAME_MS >= 120U);
    // The rainbow head must use the same full position brightness as every
    // other fully lit state. Trailing ages remain progressively dimmer.
    assert(stock_trail_pwm_compare(0U) == 0U);
    assert(stock_trail_pwm_compare(1U) == 600U);
    assert(stock_trail_pwm_compare(2U) == 880U);
    assert(stock_trail_pwm_compare(3U) == 980U);

    assert(charging_particle_cycle_frames(65U, 8U) == 8U);
    assert(charging_particle_position(0U, 65U, 8U) == 7);
    assert(charging_particle_position(1U, 65U, 8U) == 6);
    assert(charging_particle_position(2U, 65U, 8U) == 5);
    assert(charging_particle_position(3U, 65U, 8U) == 4);
    assert(charging_particle_position(4U, 65U, 8U) == -1);
    assert(charging_particle_position(5U, 65U, 8U) == -1);
    assert(charging_particle_position(6U, 65U, 8U) == -1);
    assert(charging_particle_position(7U, 65U, 8U) == -1);
    assert(charging_particle_position(8U, 65U, 8U) == 7);
    assert(charging_particle_position(0U, 100U, 8U) == -1);

    assert(charging_particle_age(0U, 7U, 65U, 8U) == 0);
    assert(charging_particle_age(1U, 7U, 65U, 8U) == 1);
    assert(charging_particle_age(1U, 6U, 65U, 8U) == 0);
    assert(charging_particle_age(3U, 4U, 65U, 8U) == -1);
    assert(charging_particle_age(4U, 5U, 65U, 8U) == 2);
    assert(charging_particle_age(5U, 5U, 65U, 8U) == 3);
    assert(charging_particle_age(5U, 4U, 65U, 8U) == -1);
    for (uint8_t position = 0U; position < 8U; position++) {
        assert(charging_particle_age(6U, position, 65U, 8U) == -1);
        assert(charging_particle_age(7U, position, 65U, 8U) == -1);
    }
    for (uint8_t position = 0U; position < 8U; position++) {
        assert(charging_particle_age(8U, position, 65U, 8U) ==
               charging_particle_age(0U, position, 65U, 8U));
    }
    assert(charging_particle_cycle_frames(0U, 8U) == 13U);
    for (uint8_t position = 0U; position < 8U; position++) {
        assert(charging_particle_age(11U, position, 0U, 8U) == -1);
        assert(charging_particle_age(12U, position, 0U, 8U) == -1);
    }
    // Crossing the former uint8_t boundary must preserve the exact phase and
    // trail instead of jumping from cycle phase 7 to phase 0.
    for (uint8_t position = 0U; position < 8U; position++) {
        assert(charging_particle_age(256U, position, 65U, 8U) ==
               charging_particle_age(0U, position, 65U, 8U));
        assert(charging_particle_age(257U, position, 65U, 8U) ==
               charging_particle_age(1U, position, 65U, 8U));
    }
    puts("boot rainbow model tests passed");
    return 0;
}
