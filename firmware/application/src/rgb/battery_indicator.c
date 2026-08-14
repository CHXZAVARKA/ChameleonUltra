#include "battery_indicator.h"

#define BATTERY_RED_PERCENTAGE 1U
#define BATTERY_ORANGE_PERCENTAGE 34U
#define BATTERY_YELLOW_PERCENTAGE 67U
#define BATTERY_GREEN_PERCENTAGE 100U

static uint8_t interpolate_channel(
    uint8_t value_start,
    uint8_t value_end,
    uint8_t percentage,
    uint8_t percentage_start,
    uint8_t percentage_end
) {
    uint16_t offset = (uint16_t)(percentage - percentage_start);
    uint16_t range = (uint16_t)(percentage_end - percentage_start);

    if (value_end >= value_start) {
        uint16_t delta = (uint16_t)(value_end - value_start);
        return (uint8_t)(value_start + (delta * offset + range / 2U) / range);
    }

    uint16_t delta = (uint16_t)(value_start - value_end);
    return (uint8_t)(value_start - (delta * offset + range / 2U) / range);
}

battery_indicator_rgb_t battery_indicator_color(uint8_t percentage) {
    if (percentage <= BATTERY_RED_PERCENTAGE) {
        battery_indicator_rgb_t red = {255U, 0U, 0U};
        return red;
    }
    if (percentage <= BATTERY_ORANGE_PERCENTAGE) {
        battery_indicator_rgb_t red_to_orange = {
            255U,
            interpolate_channel(
                0U,
                128U,
                percentage,
                BATTERY_RED_PERCENTAGE,
                BATTERY_ORANGE_PERCENTAGE
            ),
            0U,
        };
        return red_to_orange;
    }
    if (percentage <= BATTERY_YELLOW_PERCENTAGE) {
        battery_indicator_rgb_t orange_to_yellow = {
            255U,
            interpolate_channel(
                128U,
                255U,
                percentage,
                BATTERY_ORANGE_PERCENTAGE,
                BATTERY_YELLOW_PERCENTAGE
            ),
            0U,
        };
        return orange_to_yellow;
    }

    uint8_t bounded = percentage > BATTERY_GREEN_PERCENTAGE
        ? BATTERY_GREEN_PERCENTAGE
        : percentage;
    battery_indicator_rgb_t yellow_to_green = {
        interpolate_channel(
            255U,
            0U,
            bounded,
            BATTERY_YELLOW_PERCENTAGE,
            BATTERY_GREEN_PERCENTAGE
        ),
        255U,
        0U,
    };
    return yellow_to_green;
}

uint8_t battery_indicator_lit_count(uint8_t percentage, uint8_t position_count) {
    if (percentage == 0U || position_count == 0U) {
        return 0U;
    }

    uint8_t bounded = percentage > 100U ? 100U : percentage;
    uint16_t scaled = (uint16_t)bounded * position_count;
    return (uint8_t)((scaled + 99U) / 100U);
}

uint8_t battery_indicator_hardware_index(
    uint8_t left_to_right_position,
    uint8_t position_count
) {
    if (position_count == 0U || left_to_right_position >= position_count) {
        return 0U;
    }

    return (uint8_t)(position_count - 1U - left_to_right_position);
}
