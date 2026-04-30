#!/usr/bin/env python3
"""Anti-drift tests for ``version_bump_check.py`` trailer parsing.

Locks in the contract for the bypass-trailer parser hardening landed
for issue #1054. The audit found three silent-acceptance bugs and one
silent-no-op bug in the pre-#1054 parser:

1. ``mysdk=skip`` parsed as ``sdk=skip`` — substring left-attached typo
   silently bypassed the gate. (silent acceptance)
2. ``Version-Bump: sdk=skip`` (no ``reason=``) was honored. (silent
   acceptance — no audit-trail string)
3. ``Version-Bump: sdk=skip reason=""`` (empty reason) was honored.
   (silent acceptance — empty audit-trail string)
4. ``Version-Bump: cli=skip reason="x"`` (typo'd surface) parsed as
   absent with no diagnostic, leaving the author with a confusing
   "missing version bump" error and no way to know their bypass was
   rejected. (silent no-op)

Each test below deliberately fails if the hardening in
``surface_trailer_override`` / ``collect_trailer_diagnostics`` is
reverted, mirroring the pattern in ``test_local_diff_cover.py`` (see
commits ``efefe144`` + ``b258730c`` for the precedent).

Refs: #1054, #1049, #290 ("tests ship with fixes").
"""

from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

# Import the parser surface under test. Imported by name to keep the
# anti-drift tests pinned to the exact public API the script exposes.
import version_bump_check as vbc  # noqa: E402


KNOWN_SURFACES = ["sdk", "plugin"]


def _trailers_from_body(body: str) -> dict[str, list[str]]:
    """Parse a synthetic commit body the same way ``git_range_trailers``
    does — feed it through ``git interpret-trailers --parse`` and group
    the resulting key/value pairs by lowercased trailer key.

    A multi-line body (subject + blank + body) is required; ``git
    interpret-trailers`` returns nothing for a single-line input.
    """
    out = subprocess.run(
        ["git", "interpret-trailers", "--parse"],
        input=body, capture_output=True, text=True, check=True,
    )
    result: dict[str, list[str]] = {}
    for line in out.stdout.splitlines():
        if ":" not in line:
            continue
        key, _, value = line.partition(":")
        result.setdefault(key.strip().lower(), []).append(value.strip())
    return result


