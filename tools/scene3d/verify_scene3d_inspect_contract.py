#!/usr/bin/env python3
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


STATS_LINE = (
    "nodes=2 roots=1 meshes=1 primitives=1 indexed_primitives=1 "
    "vertices=24 indices=36 materials=1 textures=1 texture_samplers=1 "
    "texture_bytes=3750 advanced_material_extensions=0 cameras=0 lights=0 "
    "animations=0 unsupported_features=0 diagnostics=0 error_diagnostics=0"
)

RENDER_PACKET_LINE = (
    "render_packet transformed_nodes=2 primitives=1 diagnostics=0 "
    "has_errors=false"
)

PRIMITIVE_LINE = (
    "primitive node=1 mesh=0 primitive=0 material=0 feature_mask=15 "
    "world_transform=1,0,0,0,0,0,-1,0,0,1,0,0,0,0,0,1 "
    "features=normals,texcoord0,indexed,base_color_texture"
)


def fake_inspect_source(lines):
    body = [
        "#!/usr/bin/env python3",
        "import sys",
        "print(" + repr("\n".join(lines) + "\n") + ", end='')",
        "raise SystemExit(0)",
    ]
    return "\n".join(body) + "\n"


def run_inspect_verifier(verifier, inspect_tool, fixture):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--inspect-tool",
            str(inspect_tool),
            "--fixture",
            str(fixture),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def expect_result(name, result, expected_code, expected_text, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != expected_code:
        errors.append(
            f"{name}: expected exit {expected_code}, got {result.returncode}")
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected diagnostic containing {expected_text!r}")
    print(f"scene3d_inspect_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify scene3d inspect verifier rejects malformed output.")
    parser.add_argument("--inspect-verifier", type=Path, required=True)
    args = parser.parse_args()

    if not args.inspect_verifier.exists():
        print(f"inspect_verifier_exists=false path={args.inspect_verifier}")
        return 2

    fixture = Path(__file__)
    cases = [
        (
            "valid-fake-inspect",
            [STATS_LINE, RENDER_PACKET_LINE, PRIMITIVE_LINE],
            0,
            "scene3d_inspect_verified=boxtextured",
        ),
        (
            "missing-render-packet",
            [STATS_LINE, PRIMITIVE_LINE],
            1,
            "render_packet.count: expected 1, got 0",
        ),
        (
            "duplicate-primitive",
            [STATS_LINE, RENDER_PACKET_LINE, PRIMITIVE_LINE, PRIMITIVE_LINE],
            1,
            "primitive.count: expected 1, got 2",
        ),
        (
            "stats-drift",
            [
                STATS_LINE.replace("textures=1", "textures=0"),
                RENDER_PACKET_LINE,
                PRIMITIVE_LINE,
            ],
            1,
            "stats.textures: expected '1', got '0'",
        ),
        (
            "primitive-feature-drift",
            [
                STATS_LINE,
                RENDER_PACKET_LINE,
                PRIMITIVE_LINE.replace("feature_mask=15", "feature_mask=7"),
            ],
            1,
            "primitive.feature_mask: expected '15', got '7'",
        ),
    ]

    errors = []
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        for name, lines, expected_code, expected_text in cases:
            inspect_tool = temp_path / f"{name}.py"
            inspect_tool.write_text(fake_inspect_source(lines), encoding="utf-8")
            inspect_tool.chmod(0o755)
            result = run_inspect_verifier(args.inspect_verifier,
                                          inspect_tool,
                                          fixture)
            expect_result(name,
                          result,
                          expected_code,
                          expected_text,
                          errors)

    if errors:
        for error in errors:
            print(f"error={error}")
        return 1
    print("scene3d_inspect_contract_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
