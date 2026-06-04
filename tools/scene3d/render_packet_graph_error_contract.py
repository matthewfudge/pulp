#!/usr/bin/env python3
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


STATS = (
    "nodes=1 roots=1 meshes=1 primitives=1 indexed_primitives=1 vertices=3 "
    "indices=3 materials=0 textures=0 texture_samplers=0 texture_bytes=0 "
    "advanced_material_extensions=0 cameras=0 lights=0 animations=0 "
    "unsupported_features=0 diagnostics=0 error_diagnostics=0\n"
)
PACKET = (
    "render_packet transformed_nodes=1 primitives=0 diagnostics=1 "
    "has_errors=true\n"
)
DIAGNOSTIC = (
    "diagnostic error scene.graph_cycle cyclic node graph detected\n"
)
PRIMITIVE = (
    "primitive node=0 mesh=0 primitive=0 material=4294967295 feature_mask=132 "
    "world_transform=1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 "
    "features=indexed,material_fallback\n"
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
    print(f"render_packet_graph_error_contract_case={name}")
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
    parser.add_argument("--graph-error-smoke", required=True)
    args = parser.parse_args()
    smoke = Path(args.graph_error_smoke)

    valid = STATS + PACKET + DIAGNOSTIC
    cases = [
        ("valid-fake-inspect", valid, 0, True),
        ("inspect-exit-drift", valid, 2, False),
        ("stats-diagnostics-drift", STATS.replace("diagnostics=0", "diagnostics=1") + PACKET + DIAGNOSTIC, 0, False),
        ("stats-error-diagnostics-drift", STATS.replace("error_diagnostics=0", "error_diagnostics=1") + PACKET + DIAGNOSTIC, 0, False),
        ("missing-render-packet", STATS + DIAGNOSTIC, 0, False),
        ("packet-primitive-count-drift", STATS + PACKET.replace("primitives=0", "primitives=1") + DIAGNOSTIC, 0, False),
        ("packet-diagnostic-count-drift", STATS + PACKET.replace("diagnostics=1", "diagnostics=0") + DIAGNOSTIC, 0, False),
        ("packet-has-errors-drift", STATS + PACKET.replace("has_errors=true", "has_errors=false") + DIAGNOSTIC, 0, False),
        ("missing-diagnostic", STATS + PACKET, 0, False),
        ("diagnostic-severity-drift", STATS + PACKET + DIAGNOSTIC.replace("diagnostic error", "diagnostic warning"), 0, False),
        ("diagnostic-code-drift", STATS + PACKET + DIAGNOSTIC.replace("scene.graph_cycle", "scene.render_packet_empty"), 0, False),
        ("primitive-leak-after-error", STATS + PACKET + DIAGNOSTIC + PRIMITIVE, 0, False),
    ]

    with tempfile.TemporaryDirectory(prefix="pulp-render-packet-graph-error-") as tmp:
        tmpdir = Path(tmp)
        for index, (name, output, exit_code, should_pass) in enumerate(cases):
            fake = tmpdir / f"fake-inspect-{index}.py"
            write_fake_inspect(fake, output, exit_code)
            if not require_case(name, smoke, fake, should_pass=should_pass):
                return 1

    print("render_packet_graph_error_contract_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
