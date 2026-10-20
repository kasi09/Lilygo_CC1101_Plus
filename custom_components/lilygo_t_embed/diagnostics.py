"""Whitelisted diagnostics without codes, UID, session or broker credentials."""


async def async_get_config_entry_diagnostics(hass, entry):
    hub = entry.runtime_data
    safe_keys = (
        "version",
        "mode",
        "discovery_enabled",
        "uptime_s",
        "free_heap_bytes",
        "storage_ok",
        "radio_error",
        "rx_count",
        "tx_count",
        "saved",
        "battery_available",
        "gauge_reachable",
        "current_available",
        "chip_temperature_available",
        "charger_available",
        "nfc_available",
        "ir_available",
        "nrf_available",
        "sd_mounted",
        "noise_available",
    )
    return {
        "integration_version": "0.1.0",
        "available": hub.available,
        "state": {key: hub.protocol.state[key] for key in safe_keys if key in hub.protocol.state},
    }
