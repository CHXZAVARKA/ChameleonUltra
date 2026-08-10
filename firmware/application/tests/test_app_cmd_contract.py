#!/usr/bin/env python3
"""Build-level contract for command registration and capability advertisement."""

from pathlib import Path
import re
import struct


SOURCE_DIR = Path(__file__).resolve().parents[1] / "src"
APP_CMD = (SOURCE_DIR / "app_cmd.c").read_text(encoding="utf-8")
DATA_CMD = (SOURCE_DIR / "data_cmd.h").read_text(encoding="utf-8")


def extract_block(source: str, start_marker: str, end_marker: str) -> str:
    start = source.index(start_marker) + len(start_marker)
    end = source.index(end_marker, start)
    return source[start:end]


command_map = extract_block(
    APP_CMD,
    "static cmd_data_map_t m_data_cmd_map[] = {",
    "\n};",
)
entries = re.findall(r"\{\s*(DATA_CMD_[A-Z0-9_]+)\s*,([^\n]+)\}", command_map)
assert entries, "could not parse m_data_cmd_map"

swap_entries = [entry for entry in entries if entry[0] == "DATA_CMD_SWAP_SLOTS"]
assert len(swap_entries) == 1, "slot swap must be registered exactly once"
assert "cmd_processor_swap_slots" in swap_entries[0][1], "slot swap registration must use its processor"

defines = {
    name: int(value)
    for name, value in re.findall(
        r"#define\s+(DATA_CMD_[A-Z0-9_]+)\s+\((\d+)\)", DATA_CMD
    )
}
advertised_commands = [defines[name] for name, _ in entries if name in defines]
advertised_payload = b"".join(struct.pack(">H", command) for command in advertised_commands)
assert struct.pack(">H", 1041) in [
    advertised_payload[offset : offset + 2]
    for offset in range(0, len(advertised_payload), 2)
], "device capability payload must advertise command 1041"

capability_processor = extract_block(
    APP_CMD,
    "data_frame_tx_t *cmd_processor_get_device_capabilities(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {",
    "\n}\n",
)
assert "ARRAYLEN(m_data_cmd_map)" in capability_processor
assert "m_data_cmd_map[i].cmd" in capability_processor
assert "U16HTONS" in capability_processor
