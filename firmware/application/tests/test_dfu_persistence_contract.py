#!/usr/bin/env python3
"""Ensure DFU preserves every FDS page used by the application."""

from pathlib import Path
import re


FIRMWARE_DIR = Path(__file__).resolve().parents[2]
APPLICATION_CONFIG = (
    FIRMWARE_DIR / "application/src/sdk_config.h"
).read_text(encoding="utf-8")
BOOTLOADER_CONFIG = (
    FIRMWARE_DIR / "bootloader/src/sdk_config.h"
).read_text(encoding="utf-8")


def integer_define(source: str, name: str) -> int:
    match = re.search(rf"#define\s+{name}\s+(\d+)", source)
    assert match is not None, f"missing integer define {name}"
    return int(match.group(1))


fds_bytes = (
    integer_define(APPLICATION_CONFIG, "FDS_VIRTUAL_PAGES")
    * integer_define(APPLICATION_CONFIG, "FDS_VIRTUAL_PAGE_SIZE")
    * 4
)
dfu_reserved_bytes = integer_define(
    BOOTLOADER_CONFIG,
    "NRF_DFU_APP_DATA_AREA_SIZE",
)

assert dfu_reserved_bytes >= fds_bytes, (
    "DFU can erase slot data, settings, or BLE bonds: "
    f"bootloader preserves {dfu_reserved_bytes} bytes, but FDS uses {fds_bytes}"
)
