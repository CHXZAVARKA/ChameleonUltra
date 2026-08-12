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

int main(void) {
    assert_stable_perceived_brightness(118U);
    assert_stable_perceived_brightness(176U);
    assert_smooth_channel_transitions(118U);
    assert_smooth_channel_transitions(176U);
    assert_primary_and_secondary_hues();

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

    assert(charging_filled_count(0U, 8U) == 0U);
    assert(charging_filled_count(12U, 8U) == 0U);
    assert(charging_filled_count(13U, 8U) == 1U);
    assert(charging_filled_count(65U, 8U) == 5U);
    assert(charging_filled_count(99U, 8U) == 7U);
    assert(charging_filled_count(100U, 8U) == 8U);

    assert(charging_particle_cycle_frames(65U, 8U) == 6U);
    assert(charging_particle_position(0U, 65U, 8U) == 7);
    assert(charging_particle_position(1U, 65U, 8U) == 6);
    assert(charging_particle_position(2U, 65U, 8U) == 5);
    assert(charging_particle_position(3U, 65U, 8U) == 4);
    assert(charging_particle_position(4U, 65U, 8U) == -1);
    assert(charging_particle_position(5U, 65U, 8U) == -1);
    assert(charging_particle_position(6U, 65U, 8U) == 7);
    assert(charging_particle_position(0U, 100U, 8U) == -1);

    assert(charging_particle_level(0U, 7U, 65U, 8U) == 99U);
    assert(charging_particle_level(1U, 7U, 65U, 8U) == 68U);
    assert(charging_particle_level(1U, 6U, 65U, 8U) == 99U);
    assert(charging_particle_level(3U, 4U, 65U, 8U) == 0U);
    assert(charging_particle_level(4U, 5U, 65U, 8U) == 42U);
    assert(charging_particle_level(5U, 5U, 65U, 8U) == 22U);
    assert(charging_particle_level(5U, 4U, 65U, 8U) == 0U);
    puts("boot rainbow model tests passed");
    return 0;
}
