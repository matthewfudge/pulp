#!/usr/bin/env python3
import argparse
import re
import subprocess
import struct
import sys
import tempfile
from pathlib import Path


def write_fixture(tmpdir: Path) -> Path:
    bin_path = tmpdir / "mesh.bin"
    payload = bytearray()
    for value in (-1.0, -1.0, 0.0, 1.0, -1.0, 0.0, 0.0, 1.0, 0.0):
        payload.extend(struct.pack("<f", value))
    for value in (0, 1, 2):
        payload.extend(struct.pack("<H", value))
    payload.extend((0, 1, 2, 3))
    bin_path.write_bytes(payload)

    gltf_path = tmpdir / "draco.gltf"
    gltf_path.write_text(
        '{"asset":{"version":"2.0","generator":"pulp-draco-inspect-smoke"},'
        '"extensionsUsed":["KHR_draco_mesh_compression"],'
        '"extensionsRequired":["KHR_draco_mesh_compression"],'
        '"buffers":[{"uri":"mesh.bin","byteLength":46}],'
        '"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},'
        '{"buffer":0,"byteOffset":36,"byteLength":6,"target":34963},'
        '{"buffer":0,"byteOffset":42,"byteLength":4}],'
        '"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",'
        '"min":[-1,-1,0],"max":[1,1,0]},'
        '{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],'
        '"meshes":[{"name":"compressed","primitives":[{"attributes":{"POSITION":0},'
        '"indices":1,"mode":4,"extensions":{"KHR_draco_mesh_compression":{"bufferView":2,'
        '"attributes":{"POSITION":7,"NORMAL":8,"TEXCOORD_0":9}}}}]}],'
        '"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0}',
        encoding="utf-8",
    )
    return gltf_path


def run_inspect(inspect_tool: str, gltf_path: Path, *, native: bool):
    command = [inspect_tool]
    if native:
        command.append("--native-draco")
    command.append(str(gltf_path))
    return subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )


def expect_exit_2(name: str, result) -> bool:
    if result.returncode == 2:
        return True
    print(
        f"{name}: expected exit code 2, got {result.returncode}",
        file=sys.stderr)
    return False


def parse_native_draco_availability(output: str) -> bool | None:
    matches = re.findall(r"^native_draco_decoder_available=(true|false)$",
                         output,
                         re.MULTILINE)
    if len(matches) != 1:
        return None
    return matches[0] == "true"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--inspect-tool", required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="pulp-draco-inspect-") as tmp:
        gltf_path = write_fixture(Path(tmp))
        default_result = run_inspect(args.inspect_tool, gltf_path, native=False)
        native_result = run_inspect(args.inspect_tool, gltf_path, native=True)

    sys.stdout.write(default_result.stdout)
    if not expect_exit_2("default inspector", default_result):
        return 1
    if "gltf.draco_not_wired" not in default_result.stdout:
        print("default inspector did not report gltf.draco_not_wired", file=sys.stderr)
        return 1
    print("draco_inspect_default_not_wired=true")

    sys.stdout.write(native_result.stdout)
    if not expect_exit_2("native inspector", native_result):
        return 1
    native_draco_available = parse_native_draco_availability(native_result.stdout)
    if native_draco_available is None:
        print("missing or ambiguous native Draco availability status", file=sys.stderr)
        return 1
    if native_draco_available:
        expected = "gltf.draco_decode_failed"
    else:
        expected = "gltf.draco_unavailable"
    if expected not in native_result.stdout:
        print(f"missing native Draco diagnostic: {expected}", file=sys.stderr)
        return 1
    if "gltf.draco_not_wired" in native_result.stdout:
        print("inspector did not route through the native Draco callback", file=sys.stderr)
        return 1
    print("draco_inspect_native_callback=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
