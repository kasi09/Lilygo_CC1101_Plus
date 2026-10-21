#pragma once
#include <Arduino.h>

const char SetupPageScript[] PROGMEM = R"PORTAL(
const form = document.querySelector('form');
const ssidInput = document.getElementById('ssid');
const networkList = document.getElementById('networks');
const scanButton = document.getElementById('scan');
const scanStatus = document.getElementById('scan-status');

function selectCurrentName() {
  networkList.value = ssidInput.value;
  if (networkList.selectedIndex < 0) networkList.value = '';
}

function setScanMessage(message, emptyLabel) {
  scanStatus.textContent = message;
  if (networkList.options.length < 2) {
    networkList.options.length = 0;
    networkList.add(new Option(emptyLabel || 'WLAN waehlen / manuell eingeben', ''));
  }
}

function showNetworks(data) {
  while (networkList.firstChild) networkList.removeChild(networkList.firstChild);
  networkList.add(new Option('WLAN waehlen / manuell eingeben', ''));
  for (const network of data.networks || []) {
    const label = network.ssid + ' · ' + network.rssi + ' dBm · ' + (network.secure ? 'gesichert' : 'offen');
    const option = new Option(label, network.ssid);
    option.dataset.secure = network.secure ? '1' : '0';
    networkList.add(option);
  }
  networkList.disabled = false;
  selectCurrentName();
  scanStatus.textContent = data.error ? 'WLAN-Suche fehlgeschlagen. Erneut suchen oder WLAN-Namen direkt eingeben.' :
    networkList.options.length < 2 ? 'Kein sichtbares WLAN gefunden. Fuer versteckte Netze den Namen direkt eingeben.' :
    (networkList.options.length - 1) + ' WLAN gefunden. Auswaehlen oder den Namen direkt eingeben.';
  if (data.error && networkList.options.length < 2) networkList.options[0].textContent = 'Suche fehlgeschlagen / manuell eingeben';
}

async function portalRequest(path, options) {
  const controller = typeof AbortController === 'function' ? new AbortController() : null;
  const settings = Object.assign({cache: 'no-store'}, options || {});
  if (controller) settings.signal = controller.signal;
  let timer;
  try {
    return await Promise.race([
      (async () => {
        const response = await fetch(path, settings);
        if (response.status === 403) throw new Error('session');
        if (!response.ok) throw new Error('request');
        return await response.json();
      })(),
      new Promise((resolve, reject) => {
        timer = setTimeout(() => { if (controller) controller.abort(); reject(new Error('timeout')); }, 4500);
      })
    ]);
  } finally { clearTimeout(timer); }
}

async function scanNetworks(refresh) {
  if (scanButton.disabled) return;
  scanButton.disabled = true;
  setScanMessage('Suche nach WLANs mit 2,4 GHz ...', 'WLAN-Suche laeuft ...');
  try {
    let data;
    if (refresh) {
      const token = form.elements.namedItem('token').value;
      data = await portalRequest('/scan', {
        method: 'POST', mode: 'same-origin',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: new URLSearchParams({token})
      });
    } else data = await portalRequest('/networks');
    const deadline = Date.now() + 25000;
    while (data.scanning) {
      if (Date.now() >= deadline) throw new Error('scan');
      if (data.networks && data.networks.length) showNetworks(data);
      scanStatus.textContent = data.phase === 'retrying' ? 'WLAN ist gerade beschaeftigt. Suche wird erneut versucht ...' : 'WLAN-Suche laeuft ...';
      await new Promise(resolve => setTimeout(resolve, 600));
      data = await portalRequest('/networks');
    }
    showNetworks(data);
  } catch (error) {
    const message = error.message === 'session' ?
      'Einrichtungsseite neu laden, dann erneut suchen.' :
      error.message === 'timeout' ? 'WLAN-Suche antwortet nicht. Liste neu laden, erneut suchen oder den Namen direkt eingeben.' :
      'Aktualisierung nicht erreichbar. Vorhandene Liste nutzen, erneut suchen oder den Namen direkt eingeben.';
    setScanMessage(message, 'Suche nicht erreichbar / manuell eingeben');
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
  selectCurrentName();
  if (!networkList.value) form.elements.namedItem('open_wifi').checked = false;
});
scanButton.addEventListener('click', () => scanNetworks(true));
form.addEventListener('submit', () => {
  form.querySelector('button[type=submit]').disabled = true;
});

async function updateStatus() {
  try {
    const state = await portalRequest('/status');
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
} else scanNetworks(false);
)PORTAL";
