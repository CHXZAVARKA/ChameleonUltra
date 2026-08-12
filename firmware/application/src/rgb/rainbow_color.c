#include "rainbow_color.h"

#define PWM_POLARITY_FALLING 0x8000U
#define PERCEIVED_RED_WEIGHT 299U
#define PERCEIVED_GREEN_WEIGHT 587U
#define PERCEIVED_BLUE_WEIGHT 114U
#define COLOR_SCALE_ONE (1UL << 16U)

// HSP luma weights model perceived brightness after the quadratic PWM curve.
// Blue is the least efficient channel, so it anchors the target without clipping.

static uint32_t perceived_energy(rainbow_rgb_t color) {
    return PERCEIVED_RED_WEIGHT * (uint32_t)color.red * color.red +
           PERCEIVED_GREEN_WEIGHT * (uint32_t)color.green * color.green +
           PERCEIVED_BLUE_WEIGHT * (uint32_t)color.blue * color.blue;
}

static uint8_t scale_channel(uint8_t channel, uint32_t scale) {
    return (uint8_t)(((uint32_t)channel * scale + COLOR_SCALE_ONE / 2U) / COLOR_SCALE_ONE);
}

static rainbow_rgb_t scale_color(rainbow_rgb_t color, uint32_t scale) {
    rainbow_rgb_t scaled = {
        .red = scale_channel(color.red, scale),
        .green = scale_channel(color.green, scale),
        .blue = scale_channel(color.blue, scale),
    };
    return scaled;
}

static uint32_t distance(uint32_t left, uint32_t right) {
    return left > right ? left - right : right - left;
}

static rainbow_rgb_t balance_perceived_brightness(rainbow_rgb_t color, uint8_t brightness) {
    uint32_t target = PERCEIVED_BLUE_WEIGHT * (uint32_t)brightness * brightness;
    uint32_t lower = 0U;
    uint32_t upper = COLOR_SCALE_ONE;

    while (lower < upper) {
        uint32_t scale = (lower + upper + 1U) / 2U;
        if (perceived_energy(scale_color(color, scale)) <= target) {
            lower = scale;
        } else {
            upper = scale - 1U;
        }
    }

    rainbow_rgb_t lower_color = scale_color(color, lower);
    if (lower == COLOR_SCALE_ONE) {
        return lower_color;
    }
    rainbow_rgb_t upper_color = scale_color(color, lower + 1U);
    return distance(perceived_energy(lower_color), target) <=
                   distance(perceived_energy(upper_color), target)
               ? lower_color
               : upper_color;
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

    return balance_perceived_brightness(color, brightness);
}

uint16_t rainbow_pwm_compare(uint8_t intensity, uint16_t pwm_top) {
    uint32_t squared = (uint32_t)intensity * intensity;
    uint32_t on_time = (squared * pwm_top + 32512U) / 65025U;
    return (uint16_t)(PWM_POLARITY_FALLING | (pwm_top - on_time));
}

uint32_t rainbow_pwm_repeats(uint16_t frame_ms) {
    if (frame_ms == 0U) {
        return 0U;
    }
    return (uint32_t)frame_ms * RAINBOW_PWM_TICKS_PER_MS - 1U;
}

