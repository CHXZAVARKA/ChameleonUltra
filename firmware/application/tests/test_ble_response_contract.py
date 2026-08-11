#!/usr/bin/env python3
"""Ensure multi-packet Nordic UART responses yield back to the BLE stack."""

from pathlib import Path
import re


SOURCE = (
    Path(__file__).resolve().parents[1] / "src/ble_main.c"
).read_text(encoding="utf-8")


response_match = re.search(
    r"void\s+nus_data_response\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
    SOURCE,
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

assert "case BLE_GATTS_EVT_HVN_TX_COMPLETE:" in SOURCE, (
    "queued response chunks must resume when a notification is acknowledged"
)
assert re.search(
    r"case\s+BLE_GATTS_EVT_HVN_TX_COMPLETE:\s*nus_tx_continue\(\);",
    SOURCE,
), "the BLE TX-complete event must resume the pending response"
assert re.search(
    r"err_code\s*==\s*NRF_ERROR_RESOURCES.*?return;",
    SOURCE,
    re.DOTALL,
), "a full BLE queue must return control to the SoftDevice event loop"
