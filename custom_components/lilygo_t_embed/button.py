"""Actions and send buttons for persistent code slots."""

from homeassistant.components.button import ButtonEntity

from .entity import LilygoEntity

BUTTONS = (
    ("scan_433", "scan", {"band": "433"}),
    ("scan_868", "scan", {"band": "868"}),
    ("scan_all", "scan", {"band": "all"}),
    ("stop", "stop", {}),
    ("beep", "beep", {}),
    ("backup", "backup", {}),
    ("probe_features", "probe_features", {}),
    ("nrf_scan", "nrf_scan", {}),
    ("refresh", "status", {}),
)


async def async_setup_entry(hass, entry, async_add_entities):
    hub = entry.runtime_data
    async_add_entities(
        [LilygoButton(hub, *item) for item in BUTTONS]
        + [
            LilygoSlotButton(hub, kind, slot)
            for kind, count in (("rf", 16), ("ir", 8))
            for slot in range(count)
        ]
    )


class LilygoButton(LilygoEntity, ButtonEntity):
    def __init__(self, hub, key, action, fields):
        super().__init__(hub, key)
        self.action, self.fields = action, fields

    async def async_press(self):
        await self.hub.command(self.action, **self.fields)
        if self.key == "refresh":
            self.hub.refresh_records()


class LilygoSlotButton(LilygoButton):
    def __init__(self, hub, kind, slot):
        super().__init__(hub, f"{kind}_slot_{slot}", "send" if kind == "rf" else "ir_send", {"slot": slot})
        self.kind, self.slot = kind, slot
        self._attr_translation_key = "send_slot"
        self._attr_translation_placeholders = {"kind": kind.upper(), "slot": str(slot)}

    @property
    def record(self):
        records = self.hub.protocol.remotes if self.kind == "rf" else self.hub.protocol.ir_remotes
        return records.get(self.slot, {})

    @property
    def available(self):
        occupied = (
            self.record.get("kind") in ("rc", "raw")
            if self.kind == "rf"
            else self.record.get("occupied") is True
        )
        return super().available and occupied

    @property
    def extra_state_attributes(self):
        return {"kind": self.kind, "slot": self.slot, "code_name": self.record.get("name", "")}
