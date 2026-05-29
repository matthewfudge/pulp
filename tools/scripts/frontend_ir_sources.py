#!/usr/bin/env python3
"""Source artifact helpers for PulpFrontendIR report assembly."""

from __future__ import annotations

import pathlib
import re
from typing import Any


SOURCE_INPUT_KEYS = ("sourceJsx", "sourceHtml", "sourceFile")
WATCH_INPUT_KEYS = frozenset((*SOURCE_INPUT_KEYS, "bundle", "sourceAudit"))


def metric_key(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]+", "_", value).strip("_").lower() or "unknown"


def resource_id_key(value: str) -> str:
    return metric_key(re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", value))


def artifact_from_input(value: Any) -> dict[str, str]:
    artifact: dict[str, str] = {}
    if isinstance(value, dict):
        path = value.get("path")
        if isinstance(path, str) and path:
            artifact["path"] = path
        sha = value.get("sha256")
        if isinstance(sha, str) and sha:
            artifact["sha256"] = sha
        return artifact
    if isinstance(value, str) and value and ("/" in value or "." in pathlib.Path(value).name):
        artifact["path"] = value
    return artifact


def source_input(route_manifest: dict[str, Any]) -> dict[str, str]:
    inputs = route_manifest.get("inputs", {})
    if not isinstance(inputs, dict):
        return {}
    for key in SOURCE_INPUT_KEYS:
        artifact = artifact_from_input(inputs.get(key))
        if artifact.get("path"):
            return artifact
    return {}
