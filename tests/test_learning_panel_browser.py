"""Exercise the shipped panel in Chrome with a simulated HA connection."""

import os
from pathlib import Path

import pytest

playwright = pytest.importorskip("playwright.sync_api", reason="Install requirements-browser-test.txt")
SCRIPT = Path(__file__).resolve().parents[1] / "custom_components/lilygo_t_embed/frontend/lilygo-panel.js"


def device():
    return {"device_id": "test-device", "name": "Lilygo Wohnzimmer", "available": True,
            "version": "0.5.4", "mode": "receive", "frequency_mhz": 433.92,
            "storage_ok": True, "ir_available": True, "rf_loaded": True, "ir_loaded": True,
            "rf": [{"slot": i, "occupied": i < 2, "kind": "raw" if i < 2 else "empty",
                    "name": ("Steckdose EIN" if i == 0 else "Steckdose AUS") if i < 2 else "",
                    "frequency_mhz": 433.92 if i < 2 else None} for i in range(16)],
            "ir": [{"slot": i, "occupied": False, "kind": "ir", "name": ""} for i in range(8)]}


@pytest.fixture(scope="module")
def browser():
    with playwright.sync_playwright() as engine:
        channel = os.environ.get("PORTAL_BROWSER_CHANNEL") or ("chrome" if os.name == "nt" else None)
        instance = engine.chromium.launch(headless=True, channel=channel)
        yield instance
        instance.close()


@pytest.fixture
def panel(browser):
    contexts = []
    def open_panel(*, data=None, width=1100, height=850):
        context = browser.new_context(viewport={"width": width, "height": height})
        contexts.append(context)
        page = context.new_page()
        page.clock.install()
        errors = []
        page.on("pageerror", lambda error: errors.append(str(error)))
        page.set_content('<html lang="de"><body style="margin:0"></body></html>')
        page.add_script_tag(path=str(SCRIPT))
        page.evaluate("""(devices) => {
          window.devices = devices; window.calls = []; window.wsCalls = 0; window.unsubscribed = 0;
          window.failService = false; window.immediateCapture = false; window.holdSend = false;
          const connection = {subscribeEvents: async (cb) => {window.emit = (data) => cb({data}); return () => {window.unsubscribed++};}};
          window.fakeHass = {connection, callWS: async () => {window.wsCalls++; return structuredClone(window.devices)},
            callService: async (domain, service, data) => {
              window.calls.push({domain, service, data});
              if(window.failService) throw Error('unavailable');
              if(service === 'send_code' && window.holdSend) await new Promise(resolve => {window.releaseSend = resolve});
              if(service === 'learn_code' && window.immediateCapture) window.emit({device_id:data.device_id, event_type:'learned', slot:data.slot});
            }};
          window.panel = document.createElement('lilygo-learn-panel');
          window.panel.hass = window.fakeHass; document.body.append(window.panel);
        }""", [device()] if data is None else data)
        page.wait_for_function("window.wsCalls > 0 && !window.panel._fetching")
        return page, errors
    yield open_panel
    for context in contexts:
        context.close()


def begin(page, name="Wohnzimmer EIN"):
    page.locator("#new").click()
    page.locator("#name").fill(name)
    page.locator("#start").click()
    page.wait_for_function("window.panel._active.phase !== 'starting'")


def test_raw_default_free_slot_and_confirmed_success(panel):
    page, errors = panel()
    begin(page)
    assert page.evaluate("window.calls") == [{"domain": "lilygo_t_embed", "service": "learn_code",
        "data": {"device_id": "test-device", "slot": 2, "name": "Wohnzimmer EIN", "code_type": "raw"}}]
    assert "15 s" in page.locator("#countdown").inner_text()
    assert "Gespeichert" not in page.locator("#phase-title").inner_text()
    page.evaluate("window.emit({device_id:'other-device',event_type:'learned',slot:2})")
    page.evaluate("window.emit({device_id:'test-device',event_type:'learned',slot:3})")
    assert "Gespeichert" not in page.locator("#phase-title").inner_text()
    page.evaluate("window.emit({device_id:'test-device',event_type:'learned',slot:2})")
    assert page.locator("#phase-title").inner_text() == "Gespeichert"
    assert not errors


def test_occupied_slot_needs_explicit_replacement(panel):
    page, _ = panel()
    page.locator('.slot[data-slot="1"] button').first.click()
    assert page.locator("#start").is_disabled()
    assert page.locator("#replace-label").is_visible()
    page.locator("#replace").check()
    page.locator("#start").click()
    page.wait_for_function("window.calls.length === 1")
    assert page.evaluate("window.calls[0].data.slot") == 1


def test_timeout_and_unknown_outcome_never_retry(panel):
    page, _ = panel()
    begin(page)
    page.clock.fast_forward(20000)
    assert page.locator("#phase-title").inner_text() == "Keine Aufnahme bestätigt"
    assert len(page.evaluate("window.calls")) == 1
    page.clock.fast_forward(30000)
    assert len(page.evaluate("window.calls")) == 1


def test_device_error_and_cancel_are_visible(panel):
    page, _ = panel()
    begin(page)
    page.evaluate("window.emit({device_id:'test-device',event_type:'result',action:'learn',ok:false,error:'storage_write_failed'})")
    assert "Codespeicher" in page.locator("#phase-text").inner_text()
    page.locator("#start").click()
    page.wait_for_function("window.panel._active.phase === 'learning'")
    page.locator("#cancel").click()
    page.wait_for_function("!window.panel.shadowRoot.getElementById('dialog').open")
    assert page.evaluate("window.calls.at(-1)") == {"domain": "lilygo_t_embed", "service": "execute",
        "data": {"device_id": "test-device", "action": "stop"}}


