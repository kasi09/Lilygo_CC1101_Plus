"""Real HA entities/config flow/services, with the MQTT boundary simulated."""

import asyncio
import importlib
import json
import sys
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock

import pytest
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import HomeAssistantError, Unauthorized

from custom_components.lilygo_t_embed import binary_sensor, button, event, light, sensor, switch
from custom_components.lilygo_t_embed.config_flow import LilygoConfigFlow
from custom_components.lilygo_t_embed.const import DOMAIN, VERSION
from custom_components.lilygo_t_embed.diagnostics import async_get_config_entry_diagnostics
from custom_components.lilygo_t_embed.frontend import device_overview, websocket_devices
from custom_components.lilygo_t_embed.hub import LilygoHub
from custom_components.lilygo_t_embed.services import async_register_services, async_remove_services

STATE = {
    "event": "state",
    "version": "0.4.0",
    "discovery_enabled": False,
    "session": "0123456789abcdef",
    "mode": "receive",
}


@pytest.fixture
async def device(tmp_path, monkeypatch):
    hass = HomeAssistant(str(tmp_path))
    hass.config_entries = MagicMock()
    hass.config_entries.async_entries.return_value = []
    entry = SimpleNamespace(entry_id="entry-1", title="Test LilyGO", data={"base_topic": "lilygo/test"})
    hub = LilygoHub(hass, entry)
    entry.runtime_data = hub
    hub.device_id = "device-1"
    hass.data[DOMAIN] = {entry.entry_id: hub}
    mqtt = importlib.import_module("homeassistant.components.mqtt")
    monkeypatch.setattr(mqtt, "is_connected", lambda _: True)
    monkeypatch.setattr(mqtt, "async_publish", AsyncMock())
    yield hass, entry, hub, mqtt
    await hub.close()
    await hass.async_block_till_done()


def message(hub, suffix, data, retain=False):
    hub._message(
        SimpleNamespace(
            topic=f"{hub.topic}/{suffix}",
            payload=data if isinstance(data, str) else json.dumps(data),
            retain=retain,
        )
    )


async def test_all_platforms_create_real_entities(device):
    hass, entry, hub, _ = device
    entities = []
    for platform in (sensor, binary_sensor, switch, button, light, event):
        await platform.async_setup_entry(hass, entry, entities.extend)
    assert len(entities) == 74
    assert len({entity.unique_id for entity in entities}) == 74
    assert all(entity.device_info["identifiers"] == {(DOMAIN, hub.topic)} for entity in entities)
    assert all(not entity.available for entity in entities)
    hub.protocol.receive(
        "state",
        json.dumps(
            {
                **STATE,
                "battery_available": True,
                "battery_pct": 65,
                "gauge_reachable": True,
                "microphone_enabled": False,
            }
        ),
    )
    battery = next(e for e in entities if e.key == "battery_pct")
    assert battery.available and battery.native_value == 65
    hub.protocol.state.update(battery_available=False, battery_pct=None)
    assert not battery.available and battery.native_value is None
    slot = next(e for e in entities if e.key == "rf_slot_0")
    assert not slot.available
    hub.protocol.receive("remotes", '{"slot":0,"kind":"rc","name":"AN"}')
    assert slot.available and slot.extra_state_attributes["code_name"] == "AN"


async def test_light_and_switch_use_confirmed_commands(device):
    _, _, hub, _ = device
    hub.command = AsyncMock()
    lamp = light.LilygoLight(hub)
    await lamp.async_turn_on(brightness=42, rgb_color=(1, 2, 3), effect="Statisch")
    hub.command.assert_awaited_once_with(
        "light_set", state="ON", brightness=42, color={"r": 1, "g": 2, "b": 3}, effect="Statisch"
    )
    assert not lamp.is_on
    hub.protocol.receive("light/state", '{"state":"ON","brightness":42,"color":{"r":1,"g":2,"b":3}}')
    assert lamp.is_on and lamp.brightness == 42 and lamp.rgb_color == (1, 2, 3)
    screen = switch.LilygoSwitch(hub, "screen")
    await screen.async_turn_off()
    hub.command.assert_awaited_with("screen_set", enabled=False)


async def test_adapter_retained_events_and_tag_routing(device, monkeypatch):
    hass, _, hub, _ = device
    hub.refresh_records = MagicMock()
    hub.protocol.receive("state", json.dumps(STATE))
    received = []
    hub.event_listeners.add(lambda *args: received.append(args))
    tag = importlib.import_module("homeassistant.components.tag")
    scan = AsyncMock()
    monkeypatch.setattr(tag, "async_scan_tag", scan)
    message(hub, "input", "side_click", retain=True)
    message(hub, "remote/1/event", "press", retain=True)
    assert received == []
    message(hub, "input", "side_click")
    message(hub, "remote/1/event", "press")
    message(hub, "ir/remote/7/event", "press")
    message(hub, "nfc/tag", {"event": "tag_scanned", "uid": "01020304"})
    await hass.async_block_till_done()
    assert received[0] == ("side_click", {})
    assert received[1] == ("remote_pressed", {"kind": "rf", "slot": 1})
    assert received[2] == ("remote_pressed", {"kind": "ir", "slot": 7})
    scan.assert_awaited_once_with(hass, "01020304", "device-1")
    hub._connection_status(False)
    assert not hub.available
    message(hub, "input", "side_click")
    assert len(received) == 4


