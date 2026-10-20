"""Exercise actual delivery logic without requiring a running broker or HA."""

import asyncio
import importlib
import json
import sys
import types
from pathlib import Path

import pytest

# Load the real transport module without importing the HA entry-point package.
package = types.ModuleType("lilygo_protocol_test")
package.__path__ = [str(Path(__file__).parents[1] / "custom_components/lilygo_t_embed")]
sys.modules[package.__name__] = package
p = importlib.import_module(f"{package.__name__}.protocol")

STATE = {
    "event": "state",
    "version": "0.4.0",
    "discovery_enabled": False,
    "session": "0123456789abcdef",
    "battery_available": False,
}


def live(protocol, **updates):
    protocol.receive("state", json.dumps({**STATE, **updates}))


@pytest.fixture
def gateway():
    sent = []

    async def publish(payload):
        sent.append(json.loads(payload))

    protocol = p.GatewayProtocol(publish, lambda: None)
    return protocol, sent


@pytest.mark.parametrize(
    "topic",
    ["", "lilygo", "/lilygo/a", "lilygo/a/", "lilygo//a", "lilygo/+", "lilygo/#", "lilygo/a\n", "lilygo/a b"],
)
def test_invalid_topics(topic):
    with pytest.raises(ValueError):
        p.base_topic(topic)


def test_valid_topic_and_firmware():
    assert p.base_topic("lilygo/gateway-2") == "lilygo/gateway-2"
    assert p.compatible(STATE)
    assert not p.compatible({**STATE, "version": "0.3.1"})
    assert not p.compatible({**STATE, "discovery_enabled": True})
    assert not p.compatible({**STATE, "session": ""})


async def test_no_live_session_no_publish(gateway):
    protocol, sent = gateway
    protocol.receive("availability", "online", retained=True)
    protocol.receive("state", json.dumps(STATE), retained=True)
    with pytest.raises(p.GatewayError, match="no_fresh_session"):
        await protocol.command("send", slot=0)
    assert sent == []


async def test_send_correlated_and_never_retried(gateway):
    protocol, sent = gateway
    live(protocol)
    task = asyncio.create_task(protocol.command("send", slot=2, repeats=6))
    await asyncio.sleep(0)
    assert sent[0]["session"] == STATE["session"]
    assert len(sent[0]["request_id"]) == 32
    protocol.receive(
        "events", json.dumps({"event": "result", "action": "send", "ok": True, "request_id": "old"})
    )
    assert not task.done()
    result = {"event": "result", "action": "send", "ok": True, "request_id": sent[0]["request_id"]}
    protocol.receive("events", json.dumps(result), retained=True)
    assert not task.done()
    protocol.receive("events", json.dumps(result))
    assert (await task)["ok"] is True
    assert len(sent) == 1


async def test_unsolicited_learning_timeout_does_not_ack(gateway):
    protocol, sent = gateway
    live(protocol)
    task = asyncio.create_task(protocol.command("learn", timeout=0.02, slot=0, name="Test"))
    await asyncio.sleep(0)
    protocol.receive(
        "events", json.dumps({"event": "result", "action": "learn", "ok": False, "error": "learn_timeout"})
    )
    with pytest.raises(p.GatewayError, match="confirmation_timeout"):
        await task
    assert len(sent) == 1


async def test_timeout_blocks_until_new_live_state(gateway):
    protocol, sent = gateway
    live(protocol)
    with pytest.raises(p.GatewayError, match="confirmation_timeout"):
        await protocol.command("send", timeout=0.001, slot=0)
    with pytest.raises(p.GatewayError, match="no_fresh_session"):
        await protocol.command("send", slot=0)
    assert len(sent) == 1
    live(protocol)
    await protocol.command("status")
    assert len(sent) == 2


async def test_concurrent_actions_are_not_queued(gateway):
    protocol, sent = gateway
    live(protocol)
    first = asyncio.create_task(protocol.command("send", timeout=0.02, slot=0))
    await asyncio.sleep(0)
    with pytest.raises(p.GatewayError, match="command_in_progress"):
        await protocol.command("send", slot=1)
    protocol.disconnect()
    with pytest.raises(p.GatewayError, match="disconnected"):
        await first
    assert len(sent) == 1


async def test_session_change_cancels_pending_and_clears_slots(gateway):
    protocol, sent = gateway
    live(protocol)
    protocol.receive("remotes", '{"slot":0,"kind":"rc"}')
    task = asyncio.create_task(protocol.command("send", slot=0))
    await asyncio.sleep(0)
    live(protocol, session="fedcba9876543210")
    with pytest.raises(p.GatewayError, match="session_changed"):
        await task
    assert protocol.remotes == {}
    assert len(sent) == 1


