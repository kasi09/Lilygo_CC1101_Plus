"""MQTT protocol and command delivery, independent of Home Assistant.

Commands are never retained, queued across reconnects, or retried. A result must
match the request ID; unsolicited learning timeouts cannot acknowledge a send.
"""

import asyncio
import json
import math
import re
import time
from collections.abc import Awaitable, Callable
from typing import Any
from uuid import uuid4

from .const import COMMAND_TIMEOUT, MIN_FIRMWARE, STATE_TIMEOUT


class GatewayError(Exception):
    """A command failed or cannot safely be delivered."""


def base_topic(value: str) -> str:
    """Reject wildcards, empty levels and control characters."""
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9_-]+(?:/[A-Za-z0-9_-]+)+", value):
        raise ValueError("invalid_topic")
    if len(value) > 160:
        raise ValueError("invalid_topic")
    return value


def compatible(state: dict) -> bool:
    """Only the correlated protocol with discovery explicitly disabled is safe."""
    version = state.get("version", "")
    match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", version) if isinstance(version, str) else None
    return bool(
        match
        and tuple(map(int, match.groups())) >= MIN_FIRMWARE
        and state.get("discovery_enabled") is False
        and re.fullmatch(r"[a-f0-9]{16}", str(state.get("session", "")))
    )


def integer(value: Any, lower: int, upper: int) -> int:
    if type(value) is not int or not lower <= value <= upper:
        raise ValueError(f"integer_required_{lower}_{upper}")
    return value


def frequency(value: Any) -> float:
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError("invalid_frequency")
    if not any(lo <= value <= hi for lo, hi in ((300, 348), (387, 464), (779, 928))):
        raise ValueError("invalid_frequency")
    return float(value)


def command_payload(action: str, fields: dict) -> dict:
    """Bound user inputs before passing them to the firmware's final validator."""
    allowed = {
        "status": set(),
        "list": set(),
        "ir_list": set(),
        "stop": set(),
        "send": {"slot", "repeats"},
        "ir_send": {"slot", "repeats"},
        "learn": {"slot", "kind", "name"},
        "ir_learn": {"slot", "name"},
        "delete": {"slot"},
        "ir_delete": {"slot"},
        "tune": {"frequency_mhz"},
        "scan": {"band", "start_mhz", "end_mhz", "step_mhz"},
        "feature_set": {"feature", "enabled"},
        "screen_set": {"enabled"},
        "light_set": {"state", "brightness", "color", "effect"},
        "beep": {"frequency_hz", "duration_ms", "volume"},
        "probe_features": set(),
        "backup": set(),
        "nrf_scan": set(),
        "nrf_config": {"channel", "address_hex"},
        "nrf_send": {"payload_hex"},
        "store": {
            "slot",
            "name",
            "kind",
            "frequency_mhz",
            "code",
            "bits",
            "protocol",
            "pulse_us",
            "first_high",
            "gap_us",
            "pulses_us",
        },
        "ir_store": {"slot", "name", "decoded", "protocol", "code_hex", "bits", "carrier_khz", "pulses_us"},
    }
    if action not in allowed or not isinstance(fields, dict) or fields.keys() - allowed[action]:
        raise ValueError("invalid_command_fields")
    fields = dict(fields)
    if "slot" in allowed[action]:
        integer(fields.get("slot"), 0, 7 if action.startswith("ir_") else 15)
    if "name" in fields:
        if not isinstance(fields["name"], str) or len(fields["name"].encode("utf-8")) > 32:
            raise ValueError("invalid_name")
    if "repeats" in fields:
        integer(fields["repeats"], 0 if action == "ir_send" else 1, 3 if action == "ir_send" else 10)
    if action == "tune":
        frequency(fields.get("frequency_mhz"))
    if action == "learn" and fields.get("kind", "rc") not in ("rc", "raw"):
        raise ValueError("invalid_kind")
    if action == "scan":
        if "band" in fields:
            if len(fields) != 1 or fields["band"] not in ("433", "868", "all"):
                raise ValueError("invalid_band")
        else:
            lo, hi = frequency(fields.get("start_mhz")), frequency(fields.get("end_mhz"))
            step = fields.get("step_mhz")
            if type(step) not in (float, int) or not math.isfinite(step) or not 0.025 <= step <= 2:
                raise ValueError("invalid_step")
            if hi < lo or not any(a <= lo <= hi <= b for a, b in ((300, 348), (387, 464), (779, 928))):
                raise ValueError("invalid_scan_range")
            if math.floor((hi - lo + 0.0001) / step) + 1 > 2001:
                raise ValueError("too_many_points")
    if action in ("feature_set", "screen_set") and type(fields.get("enabled")) is not bool:
        raise ValueError("invalid_enabled")
    if action == "feature_set" and fields.get("feature") not in ("nfc", "ir", "nrf", "microphone"):
        raise ValueError("invalid_feature")
    if action == "nrf_config":
        integer(fields.get("channel"), 0, 83)
        if not re.fullmatch(r"[a-fA-F0-9]{10}", str(fields.get("address_hex", ""))):
            raise ValueError("invalid_address")
    if action == "nrf_send" and not re.fullmatch(
        r"(?:[a-fA-F0-9]{2}){1,32}", str(fields.get("payload_hex", ""))
    ):
        raise ValueError("invalid_payload")
    for key, lo, hi in (("frequency_hz", 200, 4000), ("duration_ms", 20, 300), ("volume", 0, 50)):
        if key in fields:
            integer(fields[key], lo, hi)
    # Detailed RC/RAW/IR record and light validation remains authoritative on board.
    payload = {"action": action, **fields}
    if len(json.dumps(payload).encode()) > 3900:
        raise ValueError("command_too_large")
    return payload


