#!/usr/bin/env python3
"""Source-level ownership contract for LED animations and RF interrupts."""

from pathlib import Path


SOURCE_DIR = Path(__file__).resolve().parents[1] / "src"
APP_MAIN = (SOURCE_DIR / "app_main.c").read_text(encoding="utf-8")
RGB_MARQUEE = (SOURCE_DIR / "rgb_marquee.c").read_text(encoding="utf-8")
HF_TAG = (SOURCE_DIR / "rfid/nfctag/hf/nfc_14a.c").read_text(encoding="utf-8")
LF_TAG = (SOURCE_DIR / "rfid/nfctag/lf/lf_tag_em.c").read_text(encoding="utf-8")


def extract_function(source: str, signature: str, next_signature: str) -> str:
    start = source.index(signature)
    end = source.index(next_signature, start)
    return source[start:end]


shutdown = extract_function(
    APP_MAIN,
    "static void system_off_enter(void)",
    "static void check_wakeup_src(void)",
)
assert "rgb_marquee_transition_rainbow_start();" in shutdown
assert "shutdown_interrupted_by_activity()" in shutdown
assert "rgb_marquee_rf_ownership_pending()" in APP_MAIN
assert "nrfx_power_usbstatus_get()" in APP_MAIN

show_battery = extract_function(
    APP_MAIN,
    "static void show_battery(void)",
    "static void offline_status_blink_color",
)
assert "while (batt_lvl_in_milli_volts == 0)" in show_battery
assert "rgb_marquee_show_battery_level(percentage_batt_lvl);" in show_battery

hf_detect = extract_function(
    HF_TAG,
    "case NRFX_NFCT_EVT_FIELD_DETECTED:",
    "case NRFX_NFCT_EVT_FIELD_LOST:",
)
lf_detect = extract_function(
    LF_TAG,
    "static void lpcomp_event_handler",
    "static void lpcomp_init",
)
assert hf_detect.index("rgb_marquee_request_rf_ownership();") < hf_detect.index(
    "g_is_tag_emulating = true;"
)
assert lf_detect.index("rgb_marquee_request_rf_ownership();") < lf_detect.index(
    "g_is_tag_emulating = true;"
)

assert "nrfx_pwm_stop(&pwm0_ins, false);" in RGB_MARQUEE
assert "rgb_marquee_complete_rf_handoff" in APP_MAIN
