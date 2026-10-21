"""Browser regression checks for the firmware's real form and script, using synthetic WLANs."""
import json
import os
import re
from html import escape
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

import pytest

playwright = pytest.importorskip("playwright.sync_api", reason="Install requirements-browser-test.txt")
ROOT = Path(__file__).resolve().parents[1]
ORIGIN = "http://192.168.4.1"
TOKEN = "local-browser-fixture"
NETWORKS = [
    {"ssid": "Fixture secured", "rssi": -42, "secure": True},
    {"ssid": "Fixture open", "rssi": -66, "secure": False},
    {"ssid": "<img src=x onerror=alert(1)>", "rssi": -75, "secure": True},
]


def firmware_page():
    """Read static markup from the firmware; only runtime values are supplied by this fixture."""
    source = (ROOT / "src/network_manager.cpp").read_text(encoding="utf-8")
    source = source.split("String NetworkManager::renderPage(", 1)[1]
    script = (ROOT / "include/setup_page_script.h").read_text(encoding="utf-8")
    script = script.split('R"PORTAL(', 1)[1].split(')PORTAL"', 1)[0]
    literal = r'"(?:\\.|[^"\\])*"'
    fragments = []
    for match in re.finditer(r"\bF\(\s*((?:" + literal + r"\s*)+)\)|FPSTR\(SetupPageScript\)", source):
        if match.group(1) is None:
            fragments.append(script)
        else:
            fragments.extend(json.loads(item) for item in re.findall(literal, match.group(1)))
    html = "".join(fragments).replace("name='token' value=''", f"name='token' value='{TOKEN}'")
    policy = re.search(r'sendHeader\("Referrer-Policy", "([^"]+)"\)', source).group(1)
    assert "<form" in html and "scanNetworks(false);" in html
    return html, policy


class Portal:
    def __init__(self, *, policy=None, failures=0, networks=None, saving=False, ssr=False, hang=False):
        self.html, current_policy = firmware_page()
        self.html = self.html.replace("data-saving=''", f"data-saving='{int(saving)}'")
        self.saving = saving
        self.policy = policy or current_policy
        self.failures = failures
        self.networks = NETWORKS if networks is None else networks
        self.hang = hang
        self.hanging_routes = []
        if ssr:
            options = '<option value="">WLAN waehlen / manuell eingeben</option>'
            for network in self.networks:
                options += f'<option value="{escape(network["ssid"], quote=True)}">{escape(network["ssid"])}</option>'
            self.html = self.html.replace("name='network_ssid'></select>", "name='network_ssid'>" + options + "</select>")
        self.scan_requests = 0
        self.polls = 0
        self.saved_origin = None
        self.saved_status = None
        self.scan_origin = None
        self.saved_fields = None

    def route(self, route):
        request = route.request
        path = urlsplit(request.url).path
        if path == "/" and request.method == "GET":
            route.fulfill(status=200, content_type="text/html; charset=utf-8", body=self.html,
                          headers={"Referrer-Policy": self.policy, "Cache-Control": "no-store"})
            return
        if path in ("/scan", "/save"):
            fields = parse_qs(request.post_data or "")
            origin = request.all_headers().get("origin")
            accepted = fields.get("token") == [TOKEN] and origin == ORIGIN
            if path == "/save":
                self.saved_fields = fields
                self.saved_origin = origin
                self.saved_status = 200 if accepted else 403
                route.fulfill(status=self.saved_status, body="Gespeichert" if accepted else "Sitzung ungueltig")
                return
            self.scan_requests += 1
            self.scan_origin = origin
            self.polls = 0
            if not accepted:
                route.fulfill(status=403, json={"error": "invalid_session"})
            elif self.failures:
                self.failures -= 1
                route.fulfill(status=503, json={"error": "scan_unavailable"})
            else:
                route.fulfill(status=202, json={"scanning": True})
            return
        if path == "/networks":
            if self.hang:
                self.hanging_routes.append(route)
                return
            self.polls += 1
            if self.failures:
                self.failures -= 1
                route.fulfill(status=503, json={"error": "scan_unavailable"})
            else:
                route.fulfill(json={"scanning": self.polls < 2, "networks": self.networks})
        elif path == "/status":
            route.fulfill(json={"wifi_connected": False, "mqtt_connected": False,
                                "mqtt_configured": False, "saving": self.saving})
        else:
            route.fulfill(status=404, body="Not found")


@pytest.fixture(scope="module")
def browser():
    with playwright.sync_playwright() as engine:
        channel = os.environ.get("PORTAL_BROWSER_CHANNEL") or ("chrome" if os.name == "nt" else None)
        instance = engine.chromium.launch(headless=True, channel=channel)
        yield instance
        instance.close()


