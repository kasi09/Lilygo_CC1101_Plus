"""Automation events for physical inputs, RF, IR, NFC and scans."""

from homeassistant.components.event import EventEntity

from .const import INPUT_EVENTS
from .entity import LilygoEntity

EVENTS = {
    "input": list(INPUT_EVENTS),
    "radio": ["received", "remote_pressed", "learned", "ir_received", "ir_learned", "nrf_received"],
    "nfc_tag": ["tag_scanned"],
    "operation": ["result", "scan_finished", "nrf_energy_scan"],
}


async def async_setup_entry(hass, entry, async_add_entities):
    async_add_entities([LilygoEvent(entry.runtime_data, key, types) for key, types in EVENTS.items()])


class LilygoEvent(LilygoEntity, EventEntity):
    def __init__(self, hub, key, types):
        super().__init__(hub, key)
        self._attr_event_types = types

    async def async_added_to_hass(self):
        await super().async_added_to_hass()
        self.hub.event_listeners.add(self._event)
        self.async_on_remove(lambda: self.hub.event_listeners.discard(self._event))

    def _event(self, event_type, attributes):
        if event_type in self._attr_event_types:
            self._trigger_event(event_type, attributes)
            self.async_write_ha_state()
