"""LILYGO T-Embed CC1101 Plus custom integration."""

from homeassistant.components import mqtt
from homeassistant.exceptions import ConfigEntryNotReady

from .const import DOMAIN, PLATFORMS
from .frontend import async_setup_panel
from .hub import LilygoHub
from .services import async_register_services, async_remove_services


async def async_setup(hass, config):
    await async_setup_panel(hass)
    return True


async def async_setup_entry(hass, entry):
    if not await mqtt.async_wait_for_mqtt_client(hass):
        raise ConfigEntryNotReady("Home Assistant MQTT is not available")
    hub = LilygoHub(hass, entry)
    entry.runtime_data = hub
    try:
        await hub.start()
        await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    except Exception:
        await hub.close()
        raise
    hass.data.setdefault(DOMAIN, {})[entry.entry_id] = hub
    async_register_services(hass)
    return True


async def async_unload_entry(hass, entry):
    if not await hass.config_entries.async_unload_platforms(entry, PLATFORMS):
        return False
    await entry.runtime_data.close()
    hass.data[DOMAIN].pop(entry.entry_id, None)
    if not hass.data[DOMAIN]:
        async_remove_services(hass)
    return True
