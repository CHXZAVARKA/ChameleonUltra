#include "rainbow_color.h"

#define PWM_POLARITY_FALLING 0x8000U

static uint16_t integer_sqrt(uint32_t value) {
    uint32_t original = value;
    uint32_t result = 0U;
    uint32_t bit = 1UL << 30U;

    while (bit > value) {
        bit >>= 2U;
    }
    while (bit != 0U) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1U) + bit;
        } else {
            result >>= 1U;
        }
        bit >>= 2U;
    }

    uint32_t lower_error = original - result * result;
    uint32_t upper = result + 1U;
    uint32_t upper_error = upper * upper - original;
    return (uint16_t)(upper_error < lower_error ? upper : result);
}

static uint8_t normalize_channel(uint8_t channel, uint8_t brightness, uint16_t magnitude) {
    if (channel == 0U || magnitude == 0U) {
        return 0U;
    }
    return (uint8_t)(((uint32_t)channel * brightness + magnitude / 2U) / magnitude);
}

rainbow_rgb_t rainbow_color_at(uint16_t phase, uint8_t brightness) {
    uint16_t wrapped = phase % RAINBOW_PHASE_CYCLE;
    uint8_t segment = (uint8_t)(wrapped / RAINBOW_PHASE_SEGMENT);
    uint8_t offset = (uint8_t)(wrapped % RAINBOW_PHASE_SEGMENT);
    uint8_t rising = offset;
    uint8_t falling = (uint8_t)(255U - offset);
    rainbow_rgb_t color = {0U, 0U, 0U};

    switch (segment) {
        case 0:
            color.red = 255U;
            color.green = rising;
            break;
        case 1:
            color.red = falling;
            color.green = 255U;
            break;
        case 2:
            color.green = 255U;
            color.blue = rising;
            break;
        case 3:
            color.green = falling;
            color.blue = 255U;
            break;
        case 4:
            color.red = rising;
            color.blue = 255U;
            break;
        default:
            color.red = 255U;
            color.blue = falling;
            break;
    }

    uint32_t energy = (uint32_t)color.red * color.red +
                      (uint32_t)color.green * color.green +
                      (uint32_t)color.blue * color.blue;
    uint16_t magnitude = integer_sqrt(energy);
    color.red = normalize_channel(color.red, brightness, magnitude);
    color.green = normalize_channel(color.green, brightness, magnitude);
    color.blue = normalize_channel(color.blue, brightness, magnitude);
    return color;
}

uint16_t rainbow_pwm_compare(uint8_t intensity, uint16_t pwm_top) {
    uint32_t squared = (uint32_t)intensity * intensity;
    uint32_t on_time = (squared * pwm_top + 32512U) / 65025U;
    return (uint16_t)(PWM_POLARITY_FALLING | (pwm_top - on_time));
}

static uint8_t boot_trail_edge(uint8_t start, uint8_t position_count) {
    uint8_t last = (uint8_t)(position_count - 1U);
    return start <= last / 2U ? last : 0U;
}

uint8_t boot_trail_frame_count(uint8_t start, uint8_t position_count) {
    if (position_count < 2U || start >= position_count) {
        return 1U;
    }
    uint8_t edge = boot_trail_edge(start, position_count);
    uint8_t distance = edge > start ? (uint8_t)(edge - start) : (uint8_t)(start - edge);
    return (uint8_t)(distance * 2U + 1U);
}

uint8_t boot_trail_position(uint8_t frame, uint8_t start, uint8_t position_count) {
    if (position_count < 2U || start >= position_count) {
        return 0U;
    }

    uint8_t edge = boot_trail_edge(start, position_count);
    uint8_t distance = edge > start ? (uint8_t)(edge - start) : (uint8_t)(start - edge);
    uint8_t last_frame = (uint8_t)(distance * 2U);
    uint8_t bounded = frame > last_frame ? last_frame : frame;

    if (edge > start) {
        return bounded <= distance
            ? (uint8_t)(start + bounded)
            : (uint8_t)(edge - (bounded - distance));
    }
    return bounded <= distance
        ? (uint8_t)(start - bounded)
        : (uint8_t)(bounded - distance);
}

uint8_t boot_trail_level(uint8_t frame, uint8_t position, uint8_t start, uint8_t position_count) {
    static const uint8_t levels[BOOT_TRAIL_LENGTH] = {99U, 68U, 42U, 22U};
    uint8_t level = 0U;

    if (position >= position_count || start >= position_count) {
        return 0U;
    }
    for (uint8_t age = 0U; age < BOOT_TRAIL_LENGTH && age <= frame; age++) {
        if (boot_trail_position((uint8_t)(frame - age), start, position_count) == position &&
                levels[age] > level) {
            level = levels[age];
        }
    }
    return level;
}

uint8_t charging_filled_count(uint8_t percentage, uint8_t position_count) {
    if (position_count == 0U) {
        return 0U;
    }
    if (percentage >= 100U) {
        return position_count;
    }
    return (uint8_t)(((uint16_t)percentage * position_count) / 100U);
}

static uint8_t charging_particle_target(uint8_t percentage, uint8_t position_count) {
    uint8_t filled = charging_filled_count(percentage, position_count);
    return filled == 0U ? 0U : (uint8_t)(filled - 1U);
}

uint8_t charging_particle_cycle_frames(uint8_t percentage, uint8_t position_count) {
    if (position_count == 0U || percentage >= 100U) {
        return 1U;
    }
    uint8_t target = charging_particle_target(percentage, position_count);
    return (uint8_t)(position_count - target + 2U);
}

int8_t charging_particle_position(uint8_t frame, uint8_t percentage, uint8_t position_count) {
    if (position_count == 0U || percentage >= 100U) {
        return -1;
    }
    uint8_t target = charging_particle_target(percentage, position_count);
    uint8_t travel_frames = (uint8_t)(position_count - target);
    uint8_t bounded = (uint8_t)(frame % charging_particle_cycle_frames(percentage, position_count));
    if (bounded >= travel_frames) {
        return -1;
    }
    return (int8_t)((position_count - 1U) - bounded);
}

uint8_t charging_particle_level(
    uint8_t frame,
    uint8_t position,
    uint8_t percentage,
    uint8_t position_count
) {
    static const uint8_t levels[BOOT_TRAIL_LENGTH] = {99U, 68U, 42U, 22U};
    uint8_t filled = charging_filled_count(percentage, position_count);
    uint8_t level = 0U;

    if (position >= position_count || position < filled || percentage >= 100U) {
        return 0U;
    }
    for (uint8_t age = 0U; age < BOOT_TRAIL_LENGTH && age <= frame; age++) {
        int8_t particle = charging_particle_position(
            (uint8_t)(frame - age),
            percentage,
            position_count
        );
        if (particle == (int8_t)position && levels[age] > level) {
            level = levels[age];
        }
    }
    return level;
}
