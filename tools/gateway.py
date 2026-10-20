"""USB control and backup restore for LILYGO T-Embed CC1101 Plus."""
import argparse
import json
import math
from pathlib import Path
import sys
import time


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM7")
    parser.add_argument("--seconds", type=float, default=20,
                        help="Listen duration after command; 0 = until Ctrl+C")
    parser.add_argument("--output", type=Path, help="Append received JSON to a JSONL file")
    sub = parser.add_subparsers(dest="action", required=True)
    for action in ("watch", "status", "list", "stop", "ir-list", "backup", "probe-features", "nrf-scan", "setup-info"):
        sub.add_parser(action)
    setup = sub.add_parser("network-setup", help="Open or close the local setup hotspot")
    setup.add_argument("--state", choices=("on", "off"), default="on")
    learn = sub.add_parser("learn")
    learn.add_argument("--slot", type=int, required=True, choices=range(16))
    learn.add_argument("--kind", choices=("rc", "raw"), default="rc")
    learn.add_argument("--name", default="Code")
    send = sub.add_parser("send")
    send.add_argument("--slot", type=int, required=True, choices=range(16))
    send.add_argument("--repeats", type=int, default=6, choices=range(1, 11))
    delete = sub.add_parser("delete")
    delete.add_argument("--slot", type=int, required=True, choices=range(16))
    tune = sub.add_parser("tune")
    tune.add_argument("--frequency", type=float, required=True)
    scan = sub.add_parser("scan")
    scan.add_argument("--band", choices=("433", "868", "all"), default="433")
    store = sub.add_parser("store", help="Import one remote JSON object, as emitted by list")
    store.add_argument("--file", type=Path, required=True)
    store.add_argument("--slot", type=int, required=True, choices=range(16))
    for action in ("ir-learn", "ir-send", "ir-delete"):
        ir = sub.add_parser(action)
        ir.add_argument("--slot", type=int, required=True, choices=range(8))
        if action == "ir-learn":
            ir.add_argument("--name", default="IR Code")
        if action == "ir-send":
            ir.add_argument("--repeats", type=int, default=0, choices=range(4))
    feature = sub.add_parser("feature")
    feature.add_argument("--name", required=True, choices=("nfc", "ir", "nrf", "microphone"))
    feature.add_argument("--state", required=True, choices=("on", "off"))
    custom = sub.add_parser("command", help="Send one command JSON object from a file")
    custom.add_argument("--file", type=Path, required=True)
    restore = sub.add_parser("restore", help="Restore SD JSONL backup; overwrites its occupied slots, never transmits")
    restore.add_argument("--file", type=Path, required=True)
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or args.seconds < 0:
        parser.error("--seconds must be finite and non-negative")
    return args


def make_command(args):
    if args.action == "watch":
        return None
    if args.action == "command":
        doc = json.loads(args.file.read_text(encoding="utf-8-sig"))
        if not isinstance(doc, dict) or not isinstance(doc.get("action"), str):
            raise ValueError("command file must contain one object with an action")
        return doc
    if args.action == "feature":
        return {"action": "feature_set", "feature": args.name, "enabled": args.state == "on"}
    if args.action == "network-setup":
        return {"action": "network_setup", "enabled": args.state == "on"}
    if args.action == "store":
        doc = json.loads(args.file.read_text(encoding="utf-8"))
        if not isinstance(doc, dict):
            raise ValueError("remote file must contain one JSON object")
        doc.update(action="store", slot=args.slot)
        return doc
    doc = {"action": args.action.replace("-", "_")}
    for field in ("slot", "kind", "name", "repeats", "band"):
        if hasattr(args, field):
            doc[field] = getattr(args, field)
    if args.action == "tune":
        doc["frequency_mhz"] = args.frequency
    return doc