async def test_mqtt_publication_is_qos_zero_and_not_retained(device):
    _, _, hub, mqtt = device
    hub.protocol.receive("state", json.dumps(STATE))
    await hub.command("status")
    args = mqtt.async_publish.await_args
    assert args.args[1] == "lilygo/test/command"
    assert args.kwargs == {"qos": 0, "retain": False}
    assert json.loads(args.args[2])["session"] == STATE["session"]


async def test_services_route_and_reject_injected_parameters(device):
    hass, _, hub, _ = device
    hub.command = AsyncMock()
    async_register_services(hass)
    await hass.services.async_call(
        DOMAIN,
        "learn_code",
        {"device_id": "device-1", "slot": 3, "name": "IR", "code_type": "ir"},
        blocking=True,
    )
    hub.command.assert_awaited_once_with("ir_learn", slot=3, name="IR")
    for fields in ({"timeout": 900}, {"session": "injected"}, {"action": "send"}):
        with pytest.raises(HomeAssistantError, match="invalid_command_fields"):
            await hass.services.async_call(
                DOMAIN,
                "execute",
                {"device_id": "device-1", "action": "status", "parameters": fields},
                blocking=True,
            )
    with pytest.raises(HomeAssistantError, match="integer_required_0_7"):
        await hass.services.async_call(
            DOMAIN, "send_code", {"device_id": "device-1", "kind": "ir", "slot": 8}, blocking=True
        )
    async_remove_services(hass)
    assert not hass.services.has_service(DOMAIN, "learn_code")


async def test_diagnostics_exclude_private_data(device):
    hass, entry, hub, _ = device
    hub.protocol.state = {
        **STATE,
        "nfc_uid": "private-tag",
        "nrf_last_payload": "secret",
        "code": "private-code",
        "mqtt_password": "password",
    }
    serialized = json.dumps(await async_get_config_entry_diagnostics(hass, entry))
    assert json.loads(serialized)["integration_version"] == VERSION
    for secret in (STATE["session"], "private-tag", "secret", "private-code", "password", hub.topic):
        assert secret not in serialized


async def test_config_form_and_offline_entry(device):
    hass, _, _, _ = device
    flow = LilygoConfigFlow()
    flow.hass = hass
    flow.context = {"source": "user"}
    flow.async_set_unique_id = AsyncMock()
    flow._abort_if_unique_id_configured = MagicMock()
    result = await flow.async_step_user()
    assert result["step_id"] == "user"
    result = await flow.async_step_user({"name": "Board", "base_topic": "bad/#", "offline_setup": True})
    assert result["errors"] == {"base_topic": "invalid_topic"}
    result = await flow.async_step_user(
        {"name": "Board", "base_topic": "lilygo/board", "offline_setup": True}
    )
    assert result["type"] == "create_entry"
    assert result["data"] == {"base_topic": "lilygo/board"}


async def test_duplicate_topic_is_rejected_by_real_config_flow(device):
    from homeassistant.data_entry_flow import AbortFlow

    hass, _, _, _ = device
    flow = LilygoConfigFlow()
    flow.hass = hass
    flow.context = {"source": "user"}
    flow.handler = DOMAIN
    flow._async_in_progress = MagicMock(return_value=[])
    hass.config_entries.async_entry_for_domain_unique_id.return_value = SimpleNamespace(source="user")
    with pytest.raises(AbortFlow, match="already_configured"):
        await flow.async_step_user({"name": "Duplicate", "base_topic": "lilygo/test", "offline_setup": True})


async def test_subscription_cleanup_and_expiry(device, monkeypatch):
    _, _, hub, mqtt = device
    unsubscribe, connection_unsubscribe = MagicMock(), MagicMock()
    registry = MagicMock()
    registry.async_get_or_create.return_value = SimpleNamespace(id="device-1")
    module = importlib.import_module("custom_components.lilygo_t_embed.hub")
    monkeypatch.setattr(module.dr, "async_get", lambda _: registry)
    monkeypatch.setattr(mqtt, "async_subscribe", AsyncMock(return_value=unsubscribe))
    monkeypatch.setattr(
        mqtt, "async_subscribe_connection_status", MagicMock(return_value=connection_unsubscribe)
    )
    await hub.start()
    mqtt.async_subscribe.assert_awaited_once_with(hub.hass, "lilygo/test/#", hub._message, qos=0)
    hub.protocol.receive("state", json.dumps(STATE))
    hub._expire(None)
    assert not hub.available
    await hub.close()
    unsubscribe.assert_called_once()
    connection_unsubscribe.assert_called_once()


