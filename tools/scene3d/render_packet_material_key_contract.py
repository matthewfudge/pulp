#!/usr/bin/env python3
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


FEATURES = (
    "normals,texcoord0,indexed,base_color_texture,unlit,double_sided,"
    "alpha_blend,metallic_roughness_texture,normal_texture,occlusion_texture,"
    "emissive_texture,alpha_mask,tangents,texcoord1,color0,"
    "base_color_texture_transform,non_base_color_texture_transform,"
    "non_base_color_texcoord1,normal_scale,occlusion_strength"
)
STATS = (
    "nodes=1 roots=1 meshes=1 primitives=1 indexed_primitives=1 vertices=3 "
    "indices=3 materials=1 textures=5 texture_samplers=2 texture_bytes=40 "
    "advanced_material_extensions=0 cameras=0 lights=0 animations=0 "
    "unsupported_features=0 diagnostics=0 error_diagnostics=0\n"
)
PACKET = (
    "render_packet transformed_nodes=1 primitives=1 diagnostics=0 "
    "has_errors=false\n"
)
PRIMITIVE = (
    "primitive node=0 mesh=0 primitive=0 material=0 feature_mask=2097023 "
    "world_transform=1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 "
    f"features={FEATURES}\n"
)


def write_fake_inspect(path: Path, output: str, exit_code: int = 0) -> None:
    path.write_text(
        "#!/usr/bin/env python3\n"
        "import sys\n"
        f"sys.stdout.write({output!r})\n"
        f"raise SystemExit({exit_code!r})\n",
        encoding="utf-8",
    )
    path.chmod(0o755)


def run_case(smoke: Path, fake: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(smoke), "--inspect-tool", str(fake)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )


def require_case(name: str, smoke: Path, fake: Path, *,
                 should_pass: bool) -> bool:
    result = run_case(smoke, fake)
    passed = result.returncode == 0
    print(f"render_packet_material_key_contract_case={name}")
    sys.stdout.write(result.stdout)
    if passed != should_pass:
        expected = "pass" if should_pass else "fail"
        print(
            f"{name}: expected smoke to {expected}, got exit {result.returncode}",
            file=sys.stderr,
        )
        return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--material-key-smoke", required=True)
    args = parser.parse_args()
    smoke = Path(args.material_key_smoke)

    valid = STATS + PACKET + PRIMITIVE
    cases = [
        ("valid-fake-inspect", valid, 0, True),
        ("inspect-exit-drift", valid, 2, False),
        ("stats-texture-count-drift", STATS.replace("textures=5", "textures=4") + PACKET + PRIMITIVE, 0, False),
        ("stats-texture-bytes-drift", STATS.replace("texture_bytes=40", "texture_bytes=8") + PACKET + PRIMITIVE, 0, False),
        ("stats-extra-key", STATS.rstrip() + " extras=1\n" + PACKET + PRIMITIVE, 0, False),
        ("missing-render-packet", STATS + PRIMITIVE, 0, False),
        ("packet-has-errors-drift", STATS + PACKET.replace("has_errors=false", "has_errors=true") + PRIMITIVE, 0, False),
        ("missing-primitive", STATS + PACKET, 0, False),
        ("duplicate-primitive", STATS + PACKET + PRIMITIVE + PRIMITIVE, 0, False),
        ("feature-mask-drift", STATS + PACKET + PRIMITIVE.replace("feature_mask=2097023", "feature_mask=2097022"), 0, False),
        ("feature-name-missing", STATS + PACKET + PRIMITIVE.replace(",occlusion_strength", ""), 0, False),
        ("feature-name-order-drift", STATS + PACKET + PRIMITIVE.replace("normals,texcoord0,indexed", "texcoord0,normals,indexed"), 0, False),
        ("material-index-drift", STATS + PACKET + PRIMITIVE.replace("material=0", "material=4294967295"), 0, False),
        ("world-transform-drift", STATS + PACKET + PRIMITIVE.replace("1,0,0,0,0,1", "2,0,0,0,0,1", 1), 0, False),
    ]

    with tempfile.TemporaryDirectory(prefix="pulp-render-packet-material-key-") as tmp:
        tmpdir = Path(tmp)
        for index, (name, output, exit_code, should_pass) in enumerate(cases):
            fake = tmpdir / f"fake-inspect-{index}.py"
            write_fake_inspect(fake, output, exit_code)
            if not require_case(name, smoke, fake, should_pass=should_pass):
                return 1

    print("render_packet_material_key_contract_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
