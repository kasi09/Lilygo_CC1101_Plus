"""Shared push entity."""

from homeassistant.helpers.entity import Entity


class LilygoEntity(Entity):
    _attr_should_poll = False
    _attr_has_entity_name = True

    def __init__(self, hub, key):
        self.hub = hub
        self.key = key
        self._attr_unique_id = f"{hub.topic}_{key}"
        self._attr_translation_key = key

    @property
    def device_info(self):
        return self.hub.device_info

    @property
    def available(self):
        return self.hub.available

    async def async_added_to_hass(self):
        await super().async_added_to_hass()
        self.hub.listeners.add(self.async_write_ha_state)
        self.async_on_remove(lambda: self.hub.listeners.discard(self.async_write_ha_state))
