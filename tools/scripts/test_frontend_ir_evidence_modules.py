#!/usr/bin/env python3
"""Unit tests for the extracted PulpFrontendIR evidence helper modules.

These exercise the evidence families directly through their own modules
(frontend_ir_routes / _styles / _state / _nodes / _sources / _tokens /
_tweaks / _resources) rather than via frontend_ir_report, satisfying the
issue #3119 acceptance criterion that "evidence helpers can be unit-tested
independently". Keep cases small and deterministic — these are byte-stable
adapters over fixed inputs, not integration tests.
"""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import frontend_ir_nodes as nodes_mod  # noqa: E402
import frontend_ir_resources as resources_mod  # noqa: E402
import frontend_ir_routes as routes_mod  # noqa: E402
import frontend_ir_sources as sources_mod  # noqa: E402
import frontend_ir_state as state_mod  # noqa: E402
import frontend_ir_styles as styles_mod  # noqa: E402
import frontend_ir_tokens as tokens_mod  # noqa: E402
import frontend_ir_tweaks as tweaks_mod  # noqa: E402


class RoutesModuleTests(unittest.TestCase):
    def test_route_name_normalizes_aliases(self) -> None:
        self.assertEqual(routes_mod.route_name("native_host_service"), "native_cpp")
        self.assertEqual(routes_mod.route_name("native_layout"), "native_html")
        self.assertEqual(routes_mod.route_name("native_custom_paint"), "recorded_paint")
        self.assertEqual(routes_mod.route_name("live"), "live_js")
        self.assertEqual(routes_mod.route_name("hybrid"), "hybrid")
        self.assertEqual(routes_mod.route_name(None), "unsupported")
        self.assertEqual(routes_mod.route_name("totally-unknown"), "unsupported")

    def test_semantic_role_prefers_required_primitive(self) -> None:
        self.assertEqual(
            routes_mod.semantic_role({"required_native_primitive": "knob"}), "knob"
        )
        self.assertEqual(
            routes_mod.semantic_role({"source_component_family": "Range Input"}),
            "range_input",
        )
        self.assertEqual(routes_mod.semantic_role({}), "unknown")

    def test_row_node_id_falls_back_through_stable_path_then_synthetic(self) -> None:
        self.assertEqual(routes_mod.row_node_id({"id": "knob.1"}, 3), "knob.1")
        self.assertEqual(
            routes_mod.row_node_id({"stable_source_path": "f.jsx:1:Knob"}, 3),
            "f.jsx:1:Knob",
        )
        self.assertEqual(
            routes_mod.row_node_id(
                {"required_native_primitive": "fader", "source_line": 7}, 2
            ),
            "row.2.fader.7",
        )

    def test_routes_present_defaults_to_hybrid_when_empty(self) -> None:
        self.assertEqual(routes_mod.routes_present([]), ["hybrid"])
        self.assertEqual(
            routes_mod.routes_present(
                [{"route_type": "native_cpp"}, {"route_type": "native_layout"}]
            ),
            ["native_cpp", "native_html"],
        )

    def test_route_counts_tally_rows_and_metrics(self) -> None:
        manifest = {
            "route_metrics": {"nodes_total": 4, "behavior_pass": True},
            "component_family_coverage": {"classified": 2},
        }
        rows = [
            {"route_type": "native_cpp", "recorder_eligibility": "candidate"},
            {"route_type": "live", "fallback_reason": "needs js"},
        ]
        counts = routes_mod.route_counts(manifest, rows)
        self.assertEqual(counts["nodes_total"], 4)
        self.assertNotIn("behavior_pass", counts)  # booleans are not numeric evidence
        self.assertEqual(counts["component_family_classified"], 2)
        self.assertEqual(counts["route_rows_total"], 2)
        self.assertEqual(counts["route_rows_native_cpp"], 1)
        self.assertEqual(counts["route_rows_live_js"], 1)
        self.assertEqual(counts["route_rows_with_fallback_reason"], 1)
        self.assertEqual(counts["route_rows_recorder_candidate"], 1)

    def test_primitive_counts_tally_contract_presence(self) -> None:
        rows = [
            {
                "required_native_primitive": "knob",
                "parameter_bindings": [{"param_key": "gain"}],
                "event_contracts": [{"kind": "set_param"}],
            },
            {"required_native_primitive": "knob", "state_contracts": [{"state_key": "x"}]},
        ]
        counts = routes_mod.primitive_counts(rows)
        self.assertEqual(counts["primitive_knob"], 2)
        self.assertEqual(counts["with_parameter_bindings"], 1)
        self.assertEqual(counts["with_event_contracts"], 1)
        self.assertEqual(counts["with_state_contracts"], 1)


