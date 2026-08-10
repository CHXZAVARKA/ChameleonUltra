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

static uint8_t normalize_channel(
    uint8_t channel,
    uint8_t brightness,
    uint16_t magnitude
) {
    if (channel == 0U || magnitude == 0U) {
        return 0U;
    }
    return (uint8_t)(((uint32_t)channel * brightness + magnitude / 2U) /
                     magnitude);
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

rainbow_rgb_t battery_level_color(uint8_t percentage, uint8_t brightness) {
    uint8_t clamped = percentage > 100U ? 100U : percentage;
    rainbow_rgb_t color = {0U, 0U, 0U};

    if (clamped <= 50U) {
        color.red = brightness;
        color.green = (uint8_t)(((uint16_t)brightness * clamped + 25U) / 50U);
    } else {
        color.red = (uint8_t)(((uint16_t)brightness * (100U - clamped) + 25U) /
                              50U);
        color.green = brightness;
    }
    return color;
}

uint8_t led_bounce_position(uint32_t step, uint8_t position_count) {
    if (position_count < 2U) {
        return 0U;
    }
    uint8_t last = (uint8_t)(position_count - 1U);
    uint8_t period = (uint8_t)(last * 2U);
    uint8_t offset = (uint8_t)(step % period);
    return offset <= last ? offset : (uint8_t)(period - offset);
}

uint8_t led_bounce_trail_level(
    uint32_t step,
    uint8_t position,
    uint8_t position_count
) {
    static const uint8_t levels[] = {99U, 60U, 30U, 1U};
    uint8_t level = 0U;

    if (position_count == 0U || position >= position_count) {
        return 0U;
    }
    for (uint8_t age = 0U; age < sizeof(levels) && age <= step; age++) {
        if (led_bounce_position(step - age, position_count) == position &&
                levels[age] > level) {
            level = levels[age];
        }
    }
    return level;
}

uint16_t rainbow_pwm_compare(uint8_t intensity, uint16_t pwm_top) {
    uint32_t squared = (uint32_t)intensity * intensity;
    uint32_t on_time = (squared * pwm_top + 32512U) / 65025U;
    return (uint16_t)(PWM_POLARITY_FALLING | (pwm_top - on_time));
}