@pytest.fixture
def open_portal(browser):
    contexts = []

    def open_page(**kwargs):
        javascript = kwargs.pop("javascript", True)
        portal = Portal(**kwargs)
        context = browser.new_context(service_workers="block", java_script_enabled=javascript)
        contexts.append(context)
        context.route(ORIGIN + "/**", portal.route)
        page = context.new_page()
        page.goto(ORIGIN + "/")
        return page, portal

    yield open_page
    for context in contexts:
        context.close()


@pytest.mark.parametrize("policy,expected_status,expected_origin", [
    ("no-referrer", 403, "null"),
    (None, 200, ORIGIN),
])
def test_real_form_post_origin_regression(open_portal, policy, expected_status, expected_origin):
    page, portal = open_portal(policy=policy)
    page.locator("#ssid").fill("Fixture hidden")
    page.locator("#mqtt_port").fill("1883")
    page.locator("summary").click()
    page.locator("#device_id").fill("lilygo_rf")
    page.locator("button[type=submit]").click()
    page.wait_for_url(ORIGIN + "/save")
    assert portal.saved_origin == expected_origin
    assert portal.saved_status == expected_status


def test_automatic_scan_selection_and_manual_rescan(open_portal):
    page, portal = open_portal()
    dialogs = []
    page.on("dialog", lambda dialog: (dialogs.append(dialog.message), dialog.dismiss()))
    page.wait_for_function("document.querySelector('#networks').options.length === 4")
    assert portal.scan_requests == 0
    assert portal.polls >= 1
    assert "-42 dBm" in page.locator("#networks").inner_text()
    page.locator("#networks").select_option("Fixture open")
    assert page.locator("#ssid").input_value() == "Fixture open"
    assert page.locator("[name=open_wifi]").is_checked()
    page.locator("#networks").select_option("Fixture secured")
    assert not page.locator("[name=open_wifi]").is_checked()
    page.locator("#ssid").fill("Fixture hidden")
    page.locator("#scan").click()
    page.wait_for_function("!document.querySelector('#scan').disabled")
    assert portal.scan_requests == 1
    assert portal.scan_origin == ORIGIN
    assert page.locator("#ssid").input_value() == "Fixture hidden"
    assert page.locator("#networks img").count() == 0
    assert not dialogs


def test_failed_scan_can_retry_without_losing_manual_input(open_portal):
    page, portal = open_portal(failures=1)
    page.wait_for_function("document.querySelector('#scan-status').textContent.includes('nicht erreichbar')")
    page.locator("#ssid").fill("Fixture hidden")
    page.locator("#scan").click()
    page.wait_for_function("document.querySelector('#networks').options.length === 4")
    assert portal.scan_requests == 1
    assert page.locator("#ssid").input_value() == "Fixture hidden"


def test_empty_scan_keeps_manual_entry_available(open_portal):
    page, _ = open_portal(networks=[])
    page.wait_for_function("document.querySelector('#scan-status').textContent.includes('Kein sichtbares WLAN')")
    page.locator("#ssid").fill("Fixture hidden")
    assert page.locator("#ssid").input_value() == "Fixture hidden"
    assert page.locator("#networks").is_enabled()
    assert page.locator("#scan").is_enabled()


def test_saving_page_waits_without_starting_an_invalid_scan(open_portal):
    page, portal = open_portal(saving=True)
    page.wait_for_function("document.querySelector('#status').textContent.includes('uebernommen')")
    assert portal.scan_requests == 0
    assert page.locator("#scan").is_disabled()
    assert page.locator("button[type=submit]").is_disabled()
    portal.saving = False
    page.wait_for_function("!document.querySelector('#scan').disabled")
    assert portal.scan_requests == 0
    assert page.locator("button[type=submit]").is_enabled()


def test_cached_network_can_be_selected_without_javascript(open_portal):
    page, portal = open_portal(ssr=True, javascript=False)
    assert page.locator("#networks option").count() == 4
    page.locator("#networks").select_option("Fixture secured")
    page.locator("#mqtt_port").fill("1883")
    page.locator("summary").click()
    page.locator("#device_id").fill("lilygo_rf")
    page.locator("button[type=submit]").click()
    page.wait_for_url(ORIGIN + "/save")
    assert portal.saved_fields["network_ssid"] == ["Fixture secured"]
    assert portal.saved_status == 200
    assert portal.scan_requests == 0


@pytest.mark.parametrize("cached", [False, True])
def test_hanging_request_stops_and_keeps_cached_choices(open_portal, cached):
    page, _ = open_portal(ssr=cached, hang=True)
    page.wait_for_function("document.querySelector('#scan-status').textContent.includes('antwortet nicht')")
    assert page.locator("#scan").is_enabled()
    assert page.locator("#networks option").count() == (4 if cached else 1)
    assert "laeuft" not in page.locator("#networks").inner_text()
    page.locator("#ssid").fill("Fixture hidden")
    assert page.locator("#ssid").input_value() == "Fixture hidden"
