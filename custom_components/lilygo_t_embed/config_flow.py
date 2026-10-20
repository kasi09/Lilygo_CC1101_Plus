"""Configure a board using Home Assistant's existing MQTT broker."""

import asyncio
import json

import probatio as vol
from homeassistant import config_entries
from homeassistant.components import mqtt
from homeassistant.const import CONF_NAME
from homeassistant.core import callback

from .const import CONF_BASE_TOPIC, CONF_OFFLINE, DOMAIN, NAME
from .protocol import base_topic, compatible


class LilygoConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    VERSION = 1

    async def async_step_user(self, user_input=None):
        errors = {}
        if user_input is not None:
            try:
                topic = base_topic(user_input[CONF_BASE_TOPIC])
            except ValueError:
                errors[CONF_BASE_TOPIC] = "invalid_topic"
            else:
                await self.async_set_unique_id(topic)
                self._abort_if_unique_id_configured()
                if not user_input.get(CONF_OFFLINE, False):
                    errors = await self._test_connection(topic)
                if not errors:
                    return self.async_create_entry(title=user_input[CONF_NAME], data={CONF_BASE_TOPIC: topic})
        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema(
                {
                    vol.Required(CONF_NAME, default=(user_input or {}).get(CONF_NAME, NAME)): str,
                    vol.Required(
                        CONF_BASE_TOPIC, default=(user_input or {}).get(CONF_BASE_TOPIC, "lilygo/lilygo_rf")
                    ): str,
                    vol.Optional(CONF_OFFLINE, default=False): bool,
                }
            ),
            errors=errors,
        )

    async def _test_connection(self, topic):
        if not await mqtt.async_wait_for_mqtt_client(self.hass):
            return {"base": "mqtt_unavailable"}
        received = asyncio.get_running_loop().create_future()

        @callback
        def message(msg):
            if msg.retain or received.done():
                return
            try:
                data = json.loads(msg.payload)
            except ValueError, TypeError:
                return
            if isinstance(data, dict) and data.get("event") == "state":
                received.set_result(data)

        unsubscribe = await mqtt.async_subscribe(self.hass, f"{topic}/state", message, qos=0)
        try:
            async with asyncio.timeout(25):
                state = await received
        except TimeoutError:
            return {"base": "cannot_connect"}
        finally:
            unsubscribe()
        if not compatible(state):
            return {"base": "incompatible_firmware"}
        return {}
