#!/usr/bin/env python3
"""Source-level ownership contract for LED animations and RF interrupts."""

from pathlib import Path


SOURCE_DIR = Path(__file__).resolve().parents[1] / "src"
APP_MAIN = (SOURCE_DIR / "app_main.c").read_text(encoding="utf-8")
APP_CMD = (SOURCE_DIR / "app_cmd.c").read_text(encoding="utf-8")
RGB_MARQUEE = (SOURCE_DIR / "rgb_marquee.c").read_text(encoding="utf-8")
RFID_MAIN = (SOURCE_DIR / "rfid_main.c").read_text(encoding="utf-8")
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
assert shutdown.count("shutdown_interrupted_by_activity()") >= 4
assert "static volatile bool m_system_off_processing" in APP_MAIN
assert "if (m_system_off_processing)" in extract_function(
    APP_MAIN,
    "static void button_pin_handler",
    "static void timer_button_event_handle",
)
assert "rgb_marquee_rf_owns_leds()" in APP_MAIN
assert "rgb_marquee_rf_ownership_pending()" in APP_MAIN
assert "nrfx_power_usbstatus_get()" in APP_MAIN

show_battery = extract_function(
    APP_MAIN,
    "static void show_battery(void)",
    "static void offline_status_blink_color",
)
assert "while (batt_lvl_in_milli_volts == 0)" in show_battery
assert "if (!rgb_marquee_show_battery_level(percentage_batt_lvl))" in show_battery
assert "rgb_marquee_show_battery_segments" in show_battery

field_generator = extract_function(
    APP_MAIN,
    "static void field_generator_rainbow_loop",
    "static void button_pin_handler",
)
assert "rgb_marquee_rf_owns_leds()" in field_generator
assert "CRITICAL_REGION_ENTER();" in field_generator

cycle_slot = extract_function(
    APP_MAIN,
    "static void cycle_slot",
    "static void show_battery",
)
assert "apply_slot_change(slot_now, slot_new);" in cycle_slot
assert "nrf_gpio_pin_clear" not in cycle_slot

slot_enable = extract_function(
    APP_CMD,
    "static data_frame_tx_t *cmd_processor_set_slot_enable",
    "static data_frame_tx_t *cmd_processor_slot_data_config_save",
)
assert "if (!rgb_marquee_rf_owns_leds()" in slot_enable
assert "CRITICAL_REGION_ENTER();" in slot_enable

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
hf_lost = extract_function(
    HF_TAG,
    "case NRFX_NFCT_EVT_FIELD_LOST:",
    "case NRFX_NFCT_EVT_TX_FRAMESTART:",
)
lf_lost = extract_function(
    LF_TAG,
    "static void lf_field_lost",
    "bool is_lf_field_exists",
)
assert hf_detect.index("rgb_marquee_request_rf_ownership(RGB_MARQUEE_RF_SOURCE_HF);") < hf_detect.index(
    "g_is_tag_emulating = true;"
)
assert lf_detect.index("rgb_marquee_request_rf_ownership(RGB_MARQUEE_RF_SOURCE_LF);") < lf_detect.index(
    "g_is_tag_emulating = true;"
)
assert "rgb_marquee_release_rf_ownership(RGB_MARQUEE_RF_SOURCE_HF);" in hf_lost
assert "rgb_marquee_release_rf_ownership(RGB_MARQUEE_RF_SOURCE_LF);" in lf_lost
assert "light_up_by_slot();" in hf_detect
assert "light_up_by_slot();" in lf_detect
assert "rgb_marquee_release_rf_ownership(RGB_MARQUEE_RF_SOURCE_HF);" in extract_function(
    HF_TAG,
    "void nfc_tag_14a_sense_switch",
    "bool is_valid_uid_size",
)
assert "rgb_marquee_release_rf_ownership(RGB_MARQUEE_RF_SOURCE_LF);" in extract_function(
    LF_TAG,
    "static void lf_sense_disable",
    "static enum",
)

assert "nrfx_pwm_stop(&pwm0_ins, false);" in RGB_MARQUEE
assert "rf_owner_mask" in RGB_MARQUEE
assert "CRITICAL_REGION_ENTER();" in RGB_MARQUEE
for writer, next_writer in (
    ("void rgb_marquee_usb_open_sweep", "void rgb_marquee_usb_open_symmetric"),
    ("void rgb_marquee_usb_open_symmetric", "void rgb_marquee_sweep_to"),
    ("void rgb_marquee_sweep_to", "volatile bool callback_waiting"),
    ("void rgb_marquee_slot_switch", "void rgb_marquee_sweep_fade"),
    ("void rgb_marquee_sweep_fade", "void rgb_marquee_sweep_from_to"),
    ("void rgb_marquee_sweep_from_to", "void rgb_marquee_usb_idle"),
    ("void rgb_marquee_symmetric_out", "void rgb_marquee_symmetric_in"),
    ("void rgb_marquee_symmetric_in", "bool rgb_marquee_is_enabled"),
):
    body = extract_function(RGB_MARQUEE, writer, next_writer)
    assert "rgb_decorative_leds_available()" in body
    assert "CRITICAL_REGION_ENTER();" in body
slot_change = extract_function(
    RFID_MAIN,
    "void apply_slot_change",
    "device_mode_t get_device_mode",
)
assert "if (rgb_marquee_rf_owns_leds())" in slot_change
assert "light_up_by_slot();" in slot_change
assert "rgb_marquee_complete_rf_handoff" in APP_MAIN
