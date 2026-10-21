class LilygoLearnPanel extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: "open" });
    this._devices = [];
    this._selected = "";
    this._kind = "rf";
    this._active = null;
    this._fetching = false;
    this._sending = false;
    this.shadowRoot.innerHTML = `
      <style>
        :host{display:block;min-height:100%;background:var(--primary-background-color,#f5f7fa);color:var(--primary-text-color,#17283b);font-family:var(--primary-font-family,Roboto,Arial,sans-serif);font-size:15px}
        *{box-sizing:border-box}button,input,select{font:inherit}button{cursor:pointer;border:1px solid var(--divider-color,#d5dfe8);border-radius:10px;background:var(--card-background-color,#fff);color:inherit;padding:11px 17px;min-height:44px;font-weight:600}
        button:hover:enabled{border-color:var(--primary-color,#0878cc)}button:disabled{opacity:.45;cursor:default}.primary{background:var(--primary-color,#0878cc);color:var(--text-primary-color,#fff);border-color:transparent}
        button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid var(--primary-color,#0878cc);outline-offset:3px}
        header{height:64px;display:flex;align-items:center;gap:16px;padding:0 24px;background:var(--app-header-background-color,var(--card-background-color,#fff));color:var(--app-header-text-color,inherit);border-bottom:1px solid var(--divider-color,#e1e8ef)}header strong{font-size:20px}#menu{display:none;padding:6px 12px;border:0;background:transparent;font-size:24px}:host([narrow]) #menu{display:block}
        main{max-width:1100px;margin:auto;padding:32px 28px 60px}.eyebrow{color:var(--primary-color,#0878cc);font-size:12px;font-weight:700;letter-spacing:1.8px;text-transform:uppercase}h1{font-size:30px;letter-spacing:-.7px;margin:8px 0 10px}h2{font-size:21px;margin:0}p{line-height:1.6}.muted{color:var(--secondary-text-color,#607386)}.intro{margin:0 0 26px;max-width:630px}
        .toolbar{display:flex;gap:16px;align-items:end;flex-wrap:wrap;background:var(--card-background-color,#fff);border:1px solid var(--divider-color,#dce5ed);border-radius:16px;padding:20px;margin:24px 0}.toolbar label{flex:1;min-width:190px}.status{font-size:13px;margin-top:9px;line-height:1.5}label{display:block;font-weight:600;font-size:13px}select,input[type=text],input[type=number]{width:100%;border:1px solid var(--divider-color,#cad6e1);border-radius:9px;min-height:46px;padding:10px 12px;margin-top:8px;color:var(--primary-text-color,#17283b);background:var(--card-background-color,#fff)}
        .section{display:flex;justify-content:space-between;align-items:center;gap:16px;margin:28px 0 18px}.tabs{display:flex;gap:4px;padding:4px;border-radius:12px;background:var(--secondary-background-color,#e8eef5)}.tabs button{border:0;background:transparent;min-height:38px;padding:8px 18px}.tabs button[aria-selected=true]{background:var(--card-background-color,#fff);box-shadow:0 1px 4px #0001}
        #cards{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:14px}.slot{background:var(--card-background-color,#fff);border:1px solid var(--divider-color,#dce5ed);border-radius:14px;padding:19px;min-height:166px;display:flex;flex-direction:column;gap:9px}.slot.empty{border-style:dashed}.slot .number{font-size:12px;font-weight:600;color:var(--secondary-text-color,#607386)}.slot h3{margin:0;font-size:17px;overflow-wrap:anywhere}.slot p{font-size:12px;margin:0}.slot .actions{display:flex;gap:8px;margin-top:auto;padding-top:10px}.slot button{padding:8px 12px;font-size:13px;min-height:38px}.slot.empty h3{font-weight:400;color:var(--secondary-text-color,#607386)}
        .notice{border-radius:10px;padding:13px 16px;background:var(--secondary-background-color,#e8eef5);line-height:1.5;margin:14px 0}.notice[data-tone=error]{background:var(--error-color,#db4437);color:#fff}.notice[data-tone=success]{background:#18744b;color:#fff}[hidden]{display:none!important}.empty-state{padding:24px;border:1px dashed var(--divider-color,#cad6e1);border-radius:14px}
        dialog{border:1px solid var(--divider-color,#dce5ed);border-radius:20px;padding:0;width:min(560px,calc(100vw - 28px));max-height:calc(100dvh - 28px);color:var(--primary-text-color,#17283b);background:var(--card-background-color,#fff);box-shadow:0 24px 100px #0005}dialog::backdrop{background:#12263e99}.dialog-body{padding:28px}.dialog-head{display:flex;justify-content:space-between;align-items:start;gap:20px}.dialog-head p{margin:8px 0 20px;font-size:14px}#close{border:0;padding:0 8px;min-height:30px;background:transparent;font-size:24px}.fields{display:grid;grid-template-columns:1fr 1fr;gap:18px}.wide{grid-column:1/-1}.help{font-size:12px;margin:7px 0 0}.confirm{margin-top:18px;font-weight:400;display:flex;align-items:start;gap:10px;line-height:1.5}.confirm input{margin-top:3px;width:18px;height:18px;flex-shrink:0}.dialog-footer{display:flex;justify-content:end;gap:10px;padding:18px 28px;border-top:1px solid var(--divider-color,#dce5ed);background:var(--card-background-color,#fff)}
        dialog[open]{display:flex;flex-direction:column}.dialog-body{overflow:auto;min-height:0}.dialog-footer{flex-shrink:0}.progress{margin-top:22px;border-radius:12px;padding:18px;background:var(--secondary-background-color,#edf3f8)}#countdown{font-size:30px;font-weight:700;font-variant-numeric:tabular-nums;float:right;margin-left:16px}progress{width:100%;height:8px;margin-top:16px;accent-color:var(--primary-color,#0878cc)}#phase-title{font-weight:700}#phase-text{font-size:13px;line-height:1.5;margin:7px 0 0}.dialog-body>form{margin:0}
        @media(max-width:600px){header{padding:0 12px}main{padding:24px 16px 40px}h1{font-size:27px}.toolbar{padding:16px}.toolbar .primary{width:100%}.section{align-items:start}#cards{grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.slot{padding:14px;min-height:175px}.slot .actions{flex-direction:column}.slot h3{font-size:15px}.dialog-body{padding:22px}.dialog-footer{padding:16px 22px}.fields{grid-template-columns:1fr}.wide{grid-column:auto}}
      </style>
      <header><button id="menu" aria-label="Menü öffnen">☰</button><strong>LILYGO</strong></header>
      <main>
        <div class="eyebrow">Funk &amp; Infrarot</div><h1>Deine Fernbedienungen.</h1>
        <p class="intro muted">Tasten aufnehmen, eindeutig benennen und direkt verwenden. EIN und AUS erhalten jeweils einen eigenen Speicherplatz.</p>
        <div id="page-notice" class="notice" role="status" hidden></div>
        <div class="toolbar"><label>Gerät<select id="device" aria-label="Gerät"></select><div id="device-status" class="status muted"></div></label><button id="new" class="primary">+ Fernbedienung anlernen</button></div>
        <div id="empty" class="empty-state" hidden>Noch kein Lilygo eingerichtet. Füge unter Einstellungen → Geräte &amp; Dienste die Integration „LILYGO T-Embed CC1101 Plus“ hinzu.</div>
        <section id="slots"><div class="section"><div><h2>Speicherplätze</h2><div id="occupied" class="status muted"></div></div><div class="tabs" role="tablist" aria-label="Codeart"><button id="rf-tab" role="tab" aria-selected="true">Funk</button><button id="ir-tab" role="tab" aria-selected="false">Infrarot</button></div></div><div id="cards"></div></section>
      </main>
      <dialog id="dialog" aria-labelledby="dialog-title">
        <div class="dialog-body"><div class="dialog-head"><div><div class="eyebrow">Neue Aufnahme</div><h2 id="dialog-title">Taste anlernen</h2><p class="muted">Fernbedienung bereithalten. Nach dem Start hast du 15 Sekunden.</p></div><button id="close" aria-label="Fenster schließen">×</button></div>
          <form id="learn-form"><div class="fields">
            <label class="wide">Name<input id="name" type="text" maxlength="32" required placeholder="z. B. Wohnzimmer EIN" autocomplete="off"></label>
            <label>Aufnahmeart<select id="type"><option value="raw">Funk RAW · auch SEC24</option><option value="rc">Funk RC · bekannte Protokolle</option><option value="ir">Infrarot</option></select></label>
            <label>Speicherplatz<select id="slot"></select></label>
            <label id="frequency-field" class="wide">Frequenz in MHz<input id="frequency" type="number" min="300" max="928" step="0.01" value="433.92" required><p class="help muted">Für deine 433-MHz-Steckdosen: 433,92 MHz. RAW eignet sich für die SEC24 HAF760R2.</p></label>
          </div><label id="replace-label" class="confirm" hidden><input id="replace" type="checkbox"><span id="replace-text"></span></label></form>
          <div id="flow-notice" class="notice" role="alert" hidden></div>
          <div id="progress" class="progress" role="status" aria-live="polite" hidden><span id="countdown"></span><div id="phase-title"></div><p id="phase-text"></p><progress id="bar" max="15" value="15"></progress></div>
        </div><div class="dialog-footer"><button id="cancel">Schließen</button><button id="start" class="primary">Anlernen starten</button></div>
      </dialog>`;
    this.$ = (id) => this.shadowRoot.getElementById(id);
    this.$("menu").onclick = () => this.dispatchEvent(new CustomEvent("hass-toggle-menu", { bubbles: true, composed: true }));
    this.$("device").onchange = () => { this._selected = this.$("device").value; this._cardSignature = ""; this._render(); };
    this.$("new").onclick = () => this._open();
    this.$("rf-tab").onclick = () => this._tab("rf");
    this.$("ir-tab").onclick = () => this._tab("ir");
    this.$("type").onchange = () => this._formSlots();
    this.$("slot").onchange = () => { this.$("replace").checked = false; this._formState(); };
    this.$("replace").onchange = () => this._formState();
    this.$("name").oninput = () => this._formState();
    this.$("learn-form").onsubmit = (event) => { event.preventDefault(); this._start(); };
    this.$("start").onclick = () => this._start();
    this.$("cancel").onclick = () => this._close();
    this.$("close").onclick = () => this._close();
    this.$("dialog").addEventListener("cancel", (event) => { event.preventDefault(); this._close(); });
  }
  set hass(value) { this._hass = value; if (this.isConnected && value?.connection !== this._connection) this._connect(); }
  set narrow(value) { this.toggleAttribute("narrow", Boolean(value)); }
  connectedCallback() { this._connect(); this._poll = setInterval(() => this._refresh(), 2000); this._clock = setInterval(() => this._tick(), 250); }
  disconnectedCallback() { clearInterval(this._poll); clearInterval(this._clock); this._unsubscribe?.(); this._unsubscribe = null; this._connection = null; }
  _connect() {
    if (!this._hass?.connection || this._connection === this._hass.connection) return;
    this._unsubscribe?.(); this._unsubscribe = null;
    const connection = this._connection = this._hass.connection;
    connection.subscribeEvents((event) => { if (this._connection === connection && this.isConnected) this._event(event); }, "lilygo_t_embed_event").then((unsubscribe) => {
      if (!this.isConnected || this._connection !== connection) unsubscribe(); else this._unsubscribe = unsubscribe;
    }).catch(() => this._notice("Live-Ereignisse sind nicht erreichbar. Verbindung zu Home Assistant prüfen.", "error"));
    this._refresh();
  }
  async _refresh() {
    if (!this._hass || !this.isConnected || this._fetching) return;
    this._fetching = true; const connection = this._connection;
    try {
      const devices = await this._hass.callWS({ type: "lilygo_t_embed/devices" });
      if (connection !== this._connection || !this.isConnected) return;
      this._devices = devices;
      if (!devices.some((device) => device.device_id === this._selected) && !this.$("dialog").open) this._selected = devices.find((device) => device.available)?.device_id || devices[0]?.device_id || "";
      this._render();
    } catch (error) { this._notice("Geräteübersicht nicht erreichbar. Home-Assistant-Verbindung und Integration prüfen.", "error"); }
    finally { this._fetching = false; }
  }
  _device() { return this._devices.find((device) => device.device_id === this._selected); }
  _busy(device) { return ["learn_rc", "learn_raw", "learn_ir", "scan"].includes(device?.mode); }
  _locked() { return ["starting", "learning", "cancelling"].includes(this._active?.phase); }
  _notice(text, tone = "info", id = "page-notice") { const element = this.$(id); element.textContent = text; element.hidden = !text; element.dataset.tone = tone; }
  _tab(kind) { this._kind = kind; this._cardSignature = ""; this._render(); }
  _render() {
    const device = this._device();
    const optionsSignature = JSON.stringify(this._devices.map((item) => [item.device_id, item.name]));
    if (optionsSignature !== this._optionsSignature) { this.$("device").replaceChildren(...this._devices.map((item) => new Option(item.name, item.device_id))); this._optionsSignature = optionsSignature; }
    this.$("device").value = this._selected; this.$("device").disabled = this._locked();
    this.$("empty").hidden = Boolean(device); this.$("slots").hidden = !device;
    this.$("new").disabled = !device?.available || !device.storage_ok || this._busy(device) || this._locked();
    this.$("device-status").textContent = !device ? "" : !device.available ? "Offline · Verbindung zum Lilygo prüfen" : `Verbunden · ${Number(device.frequency_mhz || 433.92).toFixed(2).replace(".", ",")} MHz · Firmware ${device.version || "–"}`;
    if (device && !device.available) this._notice("Der Lilygo ist offline. Anlernen ist möglich, sobald die Verbindung wieder steht.");
    else if (device && !device.storage_ok) this._notice("Der Codespeicher ist nicht bereit. Firmware und Gerätestatus prüfen.", "error");
    else if (!this._pagePersistent) this._notice("");
    this.$("rf-tab").setAttribute("aria-selected", this._kind === "rf"); this.$("ir-tab").setAttribute("aria-selected", this._kind === "ir");
    if (device) {
      const records = device[this._kind], loaded = device[`${this._kind}_loaded`];
      this.$("occupied").textContent = loaded ? `${records.filter((item) => item.occupied).length} von ${records.length} belegt` : "Speicherplätze werden geladen …";
      const signature = JSON.stringify([this._selected, this._kind, records, loaded, device.available, device.mode, device.storage_ok, this._sending]);
      if (signature !== this._cardSignature) { this._cardSignature = signature; this.$("cards").replaceChildren(...records.map((record) => this._card(record, device, loaded))); }
    }
    if (this.$("dialog").open) this._formState();
  }
  _card(record, device, loaded) {
    const card = document.createElement("article"); card.className = `slot${record.occupied ? "" : " empty"}`; card.dataset.slot = record.slot;
    const number = document.createElement("div"); number.className = "number"; number.textContent = `PLATZ ${record.slot}`;
    const title = document.createElement("h3"); title.textContent = record.occupied ? record.name || "Unbenannter Code" : loaded ? "Freier Speicherplatz" : "Wird geladen …";
    const detail = document.createElement("p"); detail.className = "muted";
    detail.textContent = record.occupied ? record.kind === "ir" ? "Infrarot" : `${record.kind.toUpperCase()} · ${Number(record.frequency_mhz || 433.92).toFixed(2).replace(".", ",")} MHz` : this._kind === "ir" ? "Für eine Infrarot-Taste" : "Für eine EIN- oder AUS-Taste";
    const actions = document.createElement("div"); actions.className = "actions";
    const learn = document.createElement("button"); learn.textContent = record.occupied ? "Neu anlernen" : "Anlernen"; learn.disabled = !loaded || !device.available || !device.storage_ok || this._busy(device) || this._locked(); learn.onclick = () => this._open(record.slot); actions.append(learn);
    if (record.occupied) { const send = document.createElement("button"); send.textContent = "Senden"; send.title = "Gespeicherten Code einmal senden"; send.disabled = !device.available || this._busy(device) || this._locked() || this._sending; send.onclick = () => this._send(record.slot); actions.append(send); }
    card.append(number, title, detail, actions); return card;
  }
  _open(slot) {
    const device = this._device(); if (!device?.available || this._busy(device)) return;
    this._active = { phase: "ready" }; this._notice("", "info", "flow-notice"); this.$("type").value = this._kind === "ir" ? "ir" : "raw";
    this.$("frequency").value = Number(device.frequency_mhz || 433.92).toFixed(2); this.$("replace").checked = false; this._formSlots(slot);
    const record = this._formRecord(); this.$("name").value = record?.occupied ? record.name : "";
    this.$("progress").hidden = true; this.$("dialog").showModal(); this._formState(); this.$("name").focus();
  }
  _formRecord() { const kind = this.$("type").value === "ir" ? "ir" : "rf"; return this._device()?.[kind]?.find((record) => record.slot === Number(this.$("slot").value)); }
  _formSlots(preferred) {
    const kind = this.$("type").value === "ir" ? "ir" : "rf", records = this._device()?.[kind] || [];
    this.$("slot").replaceChildren(...records.map((record) => new Option(`Platz ${record.slot} · ${record.occupied ? record.name || "belegt" : "frei"}`, String(record.slot))));
    this.$("slot").value = String(preferred ?? records.find((record) => !record.occupied)?.slot ?? 0); this.$("replace").checked = false; this._formState();
  }
  _formState() {
    const device = this._device(), record = this._formRecord(), locked = this._locked(), isIr = this.$("type").value === "ir";
    const replacement = JSON.stringify([this._selected, isIr, record]);
    if (replacement !== this._replaceSignature) { this.$("replace").checked = false; this._replaceSignature = replacement; }
    this.$("frequency-field").hidden = isIr; this.$("frequency").required = !isIr; this.$("replace-label").hidden = !record?.occupied;
    this.$("replace-text").textContent = `Vorhandenen Code „${record?.name || "Unbenannter Code"}“ auf Platz ${record?.slot ?? ""} ersetzen.`;
    this.$("learn-form").hidden = locked || this._active?.phase === "saved";
    for (const id of ["name", "type", "slot", "frequency", "replace"]) this.$(id).disabled = locked;
    this.$("start").hidden = locked || this._active?.phase === "saved";
    this.$("start").disabled = locked || !device?.available || !device.storage_ok || !device[isIr ? "ir_loaded" : "rf_loaded"] || this._busy(device) || (record?.occupied && !this.$("replace").checked) || !this.$("name").value.trim() || (isIr && !device.ir_available);
    this.$("cancel").textContent = locked ? "Abbrechen" : "Schließen"; this.$("close").disabled = this._active?.phase === "cancelling"; this.$("cancel").disabled = this._active?.phase === "cancelling";
  }
  async _start() {
    if (this.$("start").disabled || this._locked()) return;
    if (!this.$("learn-form").reportValidity()) return;
    const device = this._device(), type = this.$("type").value, name = this.$("name").value.trim(), frequency = Number(this.$("frequency").value);
    if (new TextEncoder().encode(name).length > 32) { this._notice("Der Name ist zu lang. Bitte kürzen.", "error", "flow-notice"); return; }
    if (type !== "ir" && ![[300,348],[387,464],[779,928]].some(([lo, hi]) => frequency >= lo && frequency <= hi)) { this._notice("Bitte eine Frequenz in 300–348, 387–464 oder 779–928 MHz wählen.", "error", "flow-notice"); return; }
    const active = this._active = { phase: "starting", device_id: device.device_id, slot: Number(this.$("slot").value), type, name };
    this._notice("", "info", "flow-notice"); this._flow("Aufnahme wird gestartet …", "Gleich erscheint die Aufforderung zum Drücken."); this._formState();
    try {
      if (type !== "ir" && Math.abs(frequency - Number(device.frequency_mhz)) > .005) await this._hass.callService("lilygo_t_embed", "tune", { device_id: active.device_id, frequency_mhz: frequency });
      await this._hass.callService("lilygo_t_embed", "learn_code", { device_id: active.device_id, slot: active.slot, name, code_type: type });
      if (this._active !== active || active.phase !== "starting") return;
      active.phase = "learning"; active.deadline = Date.now() + 15000; this._flow(`Jetzt „${name}“ drücken`, `Aufnahme auf Platz ${active.slot}. Bitte nur diese Taste etwa zwei Sekunden halten.`); this._tick();
    } catch (error) { if (this._active === active && active.phase !== "saved") { active.phase = "error"; this._flow("Start nicht bestätigt", this._error(error)); } }
    finally { this._formState(); this._refresh(); }
  }
  _flow(title, text) { this.$("progress").hidden = false; this.$("phase-title").textContent = title; this.$("phase-text").textContent = text; this.$("countdown").textContent = ""; this.$("bar").hidden = this._active?.phase !== "learning"; }
  _tick() {
    const active = this._active; if (active?.phase !== "learning") return;
    const remaining = Math.max(0, Math.ceil((active.deadline - Date.now()) / 1000)); this.$("countdown").textContent = `${remaining} s`; this.$("bar").value = remaining;
    if (!remaining) { this.$("phase-title").textContent = "Warte auf Bestätigung …"; this.$("phase-text").textContent = "Eine gespeicherte Aufnahme wird vom Lilygo bestätigt."; }
    if (Date.now() > active.deadline + 4000) { active.phase = "uncertain"; this._flow("Keine Aufnahme bestätigt", "Es liegt keine Erfolgsbestätigung vor. Gerätestatus prüfen und bei Bedarf erneut anlernen."); this._formState(); }
  }
  _event(event) {
    const data = event.data || {}, active = this._active;
    if (!active || !["starting", "learning"].includes(active.phase) || data.device_id !== active.device_id) return;
    const expected = active.type === "ir" ? "ir_learned" : "learned";
    if (data.event_type === expected && data.slot === active.slot) { active.phase = "saved"; this._flow("Gespeichert", `„${active.name}“ wurde auf Platz ${active.slot} aufgenommen.`); this._formState(); this._refresh(); }
    else if (data.event_type === "result" && data.ok === false && data.action === (active.type === "ir" ? "ir_learn" : "learn")) { active.phase = "error"; this._flow(data.error === "learn_timeout" ? "Keine Taste aufgenommen" : "Aufnahme fehlgeschlagen", this._error(data.error)); this._formState(); this._refresh(); }
  }
  _error(error) {
    const text = error?.message || String(error || "Unbekannter Fehler");
    const messages = { learn_timeout: "Innerhalb von 15 Sekunden wurde kein Code gespeichert. Bitte erneut versuchen.", storage_write_failed: "Die Aufnahme konnte nicht gespeichert werden. Codespeicher und Firmware prüfen.", ir_disabled: "Infrarot ist am Lilygo ausgeschaltet.", radio_error: "Der Lilygo meldet einen Funkfehler.", unavailable: "Der Lilygo ist zurzeit nicht erreichbar.", command_in_progress: "Ein anderer Befehl läuft noch. Bitte kurz warten." };
    const key = Object.keys(messages).find((item) => text.includes(item)); return key ? messages[key] : text;
  }
  async _close() {
    const active = this._active;
    if (this._locked()) { active.phase = "cancelling"; this._formState(); try { await this._hass.callService("lilygo_t_embed", "execute", { device_id: active.device_id, action: "stop" }); } catch (error) { active.phase = "error"; this._flow("Abbruch nicht bestätigt", this._error(error)); this._formState(); return; } }
    this.$("dialog").close(); this._active = null; this._cardSignature = ""; this._render(); this._refresh();
  }
  async _send(slot) {
    if (this._sending || this._locked()) return;
    const device = this._device(); if (!device?.available || this._busy(device)) return;
    const record = device[this._kind].find((item) => item.slot === slot); if (!record?.occupied) return;
    this._sending = true; this._pagePersistent = true; this._notice("Sendebefehl wird übertragen …"); this._render();
    try { await this._hass.callService("lilygo_t_embed", "send_code", { device_id: device.device_id, kind: this._kind, slot, repeats: this._kind === "ir" ? 0 : 1 }); this._notice(`„${record.name || "Code"}“ wurde gesendet.`, "success"); }
    catch (error) { this._notice(this._error(error), "error"); }
    finally { this._sending = false; this._render(); }
  }
}

if (!customElements.get("lilygo-learn-panel")) customElements.define("lilygo-learn-panel", LilygoLearnPanel);
