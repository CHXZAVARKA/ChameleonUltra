#include <assert.h>
#include <stdint.h>

#include "battery_indicator.h"

static void assert_color(uint8_t percentage, uint8_t red, uint8_t green, uint8_t blue) {
    battery_indicator_rgb_t color = battery_indicator_color(percentage);
    assert(color.red == red);
    assert(color.green == green);
    assert(color.blue == blue);
}

static void assert_color_boundaries(void) {
    assert_color(0U, 255U, 0U, 0U);
    assert_color(1U, 255U, 0U, 0U);
    assert_color(34U, 255U, 128U, 0U);
    assert_color(67U, 255U, 255U, 0U);
    assert_color(100U, 0U, 255U, 0U);
    assert_color(101U, 0U, 255U, 0U);
}

static void assert_monotonic_red_to_green_gradient(void) {
    battery_indicator_rgb_t previous = battery_indicator_color(1U);
    for (uint8_t percentage = 2U; percentage <= 100U; percentage++) {
        battery_indicator_rgb_t current = battery_indicator_color(percentage);
        assert(current.red <= previous.red);
        assert(current.green >= previous.green);
        assert(current.blue == 0U);
        previous = current;
    }
}

static void assert_stock_spatial_gauge_boundaries(void) {
    assert(battery_indicator_lit_count(0U, 8U) == 0U);
    assert(battery_indicator_lit_count(1U, 8U) == 1U);
    assert(battery_indicator_lit_count(12U, 8U) == 1U);
    assert(battery_indicator_lit_count(13U, 8U) == 2U);
    assert(battery_indicator_lit_count(24U, 8U) == 2U);
    assert(battery_indicator_lit_count(25U, 8U) == 2U);
    assert(battery_indicator_lit_count(26U, 8U) == 3U);
    assert(battery_indicator_lit_count(50U, 8U) == 4U);
    assert(battery_indicator_lit_count(51U, 8U) == 5U);
    assert(battery_indicator_lit_count(75U, 8U) == 6U);
    assert(battery_indicator_lit_count(76U, 8U) == 7U);
    assert(battery_indicator_lit_count(87U, 8U) == 7U);
    assert(battery_indicator_lit_count(88U, 8U) == 8U);
    assert(battery_indicator_lit_count(99U, 8U) == 8U);
    assert(battery_indicator_lit_count(100U, 8U) == 8U);
    assert(battery_indicator_lit_count(100U, 0U) == 0U);
}

int main(void) {
    assert_color_boundaries();
    assert_monotonic_red_to_green_gradient();
    assert_stock_spatial_gauge_boundaries();
    return 0;
}