class SourcesModuleTests(unittest.TestCase):
    def test_count_map_pulls_summary_and_contract_rows(self) -> None:
        audit = {
            "lines": 20,
            "bytes": 1000,
            "summary": {"jsx_elements": 2, "map_calls": 1, "css_values": 5},
            "materiality": {"event_contracts": 1},
            "componentInvocationTemplates": {"Knob": 1, "Fader": 2},
            "styleKeys": ["width", "background"],
        }
        rows = [{"stable_source_path": "f.jsx:1:Knob", "state_contracts": [{}, {}]}]
        counts = sources_mod.count_map(audit, rows)
        self.assertEqual(counts["lines"], 20)
        self.assertEqual(counts["bytes"], 1000)
        self.assertEqual(counts["jsx_elements"], 2)
        self.assertEqual(counts["map_calls"], 1)
        self.assertEqual(counts["css_values"], 5)
        self.assertEqual(counts["materiality_event_contracts"], 1)
        self.assertEqual(counts["component_invocations"], 3)
        self.assertEqual(counts["style_keys"], 2)
        self.assertEqual(counts["source_contract_rows"], 1)
        self.assertEqual(counts["source_contract_rows_with_source_span"], 1)
        self.assertEqual(counts["source_contract_state_contracts"], 2)

    def test_count_map_rejects_booleans_as_numeric_evidence(self) -> None:
        counts = sources_mod.count_map({"lines": True, "summary": {"jsx_elements": True}})
        self.assertNotIn("lines", counts)
        self.assertNotIn("jsx_elements", counts)

    def test_dynamic_risks_flag_react_hooks_and_maps(self) -> None:
        self.assertIn("react_state_hooks", sources_mod.dynamic_risks({"useState": 1}))
        self.assertIn("runtime_array_maps", sources_mod.dynamic_risks({"map_calls": 3}))
        self.assertEqual(sources_mod.dynamic_risks({}), [])

    def test_source_span_omits_line_for_non_positive_int(self) -> None:
        span = sources_mod.source_span(
            {"stable_source_path": "f.jsx:10", "source_line": 10}, "n1"
        )
        self.assertEqual(span, {"node_id": "n1", "path": "f.jsx:10", "line": 10})
        self.assertIsNone(sources_mod.source_span({"source_line": 10}))
        bool_span = sources_mod.source_span(
            {"stable_source_path": "f.jsx", "source_line": True}, "n2"
        )
        self.assertNotIn("line", bool_span)


class StylesModuleTests(unittest.TestCase):
    def test_style_for_row_emits_layout_and_paint_layers(self) -> None:
        style = styles_mod.style_for_row(
            {"route_type": "native_cpp", "size": 48, "style_token_references": ["colors.accent"]}
        )
        self.assertEqual(style["layout"]["size"]["value"], 48)
        self.assertEqual(style["layout"]["size"]["support"]["native_cpp"], "present")
        self.assertEqual(style["paint_layers"][0]["value"], "colors.accent")

    def test_style_counts_aggregate_support_and_source_evidence(self) -> None:
        nodes = [{"style": styles_mod.style_for_row({"route_type": "native_cpp", "size": 48})}]
        counts = styles_mod.style_counts(nodes, {"css_values": 5, "style_keys": 2})
        self.assertEqual(counts["supported"], 1)
        self.assertEqual(counts["support_native_cpp_present"], 1)
        self.assertEqual(counts["support_live_js_planned"], 1)
        self.assertEqual(counts["source_css_values"], 5)
        self.assertEqual(counts["source_style_keys"], 2)


class StateModuleTests(unittest.TestCase):
    def test_state_for_row_maps_bindings_and_local_ui(self) -> None:
        state = state_mod.state_for_row(
            {
                "route_type": "native_cpp",
                "parameter_bindings": [
                    {"param_key": "gain", "binding_contract_id": "b.gain", "module": "OSC", "param": "gain"}
                ],
                "value": 0.5,
                "default_value": 0.0,
                "state_contracts": [{"kind": "set_state", "state_key": "threshold"}],
            }
        )
        param = state["parameters"][0]
        self.assertEqual(param["id"], "gain")
        self.assertEqual(param["source_binding_id"], "b.gain")
        self.assertEqual(param["value"], 0.5)
        self.assertEqual(param["range"]["default"], 0.0)
        self.assertEqual(state["local_ui"]["threshold"], "set_state")

    def test_state_counts_tally_parameter_facets(self) -> None:
        nodes = [
            {
                "state": state_mod.state_for_row(
                    {
                        "route_type": "native_cpp",
                        "parameter_bindings": [
                            {"param_key": "gain", "binding_contract_id": "b.gain", "module": "OSC", "param": "gain"}
                        ],
                        "value": 0.5,
                        "initial_value": 0.25,
                        "default_value": 0.0,
                    }
                )
            }
        ]
        counts = state_mod.state_counts(nodes)
        self.assertEqual(counts["parameters"], 1)
        self.assertEqual(counts["parameters_with_value"], 1)
        self.assertEqual(counts["parameters_with_initial_value"], 1)
        self.assertEqual(counts["parameters_with_default"], 1)
        self.assertEqual(counts["parameters_with_source_binding_id"], 1)
        self.assertEqual(counts["parameters_with_module_param"], 1)


