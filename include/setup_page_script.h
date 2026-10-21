#pragma once
#include <Arduino.h>

const char SetupPageScript[] PROGMEM = R"PORTAL(
const form = document.querySelector('form');
const ssidInput = document.getElementById('ssid');
const networkList = document.getElementById('networks');
const scanButton = document.getElementById('scan');
const scanStatus = document.getElementById('scan-status');

function showNetworks(data) {
  networkList.replaceChildren(new Option('WLAN auswaehlen ...', ''));
  for (const network of data.networks || []) {
    const label = network.ssid + ' · ' + network.rssi + ' dBm · ' + (network.secure ? 'gesichert' : 'offen');
    const option = new Option(label, network.ssid);
    option.dataset.secure = network.secure ? '1' : '0';
    networkList.add(option);
  }
  networkList.disabled = networkList.options.length < 2;
  networkList.value = ssidInput.value;
  scanStatus.textContent = data.error ? 'WLAN-Suche fehlgeschlagen. Erneut suchen oder WLAN-Namen direkt eingeben.' :
    networkList.disabled ? 'Kein sichtbares WLAN gefunden. Fuer versteckte Netze den Namen direkt eingeben.' :
    (networkList.options.length - 1) + ' WLAN gefunden. Auswaehlen oder den Namen direkt eingeben.';
}

async function scanNetworks() {
  if (scanButton.disabled) return;
  scanButton.disabled = true;
  scanStatus.textContent = 'Suche nach WLANs mit 2,4 GHz ...';
  try {
    const token = form.elements.namedItem('token').value;
    const response = await fetch('/scan', {
      method: 'POST', mode: 'same-origin', cache: 'no-store',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: new URLSearchParams({token})
    });
    if (response.status === 403) throw new Error('session');
    if (!response.ok) throw new Error('scan');
    let data = await response.json();
    const deadline = Date.now() + 15000;
    while (data.scanning) {
      if (Date.now() >= deadline) throw new Error('scan');
      await new Promise(resolve => setTimeout(resolve, 600));
      const poll = await fetch('/networks', {cache: 'no-store'});
      if (!poll.ok) throw new Error('scan');
      data = await poll.json();
    }
    showNetworks(data);
  } catch (error) {
    scanStatus.textContent = error.message === 'session' ?
      'Einrichtungsseite neu laden, dann erneut suchen.' :
      'WLAN-Suche nicht erreichbar. Erneut suchen oder WLAN-Namen direkt eingeben.';
  } finally {
    scanButton.disabled = false;
  }
}

networkList.addEventListener('change', () => {
  if (!networkList.value) return;
  ssidInput.value = networkList.value;
  form.elements.namedItem('open_wifi').checked = networkList.selectedOptions[0].dataset.secure === '0';
});
ssidInput.addEventListener('input', () => {
  networkList.value = ssidInput.value;
  if (!networkList.value) form.elements.namedItem('open_wifi').checked = false;
});
scanButton.addEventListener('click', scanNetworks);
form.addEventListener('submit', () => {
  form.querySelector('button[type=submit]').disabled = true;
});

async function updateStatus() {
  try {
    const response = await fetch('/status', {cache: 'no-store'});
    if (!response.ok) throw new Error('status');
    const state = await response.json();
    if (form.dataset.saving === '1' && !state.saving) {
      form.dataset.saving = '0';
      scanButton.disabled = false;
      form.querySelector('button[type=submit]').disabled = false;
      scanStatus.textContent = 'Bei Bedarf erneut suchen oder den WLAN-Namen direkt eingeben.';
    }
    const mqtt = state.mqtt_connected ? 'verbunden' : !state.mqtt_configured ? 'ausgeschaltet' :
      state.mqtt_state === 4 || state.mqtt_state === 5 ? 'Anmeldung fehlgeschlagen – Benutzer und Passwort pruefen' :
      'noch nicht verbunden – Broker, Port und Zugangsdaten pruefen';
    document.getElementById('status').textContent = state.saving ? 'Einstellungen werden uebernommen ...' :
      state.closing ? 'WLAN und MQTT bereit. Der Hotspot schliesst gleich.' :
      'WLAN: ' + (state.wifi_connected ? 'verbunden' : 'noch nicht verbunden – Name und Passwort pruefen') + ' · MQTT: ' + mqtt;
  } catch (error) {
    document.getElementById('status').textContent =
      'Hotspot nicht erreichbar. Bei erfolgreicher Einrichtung endet er automatisch; sonst erneut mit dem Hotspot verbinden.';
  }
}
updateStatus();
setInterval(updateStatus, 3000);
if (form.dataset.saving === '1') {
  scanButton.disabled = true;
  form.querySelector('button[type=submit]').disabled = true;
  scanStatus.textContent = 'Die gespeicherten Einstellungen werden uebernommen ...';
} else scanNetworks();
)PORTAL";
