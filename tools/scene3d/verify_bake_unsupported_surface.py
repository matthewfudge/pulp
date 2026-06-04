#!/usr/bin/env python3
"""Pins the bake/export unsupported-feature sidecar handoff surface."""

import argparse
import re
import sys
from pathlib import Path


EXPECTED_ENUM_VALUES = [
    "shader_material",
    "raw_shader_material",
    "postprocessing",
    "render_target",
    "arbitrary_js_animation",
    "physics",
    "event_handler",
    "texture_encoding_missing",
]

EXPECTED_DESCRIPTORS = [
    (
        "shader_material",
        "ShaderMaterial",
        "bake.unsupported_shader_material",
        "THREE.ShaderMaterial",
    ),
    (
        "raw_shader_material",
        "RawShaderMaterial",
        "bake.unsupported_raw_shader_material",
        "THREE.RawShaderMaterial",
    ),
    (
        "postprocessing",
        "Postprocessing",
        "bake.unsupported_postprocessing",
        "Postprocessing and EffectComposer",
    ),
    (
        "render_target",
        "RenderTarget",
        "bake.unsupported_render_target",
        "Render targets are runtime framebuffer state",
    ),
    (
        "arbitrary_js_animation",
        "ArbitraryJSAnimation",
        "bake.unsupported_arbitrary_js_animation",
        "Arbitrary JavaScript animation",
    ),
    (
        "physics",
        "Physics",
        "bake.unsupported_physics",
        "Physics simulation",
    ),
    (
        "event_handler",
        "EventHandler",
        "bake.unsupported_event_handler",
        "Pointer, keyboard, and custom event handlers",
    ),
    (
        "texture_encoding_missing",
        "TextureEncoding",
        "bake.texture_encoding_missing",
        "toDataURL/toBlob",
    ),
]


def extract_enum_values(header):
    match = re.search(
        r"enum\s+class\s+BakeUnsupportedFeature\s*\{(?P<body>.*?)\n\};",
        header,
        re.DOTALL,
    )
    if not match:
        raise ValueError("missing BakeUnsupportedFeature enum")
    values = []
    for line in match.group("body").splitlines():
        stripped = line.strip().rstrip(",")
        if stripped:
            values.append(stripped)
    return values


def extract_function_body(source, signature):
    start = source.find(signature)
    if start < 0:
        raise ValueError(f"missing function {signature}")
    brace = source.find("{", start)
    if brace < 0:
        raise ValueError(f"missing body for {signature}")
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise ValueError(f"unterminated body for {signature}")


def extract_descriptor_cases(source):
    body = extract_function_body(
        source,
        "BakeUnsupportedDescriptor bake_descriptor(BakeUnsupportedFeature feature)",
    )
    pattern = re.compile(
        r"case\s+BakeUnsupportedFeature::(?P<enum>[A-Za-z0-9_]+):\s*"
        r"return\s+BakeUnsupportedDescriptor\s*\{\s*"
        r'"(?P<feature>[^"]+)"\s*,\s*'
        r'"(?P<code>[^"]+)"\s*,\s*'
        r'"(?P<reason>[^"]+)"\s*,\s*'
        r"\};",
        re.DOTALL,
    )
    return [
        (
            match.group("enum"),
            match.group("feature"),
            match.group("code"),
            match.group("reason"),
        )
        for match in pattern.finditer(body)
    ]


def require_subsequence(text, snippets, label):
    position = 0
    errors = []
    for snippet in snippets:
        next_position = text.find(snippet, position)
        if next_position == -1:
            errors.append(f"{label}: missing ordered snippet {snippet!r}")
        else:
            position = next_position + len(snippet)
    return errors


def verify_descriptor_cases(source):
    cases = extract_descriptor_cases(source)
    expected_keys = [(item[0], item[1], item[2]) for item in EXPECTED_DESCRIPTORS]
    actual_keys = [(item[0], item[1], item[2]) for item in cases]
    errors = []
    if actual_keys != expected_keys:
        errors.append(
            f"bake_descriptor cases drifted: expected {expected_keys!r}, "
            f"got {actual_keys!r}"
        )
    reason_by_enum = {item[0]: item[3] for item in cases}
    for enum_name, _, _, reason_substring in EXPECTED_DESCRIPTORS:
        reason = reason_by_enum.get(enum_name, "")
        if reason_substring not in reason:
            errors.append(
                f"bake_descriptor {enum_name} reason lost "
                f"{reason_substring!r}: {reason!r}"
            )
    return errors


def verify_append_function(source):
    body = extract_function_body(
        source,
        "void append_bake_unsupported_feature(SidecarData& sidecar,",
    )
    snippets = [
        "const auto descriptor = bake_descriptor(feature);",
        "sidecar.unsupported_features.push_back(UnsupportedFeature{",
        "descriptor.feature,",
        "descriptor.reason,",
        "node_path,",
        "append_diagnostic(sidecar.diagnostics,",
        "Diagnostic::Severity::warning,",
        "descriptor.code,",
        "descriptor.reason,",
        "source_path);",
    ]
    return require_subsequence(body, snippets, "append_bake_unsupported_feature")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    args = parser.parse_args()

    header = args.header.read_text(encoding="utf-8")
    source = args.source.read_text(encoding="utf-8")
    errors = []

    enum_values = extract_enum_values(header)
    if enum_values != EXPECTED_ENUM_VALUES:
        errors.append(
            f"BakeUnsupportedFeature enum drifted: expected "
            f"{EXPECTED_ENUM_VALUES!r}, got {enum_values!r}"
        )
    errors.extend(verify_descriptor_cases(source))
    errors.extend(verify_append_function(source))

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("bake_unsupported_surface_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
