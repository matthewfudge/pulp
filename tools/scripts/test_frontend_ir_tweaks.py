#!/usr/bin/env python3
"""Tests for tools/scripts/frontend_ir_tweaks.py."""

from __future__ import annotations

import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


SCRIPT = pathlib.Path(__file__).resolve().parent / "frontend_ir_tweaks.py"
spec = importlib.util.spec_from_file_location("frontend_ir_tweaks", SCRIPT)
assert spec and spec.loader
tweaks = importlib.util.module_from_spec(spec)
sys.modules["frontend_ir_tweaks"] = tweaks
spec.loader.exec_module(tweaks)


class FrontendIrTweaksTests(unittest.TestCase):
    def test_loads_sidecar_and_defaults_style_invalidation(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            path = pathlib.Path(td) / "pulp-tweaks.json"
            path.write_text(json.dumps({
                "schema": "pulp-tweaks-v0",
                "tweaks": [
                    {
                        "node_id": "node-a",
                        "property": "tokens.color.accent",
                        "value": "#ff6600",
                    }
                ],
            }), encoding="utf-8")

            result = tweaks.tweaks_from_sidecar(path, {"node-a"})

            self.assertEqual(result[0]["node_id"], "node-a")
            self.assertEqual(result[0]["invalidates"], ["style"])
            self.assertTrue(result[0]["classification_preserved"])

    def test_route_invalidation_defaults_to_not_preserved(self) -> None:
        result = tweaks.normalize_tweak({
            "node_id": "node-a",
            "property": "route.chosen_route",
            "value": "live_js",
        }, 0, {"node-a"})

        self.assertEqual(result["invalidates"], ["route"])
        self.assertFalse(result["classification_preserved"])

    def test_rejects_unknown_node_id(self) -> None:
        with self.assertRaisesRegex(ValueError, "node_id is not present"):
            tweaks.normalize_tweak({
                "node_id": "missing",
                "property": "style.color",
                "value": "#fff",
            }, 0, {"node-a"})


if __name__ == "__main__":
    unittest.main()