async def test_entry_platform_setup_and_unload(device, monkeypatch):
    hass, entry, hub, _ = device
    component = importlib.import_module("custom_components.lilygo_t_embed")
    monkeypatch.setattr(component.mqtt, "async_wait_for_mqtt_client", AsyncMock(return_value=True))
    monkeypatch.setattr(component, "LilygoHub", lambda *_: hub)
    hub.start = AsyncMock()
    hass.config_entries.async_forward_entry_setups = AsyncMock()
    hass.config_entries.async_unload_platforms = AsyncMock(return_value=True)
    assert await component.async_setup_entry(hass, entry)
    assert hass.services.has_service(DOMAIN, "learn_code")
    assert await component.async_unload_entry(hass, entry)
    assert not hass.services.has_service(DOMAIN, "learn_code")
    assert not hass.data[DOMAIN]


async def test_learning_panel_overview_excludes_codes_and_sessions(device):
    hass, _, hub, _ = device
    hub.protocol.receive("state", json.dumps({**STATE, "storage_ok": True, "frequency_mhz": 433.92}))
    hub.protocol.receive("remotes", json.dumps({"slot": 0, "kind": "raw", "name": "Steckdose EIN",
        "frequency_mhz": 433.92, "pulse_count": 131, "pulses_us": [283, 1297], "code": "987654321"}))
    overview = device_overview(hass)
    assert len(overview) == 1 and overview[0]["available"]
    assert len(overview[0]["rf"]) == 16 and len(overview[0]["ir"]) == 8
    assert overview[0]["rf"][0]["name"] == "Steckdose EIN"
    assert overview[0]["rf"][0]["occupied"]
    assert not overview[0]["rf_loaded"]
    output = json.dumps(overview)
    for private in (STATE["session"], hub.topic, "987654321", "pulses_us"):
        assert private not in output
    connection = SimpleNamespace(user=SimpleNamespace(is_admin=True), send_result=MagicMock())
    websocket_devices(hass, connection, {"id": 12, "type": "lilygo_t_embed/devices"})
    connection.send_result.assert_called_once_with(12, overview)
    connection.user.is_admin = False
    with pytest.raises(Unauthorized):
        websocket_devices(hass, connection, {"id": 13})


async def test_learning_panel_registers_once_and_uses_local_module(monkeypatch):
    frontend = importlib.import_module("custom_components.lilygo_t_embed.frontend")
    register = AsyncMock()
    import homeassistant.components
    panel_module = SimpleNamespace(async_register_panel=register)
    monkeypatch.setitem(sys.modules, "homeassistant.components.panel_custom", panel_module)
    monkeypatch.setattr(homeassistant.components, "panel_custom", panel_module, raising=False)
    monkeypatch.setattr(frontend.websocket_api, "async_register_command", MagicMock())
    hass = SimpleNamespace(data={}, http=SimpleNamespace(async_register_static_paths=AsyncMock()))
    await frontend.async_setup_panel(hass)
    await frontend.async_setup_panel(hass)
    register.assert_awaited_once()
    hass.http.async_register_static_paths.assert_awaited_once()
    fields = register.await_args.kwargs
    assert fields["frontend_url_path"] == "lilygo-t-embed"
    assert fields["require_admin"] is True
    assert fields["config_panel_domain"] == DOMAIN
    assert fields["module_url"].startswith("/lilygo_t_embed_static/")
    assert "v=0.2.0" in fields["module_url"]
    static = hass.http.async_register_static_paths.await_args.args[0][0]
    assert (Path(static.path) / "lilygo-panel.js").is_file()


@pytest.mark.parametrize(
    "state,expected", [(STATE, {}), ({**STATE, "version": "0.3.1"}, {"base": "incompatible_firmware"})]
)
async def test_config_live_validation_and_unsubscribe(device, monkeypatch, state, expected):
    hass, _, _, mqtt = device
    unsub = MagicMock()

    async def subscribe(_hass, topic, cb, **kwargs):
        assert topic == "lilygo/test/state"
        cb(SimpleNamespace(payload=json.dumps(state), retain=True))
        asyncio.get_running_loop().call_soon(cb, SimpleNamespace(payload=json.dumps(state), retain=False))
        return unsub

    monkeypatch.setattr(mqtt, "async_subscribe", subscribe)
    monkeypatch.setattr(mqtt, "async_wait_for_mqtt_client", AsyncMock(return_value=True))
    flow = LilygoConfigFlow()
    flow.hass = hass
    assert await flow._test_connection("lilygo/test") == expected
    unsub.assert_called_once()


def test_translations_and_service_metadata():
    component = Path(__file__).parents[1] / "custom_components/lilygo_t_embed"
    en = json.loads((component / "translations/en.json").read_text(encoding="utf-8"))
    de = json.loads((component / "translations/de.json").read_text(encoding="utf-8"))
    assert en == json.loads((component / "strings.json").read_text(encoding="utf-8"))
    for platform in (sensor, binary_sensor, switch, button, light, event):
        assert platform.__name__.split(".")[-1] in de["entity"]
    assert en["services"].keys() == de["services"].keys()
    assert de["entity"]["sensor"]["gauge_temperature_c"]["name"] == "Temperatur des Akku-ICs"
