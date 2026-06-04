#!/usr/bin/env python3
import argparse
import json
import os
import shutil
import shlex
import subprocess
import sys
from pathlib import Path


SKIP_MISSING_VALIDATOR = 77


def find_cli_validator():
    configured = os.environ.get("PULP_GLTF_VALIDATOR", "").strip()
    if configured:
        return shlex.split(configured)

    for candidate in ("gltf_validator", "gltf_validator.exe", "gltf-validator"):
        resolved = shutil.which(candidate)
        if resolved:
            return [resolved]

    return None


def node_can_resolve_validator(node):
    probe = subprocess.run(
        [node, "-e", "require.resolve('gltf-validator')"],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL)
    return probe.returncode == 0


def find_node_validator():
    node = shutil.which("node")
    if not node:
        return None
    return node if node_can_resolve_validator(node) else None


def run_cli_validator(command, asset):
    return subprocess.run(
        [*command, "-o", str(asset)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)


def run_node_validator(node, asset):
    script = r"""
const fs = require('fs');
const path = require('path');
const validator = require('gltf-validator');
const assetPath = process.argv[1];
const bytes = fs.readFileSync(assetPath);
const extension = path.extname(assetPath).toLowerCase();
const format = extension === '.glb' ? 'glb' : 'gltf';

validator.validateBytes(new Uint8Array(bytes), {
  uri: path.basename(assetPath),
  format,
  writeTimestamp: false,
  maxIssues: 0,
  externalResourceFunction: (uri) => new Promise((resolve, reject) => {
    const resolved = path.resolve(path.dirname(assetPath), decodeURIComponent(uri));
    fs.readFile(resolved, (error, data) => {
      if (error) {
        reject(error.toString());
        return;
      }
      resolve(new Uint8Array(data));
    });
  }),
}).then((report) => {
  console.log(JSON.stringify(report));
}, (error) => {
  console.error(error);
  process.exit(2);
});
"""
    return subprocess.run(
        [node, "-e", script, str(asset)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)


def main():
    parser = argparse.ArgumentParser(
        description="Validate a glTF/GLB asset with Khronos glTF-Validator.")
    parser.add_argument("asset", type=Path)
    parser.add_argument(
        "--require-validator",
        action="store_true",
        help="fail instead of returning the CTest skip code when the validator is missing")
    args = parser.parse_args()

    asset = args.asset
    if not asset.exists():
        print(f"glTF asset not found: {asset}", file=sys.stderr)
        return 2

    cli_validator = find_cli_validator()
    node_validator = None if cli_validator is not None else find_node_validator()
    if cli_validator is None and node_validator is None:
        message = (
            "Khronos glTF-Validator not found. Install `gltf_validator`, set "
            "PULP_GLTF_VALIDATOR=/path/to/gltf_validator, or make the official "
            "npm package `gltf-validator` resolvable to Node."
        )
        print(message, file=sys.stderr)
        return 2 if args.require_validator else SKIP_MISSING_VALIDATOR

    result = (run_cli_validator(cli_validator, asset)
              if cli_validator is not None
              else run_node_validator(node_validator, asset))

    if result.stderr.strip():
        print(result.stderr.strip(), file=sys.stderr)

    try:
        report = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        print(f"validator did not write a JSON report to stdout: {exc}", file=sys.stderr)
        if result.stdout.strip():
            print(result.stdout.strip(), file=sys.stderr)
        return 2

    issues = report.get("issues", {})
    num_errors = int(issues.get("numErrors", 0))
    num_warnings = int(issues.get("numWarnings", 0))
    num_infos = int(issues.get("numInfos", 0))
    print(
        f"{asset}: glTF-Validator errors={num_errors} "
        f"warnings={num_warnings} infos={num_infos}")

    if result.returncode != 0 or num_errors != 0:
        return result.returncode if result.returncode != 0 else 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
