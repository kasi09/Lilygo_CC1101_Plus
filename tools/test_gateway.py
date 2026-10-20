"""Host checks for backup restore boundaries; never opens a real serial port."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

import gateway


class RestoreTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "backup.jsonl"

    def backup(self, records):
        self.path.write_text("\n".join(json.dumps(x) for x in
            [{"event": "backup", "version": "0.3.0"}] + records), encoding="utf-8")

    def test_transmission_and_other_actions_rejected(self):
        for action in ("send", "ir_send", "nrf_send", "feature_set", "delete", "backup"):
            with self.subTest(action=action):
                self.backup([{"action": action, "slot": 0}])
                with self.assertRaises(ValueError):
                    gateway.restore_commands(self.path)

    def test_slot_types_bounds_and_duplicates(self):
        for action, slot in (("store", -1), ("store", 16), ("ir_store", 8), ("store", True), ("store", "0")):
            with self.subTest(action=action, slot=slot):
                self.backup([{"action": action, "slot": slot}])
                with self.assertRaises(ValueError):
                    gateway.restore_commands(self.path)
        self.backup([{"action": "store", "slot": 0}] * 2)
        with self.assertRaises(ValueError):
            gateway.restore_commands(self.path)

    def test_mixed_rf_ir_and_empty_backup(self):
        records = [{"action": "store", "slot": 15}, {"action": "ir_store", "slot": 7}]
        self.backup(records)
        self.assertEqual(gateway.restore_commands(self.path), records)
        self.backup([])
        self.assertEqual(gateway.restore_commands(self.path), [])

    def test_header_and_buffer_limit(self):
        self.path.write_text('{"action":"store","slot":0}', encoding="utf-8")
        with self.assertRaises(ValueError):
            gateway.restore_commands(self.path)
        self.backup([{"action": "store", "slot": 0, "name": "x" * 4096}])
        with self.assertRaises(ValueError):
            gateway.restore_commands(self.path)

    def simulate_restore(self, fail_at=None):
        records = [{"action": "store", "slot": 0}, {"action": "ir_store", "slot": 1}, {"action": "store", "slot": 2}]
        self.backup(records)
        sent = []

        class Port:
            def __init__(self, **kwargs):
                self.response = b""

            def __enter__(self):
                return self

            def __exit__(self, *args):
                return False

            def reset_input_buffer(self):
                pass

            def write(self, payload):
                sent.append(json.loads(payload))
                answer = {"event": "result", "action": sent[-1]["action"], "ok": len(sent) != fail_at}
                if not answer["ok"]:
                    answer["error"] = "storage_write_failed"
                self.response = (json.dumps(answer) + "\n").encode()

            def read_until(self, *args, **kwargs):
                response, self.response = self.response, b""
                return response

        serial = types.SimpleNamespace(Serial=Port, SerialException=OSError)
        with patch.dict(sys.modules, serial=serial), patch.object(sys, "argv",
                ["gateway.py", "--port", "FAKE", "restore", "--file", str(self.path)]), \
                patch.object(gateway.time, "sleep"), contextlib.redirect_stdout(io.StringIO()), \
                contextlib.redirect_stderr(io.StringIO()):
            code = gateway.main()
        return code, sent

    def test_restore_waits_for_successful_acknowledgements(self):
        code, sent = self.simulate_restore()
        self.assertEqual(code, 0)
        self.assertEqual(len(sent), 3)
        self.assertTrue(all(x["action"] in ("store", "ir_store") for x in sent))

    def test_restore_stops_after_first_device_error(self):
        code, sent = self.simulate_restore(fail_at=2)
        self.assertEqual(code, 1)
        self.assertEqual(len(sent), 2)


if __name__ == "__main__":
    unittest.main()
