#!/usr/bin/env python3
import argparse
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--expect-code", type=int, required=True)
    parser.add_argument("--expect-regex", required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    if not args.command:
        print("expect_command.py: missing command", file=sys.stderr)
        return 64
    if args.command[0] == "--":
        args.command = args.command[1:]

    result = subprocess.run(
        args.command,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    sys.stdout.write(result.stdout)
    if result.returncode != args.expect_code:
        print(
            f"expected exit code {args.expect_code}, got {result.returncode}",
            file=sys.stderr,
        )
        return 1
    if not re.search(args.expect_regex, result.stdout, re.DOTALL):
        print(
            f"expected output to match regex: {args.expect_regex}",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