class SurfaceTrailerOverrideHardening(unittest.TestCase):
    """Behavior contracts for ``surface_trailer_override``.

    Every test pairs the structurally-malformed input that USED to
    silently bypass the gate (pre-#1054) with the strict-rejection
    behavior the audit installed.
    """

    # ── Substring left-attached typo (silent acceptance bug) ─────────

    def test_mysdk_skip_does_not_match_sdk_surface(self):
        """``mysdk=skip`` must not be honored as a bypass for ``sdk``.

        Pre-#1054 the regex was ``rf"{surface}\\s*=\\s*..."`` — unanchored
        on the left, so ``mysdk=skip`` substring-matched ``sdk=skip``
        and silently bypassed the gate. If this test fails, the regex
        in ``_surface_name_regex`` has lost its left-side word boundary
        (``(?:^|[\\s,;])`` prefix).
        """
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: mysdk=skip reason="x"'
        )
        self.assertIsNone(
            vbc.surface_trailer_override(trailers, "Version-Bump", "sdk"),
            "mysdk=skip silently parsed as sdk=skip — substring bug "
            "regressed (#1054). The surface match must be anchored on a "
            "word boundary.",
        )

    def test_sdkx_skip_does_not_match_sdk_surface(self):
        """Right-side substring is also rejected.

        The pre-#1054 regex happened to fail on right-attached typos
        like ``sdkx`` because ``([A-Za-z]+)`` was greedy and consumed
        ``x`` into the level group. The hardening adds a trailing
        ``\\b`` anchor to make this rejection explicit instead of
        accidental.
        """
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: sdkx=skip reason="x"'
        )
        self.assertIsNone(
            vbc.surface_trailer_override(trailers, "Version-Bump", "sdk"),
            "sdkx=skip silently parsed as sdk=<level> — right-boundary "
            "anchor regressed (#1054).",
        )

    # ── Missing reason (silent acceptance bug) ───────────────────────

    def test_skip_without_reason_is_rejected(self):
        """``Version-Bump: sdk=skip`` with no ``reason="..."`` falls
        through. Pre-#1054 the gate accepted it silently, defeating the
        audit-trail-of-record contract.
        """
        trailers = _trailers_from_body(
            "subj\n\nVersion-Bump: sdk=skip"
        )
        self.assertIsNone(
            vbc.surface_trailer_override(trailers, "Version-Bump", "sdk"),
            "Bypass without reason=\"...\" silently honored — "
            "audit-trail contract regressed (#1054).",
        )

    def test_skip_with_empty_reason_is_rejected(self):
        """``reason=""`` (empty quotes) is rejected — empty string is
        not a justification.
        """
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: sdk=skip reason=""'
        )
        self.assertIsNone(
            vbc.surface_trailer_override(trailers, "Version-Bump", "sdk"),
            'reason="" silently honored — empty-justification check '
            "regressed (#1054).",
        )

    def test_skip_with_whitespace_only_reason_is_rejected(self):
        """``reason="   "`` (whitespace-only) is rejected — must contain
        a non-whitespace character.
        """
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: sdk=skip reason="   "'
        )
        self.assertIsNone(
            vbc.surface_trailer_override(trailers, "Version-Bump", "sdk"),
            'whitespace-only reason silently honored — '
            "_has_nonempty_reason regressed (#1054).",
        )

    # ── Canonical positive cases (must remain green) ─────────────────

    def test_canonical_skip_with_reason_is_honored(self):
        """``sdk=skip reason="..."`` continues to work — the
        anti-drift hardening must not break the documented grammar.
        """
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: sdk=skip reason="legitimate"'
        )
        self.assertEqual(
            vbc.surface_trailer_override(trailers, "Version-Bump", "sdk"),
            "skip",
        )

    def test_canonical_patch_with_reason_is_honored(self):
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: plugin=patch reason="docs-only"'
        )
        self.assertEqual(
            vbc.surface_trailer_override(trailers, "Version-Bump", "plugin"),
            "patch",
        )

    def test_canonical_levels_all_accepted(self):
        """All four documented levels are accepted (when paired with a
        non-empty reason)."""
        for level in ("patch", "minor", "major", "skip"):
            with self.subTest(level=level):
                trailers = _trailers_from_body(
                    f'subj\n\nVersion-Bump: sdk={level} reason="ok"'
                )
                self.assertEqual(
                    vbc.surface_trailer_override(
                        trailers, "Version-Bump", "sdk"
                    ),
                    level,
                )

    def test_none_sentinel_is_rejected(self):
        """``Version-Bump: sdk=none`` must be rejected even with a
        valid reason — the ``none`` sentinel is a heuristic-pipeline
        internal value, not a trailer level. Pre-existing behavior
        from #629; locked in here so it can't drift.
        """
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: sdk=none reason="bypass"'
        )
        self.assertIsNone(
            vbc.surface_trailer_override(trailers, "Version-Bump", "sdk"),
        )

    def test_bogus_level_is_rejected(self):
        """Any level outside the known set falls through."""
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: sdk=foobar reason="x"'
        )
        self.assertIsNone(
            vbc.surface_trailer_override(trailers, "Version-Bump", "sdk"),
        )

    def test_multiple_trailers_each_resolve_correctly(self):
        """When two valid trailers appear (no blank line between them),
        each surface picks up its own level."""
        trailers = _trailers_from_body(
            'subj\n\n'
            'Version-Bump: sdk=skip reason="a"\n'
            'Version-Bump: plugin=patch reason="b"'
        )
        self.assertEqual(
            vbc.surface_trailer_override(trailers, "Version-Bump", "sdk"),
            "skip",
        )
        self.assertEqual(
            vbc.surface_trailer_override(trailers, "Version-Bump", "plugin"),
            "patch",
        )


