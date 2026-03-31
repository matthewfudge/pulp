#!/usr/bin/env python3
"""Generate a C++ header that embeds JS preludes as concatenated raw strings."""

from __future__ import annotations

import pathlib
import sys

CHUNK_SIZE = 12000


def var_name_for(path: pathlib.Path) -> str:
    return path.stem.replace("-", "_")


def chunk_text(text: str, chunk_size: int) -> list[str]:
    return [text[i : i + chunk_size] for i in range(0, len(text), chunk_size)] or [""]


def main(argv: list[str]) -> int:
    if len(argv) < 3:
        print("usage: embed_js.py <output.hpp> <input1.js> [input2.js ...]", file=sys.stderr)
        return 1

    output_path = pathlib.Path(argv[1])
    input_paths = [pathlib.Path(arg) for arg in argv[2:]]

    lines = [
        "#pragma once",
        "// Auto-generated from JS prelude files — do not edit",
        "namespace pulp::view::preludes {",
        "",
    ]

    for input_path in input_paths:
        text = input_path.read_text(encoding="utf-8")
        lines.append(f"static const char* {var_name_for(input_path)} =")
        for chunk in chunk_text(text, CHUNK_SIZE):
            lines.append('R"__JS__(')
            lines.append(chunk)
            lines.append(')__JS__"')
        lines.append(";")
        lines.append("")

    lines.append("} // namespace pulp::view::preludes")
    lines.append("")
    output_path.write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
