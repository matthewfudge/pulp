#!/usr/bin/env python3
"""Tests for tools/scripts/frontend_ir_primitive_gate.py."""

from __future__ import annotations

import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


SCRIPT = pathlib.Path(__file__).resolve().parent / "frontend_ir_primitive_gate.py"
spec = importlib.util.spec_from_file_location("frontend_ir_primitive_gate", SCRIPT)
assert spec and spec.loader
primitive_gate = importlib.util.module_from_spec(spec)
sys.modules["frontend_ir_primitive_gate"] = primitive_gate
spec.loader.exec_module(primitive_gate)


def sample_coverage() -> dict:
    return {
        "schema": "pulp-frontend-ir-primitive-coverage-v0",
        "fixture_id": "panel",
        "frontend_ir": {"path": "panel-frontend-ir.json"},
        "summary": {
            "catalog_primitives": 33,
            "observed_primitives": 2,
            "covered_catalog_primitives": 2,
            "observed_missing_from_catalog": [],
            "catalog_not_observed": ["knob"],
            "nodes": 2,
            "routes": 2,
            "nodes_with_source_span": 2,
            "nodes_with_style": 2,
            "nodes_with_binding": 1,
            "nodes_requiring_js": 0,
        },
        "source_counts": {},
        "primitive_counts": {},
        "primitives": [
            {
                "role": "button",
                "category": "general",
                "in_catalog": True,
                "expected_routes": ["native_html", "native_cpp"],
                "observed_routes": {"native_cpp": 1},
                "node_count": 1,
                "nodes_with_source_span": 1,
                "nodes_with_style": 1,
                "nodes_with_binding": 1,
                "nodes_requiring_js": 0,
                "node_samples": [],
            },
            {
                "role": "layout",
                "category": "layout",
                "in_catalog": True,
                "expected_routes": ["native_html", "native_cpp"],
                "observed_routes": {"native_html": 1},
                "node_count": 1,
                "nodes_with_source_span": 1,
                "nodes_with_style": 1,
                "nodes_with_binding": 0,
                "nodes_requiring_js": 0,
                "node_samples": [],
            },
        ],
    }


class FrontendIrPrimitiveGateTests(unittest.TestCase):
    def test_coverage_gate_ready_for_cataloged_native_primitives(self) -> None:
        gate = primitive_gate.gate_primitive_coverage(sample_coverage(), "coverage")

        self.assertEqual(gate["schema"], "pulp-frontend-ir-primitive-gate-v0")
        self.assertEqual(gate["fixture_id"], "panel")
        self.assertEqual(gate["mode"], "coverage")
        self.assertEqual(gate["verdict"], "ready")
        self.assertEqual(gate["summary"]["failures"], 0)
        self.assertIn("no_unknown_primitives", {item["id"] for item in gate["checks"]})

    def test_unknown_catalog_role_fails_coverage(self) -> None:
        report = sample_coverage()
        report["summary"]["observed_missing_from_catalog"] = ["custom_widget"]
        report["primitives"][0]["role"] = "custom_widget"
        report["primitives"][0]["in_catalog"] = False
        report["primitives"][0]["expected_routes"] = []

        gate = primitive_gate.gate_primitive_coverage(report, "coverage")

        self.assertEqual(gate["verdict"], "not_ready")
        failures = {item["id"] for item in gate["checks"] if item["status"] == "fail"}
        self.assertIn("no_unknown_primitives", failures)

    def test_route_mismatch_warns_without_becoming_classifier_policy(self) -> None:
        report = sample_coverage()
        report["primitives"][1]["observed_routes"] = {"live_js": 1}

        gate = primitive_gate.gate_primitive_coverage(report, "coverage")

        self.assertEqual(gate["verdict"], "ready")
        warnings = {item["id"] for item in gate["checks"] if item["status"] == "warn"}
        self.assertIn("expected_route_alignment", warnings)

    def test_native_readiness_fails_js_required_primitives(self) -> None:
        report = sample_coverage()
        report["summary"]["nodes_requiring_js"] = 1
        report["primitives"][1]["observed_routes"] = {"live_js": 1}
        report["primitives"][1]["nodes_requiring_js"] = 1

        gate = primitive_gate.gate_primitive_coverage(report, "native-readiness")

        self.assertEqual(gate["verdict"], "not_ready")
        failures = {item["id"] for item in gate["checks"] if item["status"] == "fail"}
        self.assertIn("native_no_js_primitives", failures)
        self.assertIn("native_routes_only", failures)

    def test_cli_writes_gate_report_and_respects_allow_not_ready(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            primitive_coverage = root / "primitive-coverage.json"
            output = root / "primitive-gate.json"
            report = sample_coverage()
            report["summary"]["observed_missing_from_catalog"] = ["custom_widget"]
            primitive_coverage.write_text(json.dumps(report), encoding="utf-8")

            rc = primitive_gate.main([
                "--primitive-coverage",
                str(primitive_coverage),
                "--output",
                str(output),
                "--allow-not-ready",
            ])

            self.assertEqual(rc, 0)
            written = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(written["verdict"], "not_ready")
            self.assertEqual(written["summary"]["failures"], 1)

    def test_cli_returns_failure_for_not_ready_without_override(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            primitive_coverage = root / "primitive-coverage.json"
            output = root / "primitive-gate.json"
            report = sample_coverage()
            report["summary"]["nodes"] = 2
            report["summary"]["routes"] = 1
            primitive_coverage.write_text(json.dumps(report), encoding="utf-8")

            rc = primitive_gate.main([
                "--primitive-coverage",
                str(primitive_coverage),
                "--output",
                str(output),
            ])

            self.assertEqual(rc, 1)


if __name__ == "__main__":
    unittest.main()
