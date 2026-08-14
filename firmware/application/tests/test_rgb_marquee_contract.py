#!/usr/bin/env python3
"""Hardware-facing PWM contract for the shared Ultra/Lite LED driver."""

from pathlib import Path
import re


SOURCE = (
    Path(__file__).resolve().parents[1] / "src" / "rgb_marquee.c"
).read_text(encoding="utf-8")
APP_MAIN = (
    Path(__file__).resolve().parents[1] / "src" / "app_main.c"
).read_text(encoding="utf-8")


def initializer(name: str) -> str:
    match = re.search(
        rf"\b{name}\s*=\s*\{{(?P<body>.*?)\n\}};",
        SOURCE,
        flags=re.DOTALL,
    )
    assert match, f"could not parse {name} initializer"
    return match.group("body")


position_pwm = initializer("pwm_config")
rainbow_pwm = initializer("boot_rgb_pwm_config")

assert ".base_clock = NRF_PWM_CLK_1MHz" in position_pwm
assert ".top_value = PWM_MAX" in position_pwm
assert ".base_clock = NRF_PWM_CLK_16MHz" in rainbow_pwm
assert ".top_value = RAINBOW_PWM_COUNTER_TOP" in rainbow_pwm

# Percentage updates must not restart the independently phased position PWM.
percentage_branch = re.search(
    r"if \(battery_percentage != charging_last_percentage\) \{(?P<body>.*?)\n    \}",
    SOURCE,
    flags=re.DOTALL,
)
assert percentage_branch, "could not parse charging percentage update"
body = percentage_branch.group("body")
assert "charging_start_rainbow" not in body
assert "battery_percentage >= 100U && !charging_show_positions(battery_percentage)" in body

show_positions = re.search(
    r"static bool charging_show_positions.*?\{(?P<body>.*?)\n\}",
    SOURCE,
    flags=re.DOTALL,
)
assert show_positions, "could not parse charging position renderer"
body = show_positions.group("body")
assert "if (battery_percentage >= 100U)" not in body
assert "NRF_DRV_PWM_PIN_NOT_USED" in body
assert "position_pwm_start(NULL)" in body

initial_frame = re.search(
    r"if \(rgb_marquee_usb_idle_step == 0U\) \{(?P<body>.*?)\n    \}",
    SOURCE,
    flags=re.DOTALL,
)
assert initial_frame, "could not parse initial charging frame"
body = initial_frame.group("body")
assert "charging_show_positions(battery_percentage)" in body
assert body.index("charging_show_positions") < body.index("rgb_marquee_usb_idle_step = 1U")

position_writes = re.findall(
    r"(?:nrfx_pwm_uninit|nrfx_pwm_stop|nrf_drv_pwm_init)\(&pwm0_ins",
    SOURCE,
)
assert len(position_writes) == 4, "PWM1 lifecycle must stay inside its two helpers"

# Opening the CDC port must not replace the charging rainbow with the old
# slot-color sweep. USB power owns one continuous charging presentation before
# and after the desktop or mobile app opens the serial connection.
usb_status = re.search(
    r"static void blink_usb_led_status\(void\) \{(?P<body>.*?)\n\}",
    APP_MAIN,
    flags=re.DOTALL,
)
assert usb_status, "could not parse USB LED status handler"
body = usb_status.group("body")
assert "rgb_marquee_usb_open_sweep" not in body
assert "rgb_marquee_usb_open_symmetric" not in body
assert body.count("rgb_marquee_usb_idle(percentage_batt_lvl)") == 1

battery_level = re.search(
    r"static void battery_indicator_show_level\(uint8_t battery_percentage\) \{(?P<body>.*?)\n\}",
    SOURCE,
    flags=re.DOTALL,
)
assert battery_level, "could not parse long-B battery level renderer"
body = battery_level.group("body")
assert re.search(
    r"for \(uint8_t position = 0U; position < lit_count; position\+\+\) \{"
    r".*?nrf_gpio_pin_set\(led_pins\[position\]\)",
    body,
    flags=re.DOTALL,
), "button B must fill the battery gauge from LED 1 toward LED 8"

print("RGB marquee hardware contract tests passed")
