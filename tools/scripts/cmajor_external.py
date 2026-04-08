#!/usr/bin/env python3
"""Pulp helper for the MIT-safe external Cmajor toolchain lane."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

GUIDE_RELATIVE_PATH = "docs/guides/cmajor.md"
DEFAULT_TARGET = "cpp"
SUPPORTED_TARGETS = (
    "cpp",
    "clap",
    "javascript",
    "webaudio",
    "webaudio-html",
)
# Note: cmaj also supports --target juce, but JUCE output is not
# relevant for Pulp projects. Use "cpp" for Pulp integration.


class CmajorExternalError(RuntimeError):
    pass


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def resolve_patch(path_str: str) -> tuple[Path, Path, dict]:
    patch = Path(path_str).expanduser()
    if not patch.is_absolute():
        patch = repo_root() / patch
    patch = patch.resolve()

    if not patch.is_file():
        raise CmajorExternalError(f"patch file not found: {patch}")
    if patch.suffix != ".cmajorpatch":
        raise CmajorExternalError(f"expected a .cmajorpatch file: {patch}")

    try:
        manifest = json.loads(patch.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise CmajorExternalError(f"invalid patch JSON in {patch}: {exc}") from exc

    source_value = manifest.get("source")
    if not isinstance(source_value, str) or not source_value.strip():
        raise CmajorExternalError(f"patch manifest missing string 'source': {patch}")

    source = (patch.parent / source_value).resolve()
    if not source.is_file():
        raise CmajorExternalError(
            f"patch source listed in manifest does not exist: {source}"
        )

    return patch, source, manifest


def resolve_cmaj(explicit: str | None) -> str | None:
    candidates: list[str] = []
    if explicit:
        candidates.append(explicit)

    env_value = os.environ.get("CMAJ_BIN")
    if env_value:
        candidates.append(env_value)

    which_value = shutil.which("cmaj") or shutil.which("cmaj.exe")
    if which_value:
        candidates.append(which_value)

    for candidate in candidates:
        path = Path(candidate).expanduser()
        if path.is_file() and os.access(path, os.X_OK):
            return str(path.resolve())

    return None


def build_generate_command(
    cmaj_path: str,
    patch: Path,
    *,
    target: str,
    output: str,
    extra_args: list[str],
) -> list[str]:
    if target not in SUPPORTED_TARGETS:
        supported = ", ".join(SUPPORTED_TARGETS)
        raise CmajorExternalError(f"unsupported target '{target}' (supported: {supported})")

    command = [
        cmaj_path,
        "generate",
        f"--target={target}",
        f"--output={output}",
    ]
    command.extend(extra_args)
    command.append(str(patch))
    return command


def print_doctor_summary(
    *,
    patch: Path,
    source: Path,
    manifest: dict,
    cmaj_path: str | None,
) -> None:
    print(f"patch: {patch}")
    print(f"source: {source}")
    print(f"name: {manifest.get('name', '<unnamed>')}")
    print(f"cmaj: {cmaj_path if cmaj_path else 'missing'}")
    print("policy: Pulp supports Cmajor through an external toolchain only.")
    print(
        f"guide: {repo_root() / GUIDE_RELATIVE_PATH}"
    )


def cmd_doctor(args: argparse.Namespace) -> int:
    patch, source, manifest = resolve_patch(args.patch)
    cmaj_path = resolve_cmaj(args.cmaj)
    print_doctor_summary(patch=patch, source=source, manifest=manifest, cmaj_path=cmaj_path)

    if args.require_tool and not cmaj_path:
        print(
            "error: cmaj binary not found. Install it separately or set CMAJ_BIN/--cmaj.",
            file=sys.stderr,
        )
        return 2

    return 0


def cmd_generate(args: argparse.Namespace) -> int:
    patch, source, manifest = resolve_patch(args.patch)
    cmaj_path = resolve_cmaj(args.cmaj)
    if not cmaj_path and args.dry_run:
        cmaj_path = args.cmaj or os.environ.get("CMAJ_BIN") or "cmaj"

    if not cmaj_path:
        raise CmajorExternalError(
            "cmaj binary not found. Install it separately or set CMAJ_BIN/--cmaj.\n"
            f"See {repo_root() / GUIDE_RELATIVE_PATH}"
        )

    command = build_generate_command(
        cmaj_path,
        patch,
        target=args.target,
        output=args.output,
        extra_args=args.arg,
    )

    print(f"patch: {patch}")
    print(f"source: {source}")
    print(f"name: {manifest.get('name', '<unnamed>')}")
    print(f"target: {args.target}")
    print(f"output: {args.output}")
    print(f"command: {shlex.join(command)}")

    if args.dry_run:
        return 0

    completed = subprocess.run(command, check=False)
    return completed.returncode


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Pulp helper for the external Cmajor support lane. "
            "Pulp does not bundle the Cmajor runtime/toolchain."
        )
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    doctor = subparsers.add_parser(
        "doctor",
        help="Validate patch/example structure and report whether a cmaj binary is available.",
    )
    doctor.add_argument("--patch", required=True, help="Path to a .cmajorpatch file")
    doctor.add_argument("--cmaj", help="Explicit path to the cmaj executable")
    doctor.add_argument(
        "--require-tool",
        action="store_true",
        help="Return non-zero if cmaj is not available",
    )
    doctor.set_defaults(func=cmd_doctor)

    generate = subparsers.add_parser(
        "generate",
        help="Run `cmaj generate` with a validated patch and explicit output path.",
    )
    generate.add_argument("--patch", required=True, help="Path to a .cmajorpatch file")
    generate.add_argument("--output", required=True, help="Output path forwarded to cmaj")
    generate.add_argument(
        "--target",
        default=DEFAULT_TARGET,
        choices=SUPPORTED_TARGETS,
        help="Code generation target (default: cpp)",
    )
    generate.add_argument("--cmaj", help="Explicit path to the cmaj executable")
    generate.add_argument(
        "--arg",
        action="append",
        default=[],
        help="Extra argument forwarded directly to `cmaj generate`",
    )
    generate.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the exact command without invoking cmaj",
    )
    generate.set_defaults(func=cmd_generate)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        return args.func(args)
    except CmajorExternalError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