def test_infrared_uses_eight_slots_and_no_frequency_command(panel):
    page, _ = panel()
    page.locator("#ir-tab").click()
    assert page.locator(".slot").count() == 8
    begin(page, "TV EIN")
    assert page.evaluate("window.calls[0].data.code_type") == "ir"
    assert len(page.evaluate("window.calls")) == 1


def test_rc_selection_and_device_learning_timeout(panel):
    page, _ = panel()
    page.locator("#new").click()
    page.locator("#type").select_option("rc")
    page.locator("#name").fill("Klingel")
    page.locator("#start").click()
    page.wait_for_function("window.panel._active.phase === 'learning'")
    assert page.evaluate("window.calls[0].data.code_type") == "rc"
    page.evaluate("window.emit({device_id:'test-device',event_type:'result',action:'learn',ok:false,error:'learn_timeout'})")
    assert page.locator("#phase-title").inner_text() == "Keine Taste aufgenommen"
    assert "15 Sekunden" in page.locator("#phase-text").inner_text()
    assert len(page.evaluate("window.calls")) == 1


def test_invalid_frequency_and_long_name_do_not_start(panel):
    page, _ = panel()
    page.locator("#new").click()
    page.locator("#name").fill("Steckdose")
    page.locator("#frequency").fill("370")
    page.locator("#start").click()
    assert "Frequenz" in page.locator("#flow-notice").inner_text()
    page.locator("#frequency").fill("433.92")
    page.locator("#name").fill("ü" * 17)
    page.locator("#start").click()
    assert "zu lang" in page.locator("#flow-notice").inner_text()
    assert not page.evaluate("window.calls")


def test_tune_precedes_learning_and_fast_capture_is_not_lost(panel):
    page, _ = panel()
    page.evaluate("window.immediateCapture = true")
    page.locator("#new").click()
    page.locator("#name").fill("Andere Frequenz")
    page.locator("#frequency").fill("434.00")
    page.locator("#start").click()
    page.wait_for_function("window.panel._active.phase === 'saved'")
    assert [call["service"] for call in page.evaluate("window.calls")] == ["tune", "learn_code"]
    assert page.locator("#phase-title").inner_text() == "Gespeichert"


def test_offline_and_pending_record_lists_cannot_overwrite(panel):
    offline = device()
    offline["available"] = False
    page, _ = panel(data=[offline])
    assert page.locator("#new").is_disabled()
    assert page.get_by_text("Der Lilygo ist offline.", exact=False).is_visible()
    waiting = device()
    waiting["rf_loaded"] = False
    page, _ = panel(data=[waiting])
    page.locator("#new").click()
    page.locator("#name").fill("Noch nicht laden")
    assert page.locator("#start").is_disabled()


def test_names_are_text_and_form_survives_updates(panel):
    malicious = device()
    malicious["rf"][0]["name"] = '<img src=x onerror="window.injected=true">'
    page, errors = panel(data=[malicious])
    assert page.locator(".slot img").count() == 0
    assert not page.evaluate("window.injected || false")
    page.locator("#new").click()
    page.locator("#name").fill("Mein unveränderter Name")
    page.clock.fast_forward(6000)
    assert page.locator("#name").input_value() == "Mein unveränderter Name"
    assert not errors


def test_send_is_single_and_disconnect_stops_polling(panel):
    page, _ = panel()
    page.evaluate("window.holdSend = true")
    page.locator('.slot[data-slot="0"]').get_by_role("button", name="Senden").click()
    page.evaluate("window.panel._send(0)")
    assert len(page.evaluate("window.calls")) == 1
    page.evaluate("window.releaseSend()")
    page.wait_for_function("!window.panel._sending")
    assert page.evaluate("window.calls[0].data.repeats") == 1
    page.evaluate("window.panel.remove()")
    count = page.evaluate("window.wsCalls")
    page.clock.fast_forward(10000)
    assert page.evaluate("window.wsCalls") == count
    assert page.evaluate("window.unsubscribed") == 1


def test_open_dialog_never_switches_to_another_device(panel):
    second = device()
    second.update(device_id="second-device", name="Zweiter Lilygo")
    page, _ = panel(data=[device(), second])
    page.locator("#new").click()
    page.locator("#name").fill("Nur erstes Gerät")
    page.evaluate("window.devices.shift(); window.panel._refresh()")
    page.wait_for_function("!window.panel._fetching")
    assert page.locator("#start").is_disabled()
    assert page.evaluate("window.panel._selected") == "test-device"
    assert not page.evaluate("window.calls")
    page.locator("#cancel").click()
    page.wait_for_function("window.panel._selected === 'second-device'")


def test_changed_occupied_slot_needs_new_confirmation(panel):
    page, _ = panel()
    page.locator('.slot[data-slot="1"] button').first.click()
    page.locator("#replace").check()
    assert page.locator("#start").is_enabled()
    page.evaluate("window.devices[0].rf[1].name = 'Inzwischen geändert'; window.panel._refresh()")
    page.wait_for_function("!window.panel._fetching")
    assert not page.locator("#replace").is_checked()
    assert page.locator("#start").is_disabled()


@pytest.mark.parametrize("width,height", [(390, 844), (320, 640), (1100, 850)])
def test_dialog_stays_inside_viewport(panel, width, height):
    page, errors = panel(width=width, height=height)
    begin(page)
    bounds = page.locator("#dialog").bounding_box()
    assert bounds["x"] >= 0 and bounds["x"] + bounds["width"] <= width + 1
    assert bounds["y"] >= 0 and bounds["y"] + bounds["height"] <= height + 1
    assert page.locator("#cancel").is_visible()
    assert not errors
