#include "rainbow_color.h"

#define PWM_POLARITY_FALLING 0x8000U

static uint8_t scale_channel(uint8_t channel, uint8_t brightness) {
    return (uint8_t)(((uint16_t)channel * brightness + 127U) / 255U);
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

    color.red = scale_channel(color.red, brightness);
    color.green = scale_channel(color.green, brightness);
    color.blue = scale_channel(color.blue, brightness);
    return color;
}

uint16_t rainbow_pwm_compare(uint8_t intensity, uint16_t pwm_top) {
    uint32_t squared = (uint32_t)intensity * intensity;
    uint32_t on_time = (squared * pwm_top + 32512U) / 65025U;
    return (uint16_t)(PWM_POLARITY_FALLING | (pwm_top - on_time));
}