class CollectTrailerDiagnosticsHardening(unittest.TestCase):
    """Behavior contracts for ``collect_trailer_diagnostics``.

    The diagnostic surface is the user-visible half of the #1054 fix:
    when a malformed trailer is rejected by ``surface_trailer_override``,
    the author needs a clear "did you mean?" or "add a reason" message
    instead of the generic "missing version bump" failure.
    """

    def test_unknown_surface_emits_did_you_mean_hint(self):
        """``Version-Bump: cli=skip reason="x"`` (typo of ``sdk``) must
        emit a diagnostic naming the closest known surface.

        Pre-#1054 this case parsed as absent with no warning — the
        author saw "missing version bump" and had to guess why. If
        this test fails, ``collect_trailer_diagnostics`` has lost its
        unknown-surface branch.
        """
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: cli=skip reason="x"'
        )
        diag = vbc.collect_trailer_diagnostics(
            trailers, "Version-Bump", KNOWN_SURFACES,
        )
        self.assertEqual(len(diag), 1, f"expected 1 diagnostic, got {diag!r}")
        self.assertIn("unknown surface", diag[0])
        self.assertIn("`cli`", diag[0])
        self.assertIn("Known surfaces", diag[0])

    def test_close_typo_suggests_correction(self):
        """A small-edit-distance typo emits a ``Did you mean?`` hint."""
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: pluggin=skip reason="x"'
        )
        diag = vbc.collect_trailer_diagnostics(
            trailers, "Version-Bump", KNOWN_SURFACES,
        )
        self.assertTrue(
            any("Did you mean `plugin`" in d for d in diag),
            f"close-typo hint missing — diagnostics: {diag!r}",
        )

    def test_substring_typo_emits_diagnostic(self):
        """``mysdk=skip`` is rejected by the parser AND surfaces a
        diagnostic — the user sees the bypass was rejected.
        """
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: mysdk=skip reason="x"'
        )
        diag = vbc.collect_trailer_diagnostics(
            trailers, "Version-Bump", KNOWN_SURFACES,
        )
        self.assertTrue(
            any("`mysdk`" in d for d in diag),
            f"mysdk diagnostic missing — diagnostics: {diag!r}",
        )

    def test_missing_reason_emits_diagnostic(self):
        """A bypass without ``reason="..."`` produces a diagnostic
        explaining the audit-trail-of-record contract.
        """
        trailers = _trailers_from_body(
            "subj\n\nVersion-Bump: sdk=skip"
        )
        diag = vbc.collect_trailer_diagnostics(
            trailers, "Version-Bump", KNOWN_SURFACES,
        )
        self.assertTrue(
            any("missing a non-empty `reason" in d for d in diag),
            f"missing-reason diagnostic missing — diagnostics: {diag!r}",
        )

    def test_empty_reason_emits_diagnostic(self):
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: sdk=skip reason=""'
        )
        diag = vbc.collect_trailer_diagnostics(
            trailers, "Version-Bump", KNOWN_SURFACES,
        )
        self.assertTrue(
            any("missing a non-empty `reason" in d for d in diag),
            f"empty-reason diagnostic missing — diagnostics: {diag!r}",
        )

    def test_none_sentinel_emits_diagnostic(self):
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: sdk=none reason="x"'
        )
        diag = vbc.collect_trailer_diagnostics(
            trailers, "Version-Bump", KNOWN_SURFACES,
        )
        self.assertTrue(
            any("not a valid bypass level" in d for d in diag),
            f"none-sentinel diagnostic missing — diagnostics: {diag!r}",
        )

    def test_bogus_level_emits_diagnostic(self):
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: sdk=foobar reason="x"'
        )
        diag = vbc.collect_trailer_diagnostics(
            trailers, "Version-Bump", KNOWN_SURFACES,
        )
        self.assertTrue(
            any("unrecognised level" in d for d in diag),
            f"bogus-level diagnostic missing — diagnostics: {diag!r}",
        )

    def test_canonical_trailer_emits_no_diagnostic(self):
        """Well-formed trailers must NOT produce diagnostics — the
        warning surface is reserved for actual bugs.
        """
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: sdk=skip reason="legit"'
        )
        diag = vbc.collect_trailer_diagnostics(
            trailers, "Version-Bump", KNOWN_SURFACES,
        )
        self.assertEqual(
            diag, [],
            f"canonical trailer falsely flagged: {diag!r}",
        )

    def test_canonical_patch_emits_no_diagnostic(self):
        trailers = _trailers_from_body(
            'subj\n\nVersion-Bump: plugin=patch reason="x"'
        )
        diag = vbc.collect_trailer_diagnostics(
            trailers, "Version-Bump", KNOWN_SURFACES,
        )
        self.assertEqual(diag, [])

    def test_no_trailer_emits_no_diagnostic(self):
        """A commit with no Version-Bump trailer at all produces no
        diagnostics (it's the absent-trailer case the per-surface
        verdict pipeline handles)."""
        trailers = _trailers_from_body(
            "subj\n\nJust a normal commit body."
        )
        diag = vbc.collect_trailer_diagnostics(
            trailers, "Version-Bump", KNOWN_SURFACES,
        )
        self.assertEqual(diag, [])