uint16_t stock_trail_pwm_compare(uint8_t age) {
    static const uint16_t compares[BOOT_TRAIL_LENGTH] = {1U, 600U, 880U, 980U};
    return age < BOOT_TRAIL_LENGTH ? compares[age] : 1000U;
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

int8_t boot_trail_age(uint8_t frame, uint8_t position, uint8_t start, uint8_t position_count) {
    if (position >= position_count || start >= position_count) {
        return -1;
    }
    for (uint8_t age = 0U; age < BOOT_TRAIL_LENGTH && age <= frame; age++) {
        if (boot_trail_position((uint8_t)(frame - age), start, position_count) == position) {
            return (int8_t)age;
        }
    }
    return -1;
}

uint8_t stock_sweep_frame_count(uint8_t end) {
    uint16_t frame_count = (uint16_t)end + 4U;
    return frame_count < 12U ? (uint8_t)frame_count : 12U;
}

static void stock_trail_channels(uint8_t frame, int8_t channel_positions[BOOT_TRAIL_LENGTH]) {
    for (uint8_t channel = 0U; channel < BOOT_TRAIL_LENGTH; channel++) {
        channel_positions[channel] = -1;
    }

    if (frame < 3U) {
        for (uint8_t age = 0U; age <= frame; age++) {
            channel_positions[3U - age] = (int8_t)(frame - age);
        }
    } else if (frame <= 7U) {
        for (uint8_t age = 0U; age < BOOT_TRAIL_LENGTH; age++) {
            channel_positions[3U - age] = (int8_t)(frame - age);
        }
    } else if (frame <= 10U) {
        uint8_t visible = (uint8_t)(11U - frame);
        for (uint8_t channel = 0U; channel < visible; channel++) {
            channel_positions[channel] = (int8_t)(frame - 3U + channel);
        }
    }
}

static int8_t stock_physical_position(int8_t path_position, uint8_t dir) {
    if (path_position < 0 || path_position > 7) {
        return -1;
    }
    return dir == 0U ? path_position : (int8_t)(7 - path_position);
}

int8_t stock_sweep_position(uint8_t frame, uint8_t channel, uint8_t dir, uint8_t end) {
    if (channel >= BOOT_TRAIL_LENGTH || frame >= stock_sweep_frame_count(end)) {
        return -1;
    }
    int8_t channel_positions[BOOT_TRAIL_LENGTH];
    stock_trail_channels(frame, channel_positions);
    if (frame >= end) {
        uint8_t hidden = (uint8_t)(frame - end);
        for (uint8_t index = 0U; index < hidden && index < BOOT_TRAIL_LENGTH; index++) {
            channel_positions[3U - index] = -1;
        }
        if (end <= 7U) {
            channel_positions[3] = (int8_t)end;
        }
    }
    return stock_physical_position(channel_positions[channel], dir);
}

int8_t stock_sweep_channel(uint8_t frame, uint8_t position, uint8_t dir, uint8_t end) {
    if (position >= 8U || frame >= stock_sweep_frame_count(end)) {
        return -1;
    }

    for (int8_t channel = 3; channel >= 0; channel--) {
        if (stock_sweep_position(frame, (uint8_t)channel, dir, end) == (int8_t)position) {
            return channel;
        }
    }
    return -1;
}

uint8_t stock_full_startup_frame_count(uint8_t slot, uint8_t position_count) {
    if (position_count != 8U || slot >= position_count) {
        return 0U;
    }
    uint8_t dir = slot > 3U ? 1U : 0U;
    uint8_t final_end = dir != 0U ? slot : (uint8_t)(7U - slot);
    return (uint8_t)(
        stock_sweep_frame_count(11U) * 2U + stock_sweep_frame_count(final_end)
    );
}

int8_t stock_full_startup_channel(
    uint8_t frame,
    uint8_t position,
    uint8_t slot,
    uint8_t position_count
) {
    uint8_t frame_count = stock_full_startup_frame_count(slot, position_count);
    if (frame >= frame_count || position >= position_count) {
        return -1;
    }

    uint8_t dir = slot > 3U ? 1U : 0U;
    uint8_t long_pass_frames = stock_sweep_frame_count(11U);
    if (frame < long_pass_frames) {
        return stock_sweep_channel(frame, position, (uint8_t)!dir, 11U);
    }
    frame = (uint8_t)(frame - long_pass_frames);
    if (frame < long_pass_frames) {
        return stock_sweep_channel(frame, position, dir, 11U);
    }
    frame = (uint8_t)(frame - long_pass_frames);
    uint8_t final_end = dir != 0U ? slot : (uint8_t)(7U - slot);
    return stock_sweep_channel(frame, position, (uint8_t)!dir, final_end);
}

uint8_t stock_linear_frame_count(uint8_t start, uint8_t stop) {
    return start > stop ? (uint8_t)(start - stop + 1U) : (uint8_t)(stop - start + 1U);
}

uint8_t stock_linear_position(uint8_t frame, uint8_t start, uint8_t stop) {
    uint8_t frame_count = stock_linear_frame_count(start, stop);
    uint8_t bounded = frame < frame_count ? frame : (uint8_t)(frame_count - 1U);
    return start < stop ? (uint8_t)(start + bounded) : (uint8_t)(start - bounded);
}

uint8_t stock_fade_frame_count(uint8_t end) {
    return end < 12U ? end : 12U;
}

int8_t stock_fade_position(uint8_t frame, uint8_t channel, uint8_t dir, uint8_t end) {
    if (channel >= BOOT_TRAIL_LENGTH || frame >= stock_fade_frame_count(end)) {
        return -1;
    }
    int8_t channel_positions[BOOT_TRAIL_LENGTH];
    stock_trail_channels(frame, channel_positions);
    return stock_physical_position(channel_positions[channel], dir);
}

int8_t stock_fade_channel(uint8_t frame, uint8_t position, uint8_t dir, uint8_t end) {
    if (position >= 8U || frame >= stock_fade_frame_count(end)) {
        return -1;
    }
    for (int8_t channel = 3; channel >= 0; channel--) {
        if (stock_fade_position(frame, (uint8_t)channel, dir, end) == (int8_t)position) {
            return channel;
        }
    }
    return -1;
}

uint8_t stock_fade_level(
    uint8_t frame,
    uint8_t channel,
    uint8_t end,
    uint8_t start_light,
    uint8_t stop_light
) {
    static const double channel_scale[BOOT_TRAIL_LENGTH] = {0.01, 0.30, 0.60, 0.99};
    if (end == 0U || channel >= BOOT_TRAIL_LENGTH || frame >= stock_fade_frame_count(end)) {
        return 0U;
    }
    double light = (((double)stop_light - (double)start_light) / end) * frame + start_light;
    return (uint8_t)(channel_scale[channel] * light);
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
    uint8_t filled = charging_filled_count(percentage, position_count);
    uint8_t target = charging_particle_target(percentage, position_count);
    // Leave exactly two fully dark frames after the four-level trail. At 0%
    // the head remains visible at slot 1, so that cycle needs one additional
    // frame before the same two-frame pause begins.
    return (uint8_t)(
        position_count - target + BOOT_TRAIL_LENGTH + (filled == 0U ? 1U : 0U)
    );
}

int8_t charging_particle_position(uint32_t frame, uint8_t percentage, uint8_t position_count) {
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

int8_t charging_particle_age(
    uint32_t frame,
    uint8_t position,
    uint8_t percentage,
    uint8_t position_count
) {
    uint8_t filled = charging_filled_count(percentage, position_count);
    if (position >= position_count || position < filled || percentage >= 100U) {
        return -1;
    }
    for (uint8_t age = 0U; age < BOOT_TRAIL_LENGTH && age <= frame; age++) {
        if (charging_particle_position(frame - age, percentage, position_count) ==
                (int8_t)position) {
            return (int8_t)age;
        }
    }
    return -1;
}
