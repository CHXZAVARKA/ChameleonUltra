#!/usr/bin/env python3
"""Ensure long Nordic UART responses yield back to the BLE event loop."""

from pathlib import Path
import re


SOURCE_DIR = Path(__file__).resolve().parents[1] / "src"
BLE_MAIN = (SOURCE_DIR / "ble_main.c").read_text(encoding="utf-8")
APP_CMD = (SOURCE_DIR / "app_cmd.c").read_text(encoding="utf-8")


command_map = re.search(
    r"static\s+cmd_data_map_t\s+m_data_cmd_map\[\]\s*=\s*\{(?P<body>.*?)\n\};",
    APP_CMD,
    re.DOTALL,
)
assert command_map is not None, "missing device command map"
command_count = len(re.findall(r"\{\s*DATA_CMD_", command_map.group("body")))
assert command_count * 2 > 20, (
    "the capability response must exercise more than the default BLE payload"
)

response_match = re.search(
    r"void\s+nus_data_response\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
    BLE_MAIN,
    re.DOTALL,
)
assert response_match is not None, "missing Nordic UART response entry point"
response_body = response_match.group("body")

assert "memcpy(m_ble_nus_tx_buffer" in response_body, (
    "a response must outlive the command handler while BLE notifications drain"
)
assert "nus_tx_continue();" in response_body, (
    "the response entry point must queue the first notification"
)
assert "do {" not in response_body and "while (count != length" not in response_body, (
    "the command handler must not spin while the BLE notification queue is full"
)

assert "case BLE_GATTS_EVT_HVN_TX_COMPLETE:" in BLE_MAIN, (
    "queued response chunks must resume when a notification is acknowledged"
)
assert re.search(
    r"case\s+BLE_GATTS_EVT_HVN_TX_COMPLETE:\s*nus_tx_continue\(\);",
    BLE_MAIN,
), "the BLE TX-complete event must resume the pending response"
assert re.search(
    r"err_code\s*==\s*NRF_ERROR_RESOURCES.*?return;",
    BLE_MAIN,
    re.DOTALL,
), "a full BLE queue must return control to the SoftDevice event loop"