class TrailerParsingEdgeCases(unittest.TestCase):
    """End-to-end pipeline contract — locks the audit recipe from
    issue #1054 directly into the test suite.

    Each entry is one of the cases the audit recipe enumerates,
    paired with the post-#1054 expected behavior. If this test fails,
    the parser has drifted away from the contract recorded in #1054.
    """

    # (label, commit body, expected per-surface verdicts, must produce >=1 diagnostic)
    CASES: list[tuple[str, str, dict[str, str | None], bool]] = [
        (
            "canonical sdk=skip",
            'subj\n\nVersion-Bump: sdk=skip reason="x"',
            {"sdk": "skip", "plugin": None},
            False,
        ),
        (
            "no space after colon",
            'subj\n\nVersion-Bump:sdk=skip reason="x"',
            # `git interpret-trailers --parse` accepts `Trailer:value`
            # without the canonical space — exercise the documented
            # forgiving behavior.
            {"sdk": "skip", "plugin": None},
            False,
        ),
        (
            "missing reason",
            "subj\n\nVersion-Bump: sdk=skip",
            {"sdk": None, "plugin": None},
            True,
        ),
        (
            "bad surface (cli)",
            'subj\n\nVersion-Bump: cli=skip reason="x"',
            {"sdk": None, "plugin": None},
            True,
        ),
        (
            "absent",
            "subj\n\nNothing to see here.",
            {"sdk": None, "plugin": None},
            False,
        ),
    ]

    def test_audit_recipe_cases_match_contract(self):
        for label, body, expected, expect_diag in self.CASES:
            with self.subTest(case=label):
                trailers = _trailers_from_body(body)
                for surface, want in expected.items():
                    got = vbc.surface_trailer_override(
                        trailers, "Version-Bump", surface,
                    )
                    self.assertEqual(
                        got, want,
                        f"[{label}] surface={surface}: "
                        f"want={want!r}, got={got!r}",
                    )
                diag = vbc.collect_trailer_diagnostics(
                    trailers, "Version-Bump", KNOWN_SURFACES,
                )
                if expect_diag:
                    self.assertTrue(
                        diag,
                        f"[{label}] expected at least one diagnostic, "
                        f"got none.",
                    )
                else:
                    self.assertFalse(
                        diag,
                        f"[{label}] expected no diagnostic, got {diag!r}.",
                    )


if __name__ == "__main__":
    unittest.main()
