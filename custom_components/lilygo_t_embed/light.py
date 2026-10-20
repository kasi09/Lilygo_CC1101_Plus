"""RGB ring with firmware-reported state and status effect."""

from homeassistant.components.light import (
    ATTR_BRIGHTNESS,
    ATTR_EFFECT,
    ATTR_RGB_COLOR,
    ColorMode,
    LightEntity,
    LightEntityFeature,
)

from .entity import LilygoEntity


async def async_setup_entry(hass, entry, async_add_entities):
    async_add_entities([LilygoLight(entry.runtime_data)])


class LilygoLight(LilygoEntity, LightEntity):
    _attr_supported_color_modes = {ColorMode.RGB}
    _attr_color_mode = ColorMode.RGB
    _attr_supported_features = LightEntityFeature.EFFECT
    _attr_effect_list = ["Statisch", "Status"]

    def __init__(self, hub):
        super().__init__(hub, "rgb")

    @property
    def is_on(self):
        return self.hub.protocol.light.get("state") == "ON"

    @property
    def brightness(self):
        return self.hub.protocol.light.get("brightness")

    @property
    def rgb_color(self):
        color = self.hub.protocol.light.get("color", {})
        return tuple(color.get(key, 0) for key in ("r", "g", "b"))

    @property
    def effect(self):
        return self.hub.protocol.light.get("effect")

    async def async_turn_on(self, **kwargs):
        fields = {"state": "ON"}
        if ATTR_BRIGHTNESS in kwargs:
            fields["brightness"] = kwargs[ATTR_BRIGHTNESS]
        if ATTR_RGB_COLOR in kwargs:
            fields["color"] = dict(zip(("r", "g", "b"), kwargs[ATTR_RGB_COLOR], strict=True))
        if ATTR_EFFECT in kwargs:
            fields["effect"] = kwargs[ATTR_EFFECT]
        await self.hub.command("light_set", **fields)

    async def async_turn_off(self, **kwargs):
        await self.hub.command("light_set", state="OFF")