async def test_rejection_is_reported(gateway):
    protocol, sent = gateway
    live(protocol)
    task = asyncio.create_task(protocol.command("send", slot=0))
    await asyncio.sleep(0)
    protocol.receive(
        "events",
        json.dumps(
            {
                "event": "result",
                "action": "send",
                "ok": False,
                "error": "tx_cooldown",
                "request_id": sent[0]["request_id"],
            }
        ),
    )
    with pytest.raises(p.GatewayError, match="tx_cooldown"):
        await task


def test_expiry_and_malformed_data(gateway):
    protocol, _ = gateway
    clock = [0.0]
    protocol.clock = lambda: clock[0]
    for malformed in ("bad", "[]", "null", "3"):
        protocol.receive("state", malformed)
    assert not protocol.available
    live(protocol)
    assert protocol.available
    clock[0] = 45
    assert not protocol.available
    protocol.receive("remotes", '{"slot":0,"kind":"rc"}')
    assert not protocol.remotes


@pytest.mark.parametrize(
    "action,fields",
    [
        ("send", {"slot": 16}),
        ("send", {"slot": True}),
        ("ir_send", {"slot": 8}),
        ("send", {"slot": 0, "repeats": 0}),
        ("ir_send", {"slot": 0, "repeats": 4}),
        ("learn", {"slot": 0, "name": "ä" * 17}),
        ("learn", {"slot": 0, "kind": "rolling"}),
        ("tune", {"frequency_mhz": 500}),
        ("tune", {"frequency_mhz": float("nan")}),
        ("scan", {"start_mhz": 347, "end_mhz": 388, "step_mhz": 0.1}),
        ("scan", {"start_mhz": 779, "end_mhz": 928, "step_mhz": 0.025}),
        ("scan", {"band": "433", "step_mhz": 0.1}),
        ("nrf_config", {"channel": 84, "address_hex": "0102030405"}),
        ("nrf_send", {"payload_hex": "A"}),
        ("nrf_send", {"payload_hex": "AA" * 33}),
        ("feature_set", {"feature": "nfc", "enabled": "true"}),
        ("send", {"slot": 0, "session": "injected"}),
        ("anything", {}),
    ],
)
def test_invalid_commands(action, fields):
    with pytest.raises(ValueError):
        p.command_payload(action, fields)


def test_valid_commands_and_slot_records(gateway):
    protocol, _ = gateway
    for action, fields in (
        ("send", {"slot": 15, "repeats": 10}),
        ("ir_send", {"slot": 7, "repeats": 0}),
        ("scan", {"band": "all"}),
        ("scan", {"start_mhz": 433.05, "end_mhz": 434.79, "step_mhz": 0.05}),
        ("nrf_config", {"channel": 83, "address_hex": "0102030405"}),
        ("nrf_send", {"payload_hex": "AA" * 32}),
    ):
        assert p.command_payload(action, fields)["action"] == action
    live(protocol)
    protocol.receive("remotes", '{"slot":15,"kind":"rc","name":"Socket"}')
    protocol.receive("ir/remotes", '{"slot":7,"occupied":true}')
    protocol.receive("remotes", '{"slot":16,"kind":"rc"}')
    assert list(protocol.remotes) == [15]
    assert list(protocol.ir_remotes) == [7]
    protocol.receive("availability", "offline", retained=True)
    assert not protocol.available
    assert not protocol.remotes


def test_learning_event_updates_slot_without_list(gateway):
    protocol, _ = gateway
    live(protocol)
    protocol.receive("events", '{"event":"learned","slot":4,"kind":"rc","name":"Bell"}')
    protocol.receive("events", '{"event":"ir_learned","slot":2,"occupied":true,"name":"TV"}')
    assert protocol.remotes[4]["name"] == "Bell"
    assert protocol.ir_remotes[2]["occupied"] is True


def test_scan_updates_are_bounded_but_final_is_immediate(gateway):
    protocol, _ = gateway
    notifications = []
    protocol.changed = lambda: notifications.append(True)
    live(protocol)
    notifications.clear()
    clock = [0.0]
    protocol.clock = lambda: clock[0]
    protocol.last_state = 0
    for index in range(20):
        clock[0] = index * 0.02
        protocol.receive("scan", json.dumps({"event": "scan_sample", "frequency_mhz": 433 + index * 0.05}))
    assert len(notifications) == 2
    protocol.receive("scan", '{"event":"scan_finished","points":20,"peak_mhz":433.92}')
    assert len(notifications) == 3
    assert protocol.scan["points"] == 20
