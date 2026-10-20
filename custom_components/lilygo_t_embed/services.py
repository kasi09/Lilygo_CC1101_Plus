"""Device-targeted actions; every mutation uses the live session protocol."""

import probatio as vol
from homeassistant.core import callback
from homeassistant.exceptions import HomeAssistantError

from .const import DOMAIN
from .protocol import command_payload

SERVICES = ("learn_code", "send_code", "delete_code", "scan", "tune", "store_code", "execute")


@callback
def async_register_services(hass):
    if hass.services.has_service(DOMAIN, "learn_code"):
        return
    common = {vol.Required("device_id"): str}
    schemas = {
        "learn_code": {
            vol.Required("slot"): int,
            vol.Required("name"): str,
            vol.Optional("code_type", default="rc"): vol.In(("rc", "raw", "ir")),
        },
        "send_code": {
            vol.Required("slot"): int,
            vol.Optional("kind", default="rf"): vol.In(("rf", "ir")),
            vol.Optional("repeats"): int,
        },
        "delete_code": {vol.Required("slot"): int, vol.Optional("kind", default="rf"): vol.In(("rf", "ir"))},
        "scan": {
            vol.Optional("band"): vol.In(("433", "868", "all")),
            vol.Optional("start_mhz"): vol.Coerce(float),
            vol.Optional("end_mhz"): vol.Coerce(float),
            vol.Optional("step_mhz"): vol.Coerce(float),
        },
        "tune": {vol.Required("frequency_mhz"): vol.Coerce(float)},
        "store_code": {
            vol.Optional("kind", default="rf"): vol.In(("rf", "ir")),
            vol.Required("record"): dict,
        },
        "execute": {vol.Required("action"): str, vol.Optional("parameters", default=dict): dict},
    }

    async def handle(call):
        data = dict(call.data)
        device_id = data.pop("device_id")
        hub = next((item for item in hass.data.get(DOMAIN, {}).values() if item.device_id == device_id), None)
        if hub is None:
            raise HomeAssistantError("LILYGO: device is not loaded")
        if call.service == "execute":
            action, fields = data["action"], data["parameters"]
        elif call.service == "learn_code":
            code_type = data.pop("code_type")
            action, fields = ("ir_learn" if code_type == "ir" else "learn"), data
            if code_type != "ir":
                fields["kind"] = code_type
        elif call.service in ("send_code", "delete_code"):
            kind = data.pop("kind")
            action = ("ir_" if kind == "ir" else "") + ("send" if call.service == "send_code" else "delete")
            fields = data
        elif call.service == "store_code":
            action = "ir_store" if data["kind"] == "ir" else "store"
            fields = dict(data["record"])
            for key in ("event", "action", "occupied", "pulse_count"):
                fields.pop(key, None)
        else:
            action, fields = call.service, data
        try:
            command_payload(action, fields)
        except ValueError as err:
            raise HomeAssistantError(f"LILYGO: {err}") from err
        await hub.command(action, **fields)

    for name in SERVICES:
        hass.services.async_register(DOMAIN, name, handle, schema=vol.Schema({**common, **schemas[name]}))


@callback
def async_remove_services(hass):
    for name in SERVICES:
        hass.services.async_remove(DOMAIN, name)
