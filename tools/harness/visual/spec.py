"""Visual harness fixture metadata and validation-reference helpers."""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path
from typing import Any, Iterable, Optional


RUNTIME_FIXTURE_KINDS = {"semantic", "visual", "dom", "behavior"}
NON_RUNTIME_KINDS = {"unit", "cannot-validate"}
VALIDATION_KINDS = RUNTIME_FIXTURE_KINDS | NON_RUNTIME_KINDS


@dataclass(frozen=True)
class ValidationRef:
    kind: str
    target: str
    raw: str

    @property
    def is_runtime_fixture(self) -> bool:
        return self.kind in RUNTIME_FIXTURE_KINDS

    @property
    def excludes_from_visual_denominator(self) -> bool:
        return self.kind == "cannot-validate"


@dataclass(frozen=True)
class VisualFixtureSpec:
    id: str
    surface: str
    entry: str
    driver: str
    kind: str
    capture_format: str
    source_path: Path
    viewport: dict[str, Any] | None
    tolerance: dict[str, Any]

    @property
    def golden_suffix(self) -> str:
        return ".png" if self.capture_format == "png" else ".json"


def parse_validation_ref(value: str) -> Optional[ValidationRef]:
    if not isinstance(value, str) or ":" not in value:
        return None
    kind, target = value.split(":", 1)
    if kind not in VALIDATION_KINDS or not target:
        return None
    return ValidationRef(kind=kind, target=target, raw=value)


def validation_refs(values: Iterable[str]) -> list[ValidationRef]:
    refs: list[ValidationRef] = []
    for value in values:
        ref = parse_validation_ref(value)
        if ref is not None:
            refs.append(ref)
    return refs


def runtime_fixture_refs(values: Iterable[str]) -> list[ValidationRef]:
    return [ref for ref in validation_refs(values) if ref.is_runtime_fixture]


def fixture_spec_from_file(path: Path, *, default_surface: str | None = None) -> VisualFixtureSpec:
    raw = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict):
        raise ValueError(f"visual fixture {path} must contain a JSON object")

    surface = str(raw.get("surface") or default_surface or path.parent.name)
    entry = path.stem
    fixture_id = str(raw.get("id") or f"{surface}/{entry}")
    if "/" not in fixture_id:
        fixture_id = f"{surface}/{fixture_id}"

    kind = str(raw.get("kind") or "layout")
    driver = str(raw.get("driver") or _default_driver_for_kind(kind))
    capture = raw.get("capture")
    capture_format = raw.get("capture_format") or raw.get("captureFormat")
    if isinstance(capture, dict):
        capture_format = capture_format or capture.get("format")
    capture_format = str(capture_format or _default_capture_format_for_kind(kind)).lower()

    tolerance = raw.get("tolerance")
    viewport = raw.get("viewport")
    return VisualFixtureSpec(
        id=fixture_id,
        surface=surface,
        entry=entry,
        driver=driver,
        kind=kind,
        capture_format=capture_format,
        source_path=path,
        viewport=viewport if isinstance(viewport, dict) else None,
        tolerance=tolerance if isinstance(tolerance, dict) else {},
    )


def fixture_specs(repo_root: Path, surfaces: Iterable[str] | None = None) -> list[VisualFixtureSpec]:
    root = repo_root / "tools" / "harness" / "visual" / "fixtures"
    surface_names = list(surfaces or [])
    if not surface_names:
        surface_names = sorted(p.name for p in root.iterdir() if p.is_dir()) if root.exists() else []

    specs: list[VisualFixtureSpec] = []
    for surface in surface_names:
        fixture_dir = root / surface
        if not fixture_dir.exists():
            continue
        for fixture in sorted(fixture_dir.glob("*.json")):
            specs.append(fixture_spec_from_file(fixture, default_surface=surface))
    return specs


def fixture_index(
    repo_root: Path,
    surfaces: Iterable[str] | None = None,
) -> dict[str, VisualFixtureSpec]:
    index: dict[str, VisualFixtureSpec] = {}
    duplicates: list[str] = []
    for fixture in fixture_specs(repo_root, surfaces):
        previous = index.get(fixture.id)
        if previous is not None:
            duplicates.append(
                f"{fixture.id}: {previous.source_path.relative_to(repo_root)} and "
                f"{fixture.source_path.relative_to(repo_root)}"
            )
            continue
        index[fixture.id] = fixture
    if duplicates:
        raise ValueError("duplicate visual fixture id(s): " + "; ".join(duplicates))
    return index


def golden_path(repo_root: Path, fixture: VisualFixtureSpec) -> Path:
    return (
        repo_root
        / "tools"
        / "harness"
        / "visual"
        / "goldens"
        / fixture.surface
        / f"{fixture.entry}{fixture.golden_suffix}"
    )


def fixture_ids_with_goldens(
    repo_root: Path,
    surfaces: Iterable[str] | None = None,
) -> set[str]:
    out: set[str] = set()
    for fixture in fixture_index(repo_root, surfaces).values():
        if golden_path(repo_root, fixture).exists():
            out.add(fixture.id)
    return out


def orphaned_golden_paths(
    repo_root: Path,
    surfaces: Iterable[str] | None = None,
) -> list[Path]:
    fixture_golden_paths = {
        golden_path(repo_root, fixture).resolve()
        for fixture in fixture_index(repo_root, surfaces).values()
    }
    root = repo_root / "tools" / "harness" / "visual" / "goldens"
    if not root.exists():
        return []

    surface_names = list(surfaces or [])
    if not surface_names:
        surface_names = sorted(p.name for p in root.iterdir() if p.is_dir())

    out: list[Path] = []
    for surface in surface_names:
        surface_dir = root / surface
        if not surface_dir.exists():
            continue
        for path in sorted(surface_dir.iterdir()):
            if path.is_file() and path.suffix in {".json", ".png"}:
                if path.resolve() not in fixture_golden_paths:
                    out.append(path)
    return out


def _default_driver_for_kind(kind: str) -> str:
    return "native_tree" if kind == "layout" else kind


def _default_capture_format_for_kind(kind: str) -> str:
    return "png" if kind == "render" else "json"
