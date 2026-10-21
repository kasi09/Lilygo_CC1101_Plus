"""Register the local learning panel and its read-only device overview."""

from pathlib import Path

from homeassistant.components import websocket_api
from homeassistant.components.http import StaticPathConfig
from homeassistant.core import callback

from .const import DOMAIN, VERSION

PANEL_DATA = f"{DOMAIN}_panel"
PANEL_PATH = "lilygo-t-embed"
STATIC_PATH = "/lilygo_t_embed_static"


async def async_setup_panel(hass):
    """Install one sidebar entry, without YAML or dashboard resources."""
    if hass.data.get(PANEL_DATA):
        return
    from homeassistant.components import panel_custom

    await hass.http.async_register_static_paths(
        [StaticPathConfig(STATIC_PATH, str(Path(__file__).parent / "frontend"), True)]
    )
    await panel_custom.async_register_panel(
        hass,
        frontend_url_path=PANEL_PATH,
        webcomponent_name="lilygo-learn-panel",
        sidebar_title="LILYGO",
        sidebar_icon="mdi:access-point",
        module_url=f"{STATIC_PATH}/lilygo-panel.js?v={VERSION}",
        require_admin=True,
        config_panel_domain=DOMAIN,
    )
    websocket_api.async_register_command(hass, websocket_devices)
    hass.data[PANEL_DATA] = True


def device_overview(hass):
    """Expose slot labels and readiness, without sessions or captured codes."""
    result = []
    for hub in hass.data.get(DOMAIN, {}).values():
        state = hub.protocol.state
        rf = []
        ir = []
        for slot in range(16):
            record = hub.protocol.remotes.get(slot, {})
            kind = record.get("kind", "unknown")
            rf.append(
                {"slot": slot, "name": record.get("name", ""), "kind": kind,
                 "occupied": kind in ("rc", "raw"),
                 "frequency_mhz": record.get("frequency_mhz"),
                 "pulse_count": record.get("pulse_count")}
            )
        for slot in range(8):
            record = hub.protocol.ir_remotes.get(slot, {})
            ir.append({"slot": slot, "name": record.get("name", ""),
                       "occupied": record.get("occupied", False), "kind": "ir"})
        result.append(
            {"device_id": hub.device_id, "name": hub.entry.title, "available": hub.available,
             "version": state.get("version"), "mode": state.get("mode"),
             "frequency_mhz": state.get("frequency_mhz"), "storage_ok": state.get("storage_ok", False),
             "ir_available": state.get("ir_available", False),
             "rf_loaded": len(hub.protocol.remotes) == 16,
             "ir_loaded": len(hub.protocol.ir_remotes) == 8, "rf": rf, "ir": ir}
        )
    return result


@websocket_api.websocket_command({"type": f"{DOMAIN}/devices"})
@websocket_api.require_admin
@callback
def websocket_devices(hass, connection, msg):
    """Learning configuration follows the administrator-only sidebar access."""
    connection.send_result(msg["id"], device_overview(hass))
