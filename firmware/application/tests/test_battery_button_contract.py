#!/usr/bin/env python3
"""Integration contract for the configured long-B battery indication."""

from pathlib import Path


SOURCE_DIR = Path(__file__).resolve().parents[1] / "src"
APP_MAIN = (SOURCE_DIR / "app_main.c").read_text(encoding="utf-8")
BLE_MAIN = (SOURCE_DIR / "ble_main.c").read_text(encoding="utf-8")


assert "m_is_b_btn_press && b_long_shows_battery" in APP_MAIN
assert "ticks > APP_TIMER_TICKS(1000)" in APP_MAIN
assert (
    "rgb_marquee_show_battery(battery_level_is_available(), percentage_batt_lvl)"
    in APP_MAIN
)

release_start = APP_MAIN.index("if (m_is_btn_long_press && b_long_shows_battery)")
release_end = APP_MAIN.index("} else if (!m_is_btn_long_press)", release_start)
release_block = APP_MAIN[release_start:release_end]
assert release_block.index("rgb_marquee_stop()") < release_block.index("light_up_by_slot()")
assert "m_is_b_battery_indicator_active = false" in release_block

measurement_start = BLE_MAIN.index("percentage_batt_lvl = BATVOL2PERCENT")
measurement_end = BLE_MAIN.index("ble_bas_battery_level_update", measurement_start)
measurement_block = BLE_MAIN[measurement_start:measurement_end]
assert "m_battery_level_available = true" in measurement_block
