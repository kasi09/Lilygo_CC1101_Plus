"""Board health and charger binary values."""

from homeassistant.components.binary_sensor import BinarySensorDeviceClass, BinarySensorEntity
from homeassistant.helpers.entity import EntityCategory

from .entity import LilygoEntity

BINARY = (
    ("usb_power", BinarySensorDeviceClass.POWER, "charger_available"),
    ("charging", BinarySensorDeviceClass.BATTERY_CHARGING, "charger_available"),
    ("storage_ok", None, None),
    ("gauge_reachable", None, None),
    ("nfc_available", None, None),
    ("ir_available", None, None),
    ("nrf_available", None, None),
    ("sd_mounted", None, None),
)


async def async_setup_entry(hass, entry, async_add_entities):
    async_add_entities([LilygoBinarySensor(entry.runtime_data, *item) for item in BINARY])


class LilygoBinarySensor(LilygoEntity, BinarySensorEntity):
    _attr_entity_category = EntityCategory.DIAGNOSTIC

    def __init__(self, hub, key, device_class, flag):
        super().__init__(hub, key)
        self.flag = flag
        self._attr_device_class = device_class

    @property
    def available(self):
        return super().available and (not self.flag or self.hub.protocol.state.get(self.flag) is True)

    @property
    def is_on(self):
        value = self.hub.protocol.state.get(self.key)
        return value if isinstance(value, bool) else None
