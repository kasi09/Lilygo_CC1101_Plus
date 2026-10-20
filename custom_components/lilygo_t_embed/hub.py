"""Home Assistant MQTT adapter for the gateway protocol."""

import asyncio
import json
import logging
import re
from collections.abc import Callable

from homeassistant.components import mqtt, tag
from homeassistant.core import callback
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers import device_registry as dr
from homeassistant.helpers.event import async_call_later

from .const import CONF_BASE_TOPIC, DOMAIN, INPUT_EVENTS, NAME, STATE_TIMEOUT
from .protocol import GatewayError, GatewayProtocol, command_payload

_LOGGER = logging.getLogger(__name__)


class LilygoHub:
    """Own one subscription, session and live device per config entry."""

    def __init__(self, hass, entry):
        self.hass = hass
        self.entry = entry
        self.topic = entry.data[CONF_BASE_TOPIC]
        self.listeners: set[Callable] = set()
        self.event_listeners: set[Callable] = set()
        self.protocol = GatewayProtocol(self._publish, self._changed)
        self._unsubscribe = None
        self._connection_unsubscribe = None
        self._expiry = None
        self._refresh_task = None
        self._tasks: set[asyncio.Task] = set()
        self._closed = False
        self.device_id = None

    @property
    def available(self):
        return self.protocol.available and mqtt.is_connected(self.hass)

    @property
    def device_info(self):
        return dr.DeviceInfo(
            identifiers={(DOMAIN, self.topic)},
            name=self.entry.title,
            manufacturer="LILYGO",
            model=NAME,
            sw_version=self.protocol.state.get("version"),
        )

    async def start(self):
        device = dr.async_get(self.hass).async_get_or_create(
            config_entry_id=self.entry.entry_id, **self.device_info
        )
        self.device_id = device.id
        self._unsubscribe = await mqtt.async_subscribe(self.hass, f"{self.topic}/#", self._message, qos=0)
        self._connection_unsubscribe = mqtt.async_subscribe_connection_status(
            self.hass, self._connection_status
        )

    async def close(self):
        if self._closed:
            return
        self._closed = True
        if self._unsubscribe:
            self._unsubscribe()
        if self._connection_unsubscribe:
            self._connection_unsubscribe()
        if self._expiry:
            self._expiry()
        self.protocol.disconnect()
        for task in self._tasks:
            task.cancel()
        if self._tasks:
            await asyncio.gather(*self._tasks, return_exceptions=True)

    def _task(self, coroutine):
        task = self.hass.async_create_task(coroutine)
        self._tasks.add(task)
        task.add_done_callback(self._tasks.discard)
        return task

    @callback
    def _connection_status(self, connected):
        if not connected:
            self.protocol.disconnect()

    @callback
    def _changed(self):
        for listener in tuple(self.listeners):
            listener()

    async def _publish(self, payload):
        if not mqtt.is_connected(self.hass):
            self.protocol.disconnect()
            raise GatewayError("mqtt_disconnected")
        await mqtt.async_publish(self.hass, f"{self.topic}/command", payload, qos=0, retain=False)

    @callback
    def _expire(self, _now):
        self._expiry = None
        self.protocol.disconnect()

    @callback
    def _message(self, message):
        if self._closed:
            return
        suffix = message.topic[len(self.topic) + 1 :]
        old_session = self.protocol.state.get("session")
        was_available = self.protocol.available
        self.protocol.receive(suffix, message.payload, message.retain)
        if suffix == "state" and self.protocol.available and not message.retain:
            if self._expiry:
                self._expiry()
            self._expiry = async_call_later(self.hass, STATE_TIMEOUT, self._expire)
            if not was_available or old_session != self.protocol.state.get("session"):
                dr.async_get(self.hass).async_update_device(
                    self.device_id, sw_version=self.protocol.state["version"]
                )
                self.refresh_records()
        if message.retain or not self.protocol.available:
            return
        event_type = None
        attributes = {}
        if suffix == "input" and message.payload in INPUT_EVENTS:
            event_type = message.payload
        elif re.fullmatch(r"(?:ir/)?remote/\d+/event", suffix) and message.payload == "press":
            slot = int(suffix.split("/")[-2])
            kind = "ir" if suffix.startswith("ir/") else "rf"
            if slot >= (8 if kind == "ir" else 16):
                return
            event_type, attributes = "remote_pressed", {"kind": kind, "slot": slot}
        elif suffix in ("events", "ir/events", "nfc/tag", "nrf/events", "scan", "nrf/scan"):
            try:
                data = json.loads(message.payload)
            except ValueError, TypeError:
                return
            if not isinstance(data, dict):
                return
            event = data.get("event")
            if event in ("learned", "ir_learned"):
                self.refresh_records()
            supported = {
                "learned",
                "ir_learned",
                "received",
                "ir_received",
                "tag_scanned",
                "nrf_received",
                "scan_finished",
                "nrf_energy_scan",
                "result",
            }
            if event in supported:
                # Small useful automation attributes; never session, raw RF codes or pulse arrays.
                keys = (
                    "slot",
                    "name",
                    "uid",
                    "code",
                    "bits",
                    "payload_hex",
                    "channel",
                    "length",
                    "action",
                    "ok",
                    "error",
                    "points",
                    "peak_mhz",
                    "peak_dbm",
                )
                attributes = {k: data[k] for k in keys if k in data}
                if event == "nrf_energy_scan":
                    channels = data.get("channels", [])
                    attributes["active_channels"] = [
                        c["channel"]
                        for c in channels
                        if isinstance(c, dict)
                        and c.get("energy") is True
                        and type(c.get("channel")) is int
                        and 0 <= c["channel"] <= 83
                    ]
                event_type = event
                if event == "tag_scanned":
                    uid = data.get("uid", "")
                    if isinstance(uid, str) and re.fullmatch(
                        r"(?:[A-F0-9]{8}|[A-F0-9]{14}|[A-F0-9]{20})", uid
                    ):
                        self._task(tag.async_scan_tag(self.hass, uid, self.device_id))
        if event_type:
            for listener in tuple(self.event_listeners):
                listener(event_type, attributes)
            self.hass.bus.async_fire(
                f"{DOMAIN}_event", {"device_id": self.device_id, "event_type": event_type, **attributes}
            )

    def refresh_records(self):
        if self._closed or (self._refresh_task and not self._refresh_task.done()):
            return
        self._refresh_task = self._task(self._refresh_records())

    async def _refresh_records(self):
        session = self.protocol.state.get("session")
        try:
            await self.protocol.command("list")
            await self.protocol.command("ir_list")
        except GatewayError as err:
            # Next reconnect, manual refresh or slot operation requests a new list.
            _LOGGER.debug("Code list refresh skipped: %s", err)
        finally:
            if self.protocol.available and session != self.protocol.state.get("session"):
                self._refresh_task = None
                self.refresh_records()

    async def command(self, action, **fields):
        try:
            command_payload(action, fields)
            result = await self.protocol.command(action, **fields)
        except (GatewayError, ValueError) as err:
            raise HomeAssistantError(f"LILYGO: {err}") from err
        if action in ("store", "delete", "ir_store", "ir_delete"):
            self.refresh_records()
        return result