class NodesModuleTests(unittest.TestCase):
    def test_nodes_from_rows_composes_role_style_state_and_span(self) -> None:
        nodes = nodes_mod.nodes_from_rows(
            [
                {
                    "id": "knob.1",
                    "stable_source_path": "f.jsx:10:Knob",
                    "source_line": 10,
                    "required_native_primitive": "knob",
                    "route_type": "native_cpp",
                }
            ]
        )
        self.assertEqual(nodes[0]["id"], "knob.1")
        self.assertEqual(nodes[0]["semantic_role"], "knob")
        self.assertIn("style", nodes[0])
        self.assertIn("state", nodes[0])
        self.assertEqual(nodes[0]["source_span"]["line"], 10)


class TokensModuleTests(unittest.TestCase):
    def test_token_key_strips_whitespace(self) -> None:
        self.assertEqual(tokens_mod.token_key("  colors.accent  "), "colors.accent")

    def test_tokens_from_rows_dedupes_and_sorts(self) -> None:
        tokens = tokens_mod.tokens_from_rows(
            [
                {"style_token_references": ["colors.b", "colors.a"]},
                {"style_token_references": ["colors.a"]},
            ]
        )
        self.assertEqual(list(tokens.keys()), ["colors.a", "colors.b"])
        self.assertEqual(tokens["colors.a"]["type"], "reference")
        self.assertEqual(
            tokens["colors.a"]["source_identity"]["provenance"], "style_token_references"
        )

    def test_token_counts_report_unresolved_and_referenced_rows(self) -> None:
        rows = [{"style_token_references": ["colors.a"]}, {"style_token_references": []}]
        tokens = tokens_mod.tokens_from_rows(rows)
        counts = tokens_mod.token_counts(tokens, rows)
        self.assertEqual(counts["total"], 1)
        self.assertEqual(counts["unresolved"], 1)
        self.assertEqual(counts["referenced_by_rows"], 1)


class TweaksModuleTests(unittest.TestCase):
    def test_tweak_counts_tally_classification_and_invalidations(self) -> None:
        tweaks = [
            {"classification_preserved": True, "invalidates": ["style"]},
            {"classification_preserved": False, "invalidates": ["route"]},
        ]
        counts = tweaks_mod.tweak_counts(tweaks)
        self.assertEqual(counts["total"], 2)
        self.assertEqual(counts["classification_preserved"], 1)
        self.assertEqual(counts["invalidates_style"], 1)
        self.assertEqual(counts["invalidates_route"], 1)

    def test_tweak_counts_empty(self) -> None:
        counts = tweaks_mod.tweak_counts([])
        self.assertEqual(counts, {"total": 0, "classification_preserved": 0})


class ResourcesModuleTests(unittest.TestCase):
    def test_artifact_ref_from_manifest_reads_path_and_sha(self) -> None:
        manifest = {"inputs": {"ir": {"path": "reports/ui-ir.json", "sha256": "b" * 64}}}
        ref = resources_mod.artifact_ref_from_manifest(manifest, "ir", "design_ir")
        self.assertEqual(ref, {"path": "reports/ui-ir.json", "kind": "design_ir", "sha256": "b" * 64})
        self.assertIsNone(resources_mod.artifact_ref_from_manifest({"inputs": {}}, "ir", "design_ir"))

    def test_binary_dependency_evidence_flags_js_engine(self) -> None:
        self.assertEqual(
            resources_mod.binary_dependency_evidence({"route_metrics": {"js_engine_initialized": True}}),
            {"js_engine_present": True, "source": "route_manifest_runtime_metrics"},
        )
        self.assertEqual(
            resources_mod.binary_dependency_evidence({"route_metrics": {"js_engine_initialized": False}}),
            {},
        )


if __name__ == "__main__":
    unittest.main()
