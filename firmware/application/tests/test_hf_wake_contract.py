#!/usr/bin/env python3
"""Contract checks for first-field HF wake readiness."""

from pathlib import Path


SOURCE_DIR = Path(__file__).resolve().parents[1] / "src"
APP_MAIN = (SOURCE_DIR / "app_main.c").read_text(encoding="utf-8")
RFID_MAIN = (SOURCE_DIR / "rfid_main.c").read_text(encoding="utf-8")


field_wake_start = APP_MAIN.index(
    "} else if ((m_reset_source & (NRF_POWER_RESETREAS_NFC_MASK |"
)
field_wake_end = APP_MAIN.index(
    "} else if (m_reset_source & NRF_POWER_RESETREAS_VBUS_MASK)",
    field_wake_start,
)
field_wake = APP_MAIN[field_wake_start:field_wake_end]

# A terminal can keep its field continuously present while the device boots.
# Emulation must therefore be ready before any decorative, blocking LED work.
assert "tag_mode_enter();" in field_wake
assert field_wake.index("tag_mode_enter();") < field_wake.index(
    "TAG_FIELD_LED_ON();"
)

tag_mode_start = RFID_MAIN.index("void tag_mode_enter(void)")
tag_mode_end = RFID_MAIN.index("\n}\n", tag_mode_start) + 2
tag_mode = RFID_MAIN[tag_mode_start:tag_mode_end]

# The 60 ms antenna-settle delay is required when leaving reader mode, but a
# cold boot starts in DEVICE_MODE_NONE and must not pay that delay.
assert "bool leaving_reader_mode = rfid_state == DEVICE_MODE_READER;" in tag_mode
delay_guard = tag_mode.index("if (leaving_reader_mode)")
delay = tag_mode.index("bsp_delay_ms(60)")
assert delay_guard < delay

print("HF wake readiness contract: OK")