def restore_commands(path):
    """Validate the whole file before opening a port. Never restore executable actions."""
    commands, seen = [], set()
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    if not lines:
        raise ValueError("backup is empty")
    header = json.loads(lines[0])
    if not isinstance(header, dict) or header.get("event") != "backup" or "action" in header:
        raise ValueError("missing SD backup header")
    for number, line in enumerate(lines[1:], 2):
        if not line.strip():
            continue
        doc = json.loads(line)
        if not isinstance(doc, dict) or doc.get("action") not in ("store", "ir_store"):
            raise ValueError(f"line {number}: only store/ir_store are accepted")
        slot = doc.get("slot")
        limit = 16 if doc["action"] == "store" else 8
        if type(slot) is not int or not 0 <= slot < limit:
            raise ValueError(f"line {number}: invalid slot")
        identity = doc["action"], slot
        if identity in seen:
            raise ValueError(f"line {number}: duplicate slot")
        seen.add(identity)
        if len(json.dumps(doc, ensure_ascii=True, separators=(",", ":")).encode()) > 4096:
            raise ValueError(f"line {number}: command exceeds device buffer")
        commands.append(doc)
    return commands


def main():
    args = arguments()
    try:
        import serial
    except ImportError:
        print("pyserial fehlt. Installieren: python -m pip install pyserial", file=sys.stderr)
        return 2
    output = None
    try:
        command = None if args.action == "restore" else make_command(args)
        restoring = restore_commands(args.file) if args.action == "restore" else []
        if command and len(json.dumps(command, ensure_ascii=True, separators=(",", ":")).encode()) > 4096:
            raise ValueError("command exceeds the 4096 byte device buffer")
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            output = args.output.open("a", encoding="utf-8")
        connection = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=2)
        connection.dtr = False
        connection.rts = False
        connection.port = args.port
        with connection:
            time.sleep(1)
            if args.action == "restore":
                connection.reset_input_buffer()
                for item in restoring:
                    connection.write((json.dumps(item, ensure_ascii=True, separators=(",", ":")) + "\n").encode())
                    deadline = time.monotonic() + 5
                    acknowledged = False
                    pending = bytearray()
                    while time.monotonic() < deadline:
                        pending.extend(connection.read_until(b"\n", size=8192))
                        if len(pending) > 8192:
                            raise ValueError("oversized response during restore")
                        if not pending.endswith(b"\n"):
                            continue
                        line = pending.decode("utf-8", errors="replace").strip()
                        pending.clear()
                        print(line, flush=True)
                        try:
                            response = json.loads(line)
                        except ValueError:
                            continue
                        if output:
                            output.write(json.dumps(response, ensure_ascii=False) + "\n"); output.flush()
                        if isinstance(response, dict) and response.get("event") == "result" and response.get("action") == item["action"]:
                            if response.get("ok") is not True:
                                raise ValueError(f"restore stopped at {item['action']} slot {item['slot']}: {response.get('error')}")
                            acknowledged = True
                            break
                    if not acknowledged:
                        raise ValueError(f"restore acknowledgement timeout at slot {item['slot']}")
                print(f"Restored {len(restoring)} slots. Other slots were kept.")
                return 0
            if command:
                payload = json.dumps(command, ensure_ascii=True, separators=(",", ":"))
                connection.write((payload + "\n").encode("utf-8"))
            end = time.monotonic() + args.seconds if args.seconds else float("inf")
            pending = bytearray()
            while time.monotonic() < end:
                chunk = connection.read_until(b"\n", size=8192)
                if not chunk:
                    continue
                pending.extend(chunk)
                if len(pending) > 8192:
                    pending.clear()
                    print("Oversized serial line discarded", file=sys.stderr)
                    continue
                if not pending.endswith(b"\n"):
                    continue
                line = pending.decode("utf-8", errors="replace").strip()
                pending.clear()
                print(line, flush=True)
                try:
                    doc = json.loads(line)
                except ValueError:
                    continue
                if output:
                    output.write(json.dumps(doc, ensure_ascii=False) + "\n")
                    output.flush()
    except KeyboardInterrupt:
        return 0
    except (OSError, ValueError, serial.SerialException) as error:
        print(str(error), file=sys.stderr)
        return 1
    finally:
        if output:
            output.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
