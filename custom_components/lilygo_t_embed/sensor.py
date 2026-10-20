"""Measurements and diagnostics. Missing hardware values stay unavailable."""

from homeassistant.components.sensor import SensorDeviceClass, SensorEntity, SensorStateClass
from homeassistant.helpers.entity import EntityCategory

from .entity import LilygoEntity

# key, unit, device class, availability flag. Temperature is the gauge IC, not ambient.
SENSORS = (
    ("battery_pct", "%", SensorDeviceClass.BATTERY, "battery_available"),
    ("battery_voltage_v", "V", SensorDeviceClass.VOLTAGE, "battery_available"),
    ("battery_current_ma", "mA", SensorDeviceClass.CURRENT, "current_available"),
    ("gauge_temperature_c", "°C", SensorDeviceClass.TEMPERATURE, "chip_temperature_available"),
    ("frequency_mhz", "MHz", SensorDeviceClass.FREQUENCY, None),
    ("rssi_dbm", "dBm", SensorDeviceClass.SIGNAL_STRENGTH, None),
    ("wifi_rssi_dbm", "dBm", SensorDeviceClass.SIGNAL_STRENGTH, None),
    ("rx_count", None, None, None),
    ("tx_count", None, None, None),
    ("saved", None, None, None),
    ("mode", None, None, None),
    ("uptime_s", "s", SensorDeviceClass.DURATION, None),
    ("free_heap_bytes", "B", SensorDeviceClass.DATA_SIZE, None),
    ("charge_status", None, None, "charger_available"),
    ("noise_dbfs", "dBFS", None, "noise_available"),
    ("nfc_count", None, None, "nfc_available"),
    ("ir_count", None, None, "ir_available"),
    ("nrf_count", None, None, "nrf_available"),
    ("nrf_event_dropped", None, None, "nrf_available"),
    ("nrf_channel", None, None, "nrf_available"),
    ("sd_total_mb", "MiB", SensorDeviceClass.DATA_SIZE, "sd_mounted"),
    ("sd_used_mb", "MiB", SensorDeviceClass.DATA_SIZE, "sd_mounted"),
)


async def async_setup_entry(hass, entry, async_add_entities):
    async_add_entities(
        [LilygoSensor(entry.runtime_data, *description) for description in SENSORS]
        + [LilygoScanSensor(entry.runtime_data)]
    )


class LilygoSensor(LilygoEntity, SensorEntity):
    _attr_entity_category = EntityCategory.DIAGNOSTIC

    def __init__(self, hub, key, unit, device_class, flag):
        super().__init__(hub, key)
        self.flag = flag
        self._attr_native_unit_of_measurement = unit
        self._attr_device_class = device_class
        if unit:
            self._attr_state_class = SensorStateClass.MEASUREMENT
        if key in ("rx_count", "tx_count", "nfc_count", "ir_count", "nrf_count", "nrf_event_dropped"):
            self._attr_state_class = SensorStateClass.TOTAL_INCREASING

    @property
    def available(self):
        return (
            super().available
            and (not self.flag or self.hub.protocol.state.get(self.flag) is True)
            and self.hub.protocol.state.get(self.key) is not None
        )

    @property
    def native_value(self):
        return self.hub.protocol.state.get(self.key)


class LilygoScanSensor(LilygoEntity, SensorEntity):
    def __init__(self, hub):
        super().__init__(hub, "scan")

    @property
    def native_value(self):
        return self.hub.protocol.scan.get("event", "idle")

    @property
    def extra_state_attributes(self):
        return {k: v for k, v in self.hub.protocol.scan.items() if k != "event"}
