#!/usr/bin/env python3
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def write_fake_inspect(path: Path, default_exit: int, default_output: str,
                       native_exit: int, native_output: str) -> None:
    path.write_text(
        "#!/usr/bin/env python3\n"
        "import sys\n"
        f"default_exit = {default_exit!r}\n"
        f"default_output = {default_output!r}\n"
        f"native_exit = {native_exit!r}\n"
        f"native_output = {native_output!r}\n"
        "if '--native-draco' in sys.argv:\n"
        "    sys.stdout.write(native_output)\n"
        "    raise SystemExit(native_exit)\n"
        "sys.stdout.write(default_output)\n"
        "raise SystemExit(default_exit)\n",
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
    print(f"draco_inspect_smoke_contract_case={name}")
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
    parser.add_argument("--smoke", required=True)
    args = parser.parse_args()
    smoke = Path(args.smoke)

    default_ok = "diagnostic code=gltf.draco_not_wired\n"
    native_false_ok = (
        "native_draco_decoder_available=false\n"
        "diagnostic code=gltf.draco_unavailable\n"
    )
    native_true_ok = (
        "native_draco_decoder_available=true\n"
        "diagnostic code=gltf.draco_decode_failed\n"
    )

    cases = [
        ("valid-native-disabled", 2, default_ok, 2, native_false_ok, True),
        ("valid-native-enabled", 2, default_ok, 2, native_true_ok, True),
        ("default-exit-drift", 0, default_ok, 2, native_false_ok, False),
        ("default-diagnostic-drift", 2, "scene clean\n", 2, native_false_ok, False),
        ("native-exit-drift", 2, default_ok, 0, native_false_ok, False),
        ("native-missing-availability", 2, default_ok, 2,
         "diagnostic code=gltf.draco_unavailable\n", False),
        ("native-ambiguous-availability", 2, default_ok, 2,
         "native_draco_decoder_available=false\n"
         "native_draco_decoder_available=true\n"
         "diagnostic code=gltf.draco_unavailable\n", False),
        ("native-disabled-diagnostic-drift", 2, default_ok, 2,
         "native_draco_decoder_available=false\n"
         "diagnostic code=gltf.draco_decode_failed\n", False),
        ("native-enabled-diagnostic-drift", 2, default_ok, 2,
         "native_draco_decoder_available=true\n"
         "diagnostic code=gltf.draco_unavailable\n", False),
        ("native-not-wired-leak", 2, default_ok, 2,
         "native_draco_decoder_available=false\n"
         "diagnostic code=gltf.draco_unavailable\n"
         "diagnostic code=gltf.draco_not_wired\n", False),
    ]

    with tempfile.TemporaryDirectory(prefix="pulp-draco-contract-") as tmp:
        tmpdir = Path(tmp)
        for index, (name, default_exit, default_output, native_exit,
                    native_output, should_pass) in enumerate(cases):
            fake = tmpdir / f"fake-inspect-{index}.py"
            write_fake_inspect(fake, default_exit, default_output, native_exit,
                               native_output)
            if not require_case(name, smoke, fake, should_pass=should_pass):
                return 1

    print("draco_inspect_smoke_contract_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
