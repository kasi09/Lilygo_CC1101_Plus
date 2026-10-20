"""Reported feature switches, never optimistic."""

from homeassistant.components.switch import SwitchEntity
from homeassistant.helpers.entity import EntityCategory

from .entity import LilygoEntity


async def async_setup_entry(hass, entry, async_add_entities):
    async_add_entities(
        [
            LilygoSwitch(entry.runtime_data, feature)
            for feature in ("nfc", "ir", "nrf", "microphone", "screen")
        ]
    )


class LilygoSwitch(LilygoEntity, SwitchEntity):
    _attr_entity_category = EntityCategory.CONFIG

    def __init__(self, hub, feature):
        super().__init__(hub, feature)
        self.state_key = "screen_on" if feature == "screen" else f"{feature}_enabled"

    @property
    def available(self):
        return super().available and (
            self.key in ("screen", "microphone")
            or self.hub.protocol.state.get(f"{self.key}_available") is True
        )

    @property
    def is_on(self):
        return self.hub.protocol.state.get(self.state_key)

    async def async_turn_on(self, **kwargs):
        await self._set(True)

    async def async_turn_off(self, **kwargs):
        await self._set(False)

    async def _set(self, enabled):
        if self.key == "screen":
            await self.hub.command("screen_set", enabled=enabled)
        else:
            await self.hub.command("feature_set", feature=self.key, enabled=enabled)