class GatewayProtocol:
    """A live session and exactly one in-flight correlated command."""

    def __init__(
        self,
        publish: Callable[[str], Awaitable[None]],
        changed: Callable[[], None],
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        self.publish = publish
        self.changed = changed
        self.clock = clock
        self.state: dict = {}
        self.light: dict = {}
        self.remotes: dict[int, dict] = {}
        self.ir_remotes: dict[int, dict] = {}
        self.scan: dict = {}
        self.last_state: float | None = None
        self.online = False
        self._lock = asyncio.Lock()
        self._pending: tuple[str, str, asyncio.Future] | None = None
        self._uncertain = False
        self._scan_notify_at = -1.0

    @property
    def available(self) -> bool:
        return bool(
            self.online
            and self.last_state is not None
            and self.clock() - self.last_state < STATE_TIMEOUT
            and compatible(self.state)
        )

    def disconnect(self) -> None:
        self.online = False
        self.last_state = None
        self.remotes.clear()
        self.ir_remotes.clear()
        self._fail_pending("disconnected")
        self.changed()

    def _fail_pending(self, error: str) -> None:
        if self._pending and not self._pending[2].done():
            self._pending[2].set_exception(GatewayError(error))

    def receive(self, suffix: str, payload: str, retained: bool = False) -> None:
        if suffix == "availability":
            if payload == "offline":
                self.disconnect()
            # Retained 'online' alone never establishes a usable session.
            return
        if retained and suffix != "light/state":
            return
        try:
            data = json.loads(payload)
        except ValueError, TypeError:
            return
        if not isinstance(data, dict):
            return
        if suffix == "state":
            old_session = self.state.get("session")
            if data.get("event") != "state" or not compatible(data):
                self.state = data
                self.disconnect()
                return
            if old_session != data["session"]:
                self._fail_pending("session_changed")
                self.remotes.clear()
                self.ir_remotes.clear()
            self._uncertain = False
            self.state = data
            self.last_state = self.clock()
            self.online = True
        elif suffix == "light/state":
            self.light = data
        elif not self.available:
            return
        elif suffix in ("remotes", "ir/remotes"):
            count = 8 if suffix == "ir/remotes" else 16
            slot = data.get("slot")
            if type(slot) is int and 0 <= slot < count:
                records = self.ir_remotes if count == 8 else self.remotes
                records[slot] = data
        elif suffix == "scan":
            # Do not accumulate raw samples in entity attributes or recorder.
            self.scan = {
                k: v
                for k, v in data.items()
                if k in ("event", "frequency_mhz", "rssi_dbm", "points", "peak_mhz", "peak_dbm", "ok")
            }
            if data.get("event") == "scan_sample":
                if self.clock() - self._scan_notify_at < 0.25:
                    return
                self._scan_notify_at = self.clock()
        elif suffix == "events" and data.get("event") == "result":
            if self._pending:
                request_id, action, future = self._pending
                if (
                    data.get("request_id") == request_id
                    and data.get("action") in (action, "mqtt", "parse")
                    and not future.done()
                ):
                    if data.get("ok") is True:
                        future.set_result(data)
                    else:
                        future.set_exception(GatewayError(str(data.get("error", "command_failed"))))
        elif suffix == "events" and data.get("event") in ("learned", "ir_learned"):
            is_ir = data["event"] == "ir_learned"
            slot = data.get("slot")
            if type(slot) is int and 0 <= slot < (8 if is_ir else 16):
                (self.ir_remotes if is_ir else self.remotes)[slot] = data
        self.changed()

    async def command(self, action: str, timeout: float = COMMAND_TIMEOUT, **fields: Any) -> dict:
        payload = command_payload(action, fields)
        # A second invocation while busy is rejected, never queued for later TX.
        if self._lock.locked():
            raise GatewayError("command_in_progress")
        async with self._lock:
            if not self.available or self._uncertain:
                raise GatewayError("no_fresh_session")
            request_id = uuid4().hex
            payload.update(session=self.state["session"], request_id=request_id)
            no_ack = action in ("status", "list")
            future = asyncio.get_running_loop().create_future()
            self._pending = request_id, action, future
            try:
                await self.publish(json.dumps(payload, separators=(",", ":")))
                if no_ack:
                    return {"ok": True, "delivery": "published"}
                async with asyncio.timeout(timeout):
                    return await future
            except TimeoutError as err:
                self._uncertain = True
                raise GatewayError("confirmation_timeout_no_retry") from err
            finally:
                self._pending = None
                if not future.done():
                    future.cancel()
                elif not future.cancelled():
                    # A disconnect during a no-ACK publish may complete the
                    # future before publish returns. Consume that exception.
                    future.exception()
