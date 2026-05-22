"""Tests for the harness verifier orchestration layer."""

from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.harness import verifier  # noqa: E402
from tools.harness.adapters.base import CatalogEntry, Result  # noqa: E402
from tools.harness.status import Status  # noqa: E402


def _result(surface: str = "fake", name: str = "fake/alpha") -> Result:
    return Result(CatalogEntry(surface=surface, name=name, status="supported"), Status.PASS)


class FakeAdapter:
    seen_roots: list[Path] = []
    seen_entries: list[CatalogEntry] = []

    def __init__(self, repo_root: Path) -> None:
        self.seen_roots.append(repo_root)

    def run(self, entry: CatalogEntry) -> Result:
        self.seen_entries.append(entry)
        return _result(entry.surface, entry.name)


class VerifierDiscoveryTests(unittest.TestCase):
    def test_find_repo_root_finds_markers_or_falls_back_to_start(self) -> None:
        with tempfile.TemporaryDirectory() as tdir:
            root = Path(tdir)
            nested = root / "a" / "b"
            nested.mkdir(parents=True)
            (root / "compat.json").write_text("{}", encoding="utf-8")
            (root / "CMakeLists.txt").write_text("cmake", encoding="utf-8")
            self.assertEqual(verifier.find_repo_root(nested), root.resolve())
        with tempfile.TemporaryDirectory() as tdir:
            start = Path(tdir) / "missing"
            self.assertEqual(verifier.find_repo_root(start), start.resolve())

    def test_load_compat_and_collect_entries_skip_non_dict_payloads_and_sort(self) -> None:
        with tempfile.TemporaryDirectory() as tdir:
            root = Path(tdir)
            (root / "compat.json").write_text(
                json.dumps({"fake": {"zeta": {"status": "missing"}, "alpha": {"status": "supported"}, "bad": "skip"}}),
                encoding="utf-8",
            )
            compat = verifier.load_compat(root)
        entries = verifier.collect_entries(compat, "fake")
        self.assertEqual([entry.name for entry in entries], ["alpha", "zeta"])
        self.assertEqual([entry.status for entry in entries], ["supported", "missing"])
        self.assertEqual(verifier.collect_entries({}, "fake"), [])

    def test_discover_adapters_imports_non_private_modules_and_logs_failures(self) -> None:
        with mock.patch("pkgutil.iter_modules", return_value=[(None, "base", False), (None, "_private", False), (None, "ok", False), (None, "bad", False)]), \
             mock.patch("importlib.import_module", side_effect=[object(), RuntimeError("boom")]) as import_module, \
             self.assertLogs(verifier.logger, level="WARNING") as logs:
            registry = verifier._discover_adapters()
        self.assertIs(registry, verifier.adapters_base.ADAPTERS)
        self.assertEqual([call.args[0] for call in import_module.call_args_list], ["tools.harness.adapters.ok", "tools.harness.adapters.bad"])
        self.assertIn("failed to load", logs.output[0])


class VerifierRunTests(unittest.TestCase):
    def setUp(self) -> None:
        FakeAdapter.seen_roots = []
        FakeAdapter.seen_entries = []

    def test_run_surface_uses_adapter_and_evidence_filter(self) -> None:
        compat = {"fake": {"alpha": {"status": "supported"}}}
        with mock.patch.dict(verifier.ADAPTERS, {"fake": FakeAdapter}, clear=True), \
             mock.patch.object(verifier, "load_compat", return_value=compat), \
             mock.patch.object(verifier, "check_evidence", side_effect=lambda _root, results, _compat: results) as check_evidence:
            results = verifier.run_surface(Path("/repo"), "fake")
        self.assertEqual([result.entry.name for result in results], ["alpha"])
        self.assertEqual(FakeAdapter.seen_roots, [Path("/repo")])
        self.assertEqual([entry.name for entry in FakeAdapter.seen_entries], ["alpha"])
        check_evidence.assert_called_once()

    def test_run_surface_rejects_unwired_surface(self) -> None:
        with mock.patch.dict(verifier.ADAPTERS, {}, clear=True):
            with self.assertRaisesRegex(ValueError, "no adapter wired"):
                verifier.run_surface(Path("/repo"), "fake")

    def test_run_all_skips_known_surfaces_without_adapters(self) -> None:
        compat = {"fake": {"alpha": {"status": "supported"}}, "missing": {"beta": {"status": "supported"}}}
        with mock.patch.object(verifier, "KNOWN_SURFACES", ["fake", "missing"]), \
             mock.patch.dict(verifier.ADAPTERS, {"fake": FakeAdapter}, clear=True), \
             mock.patch.object(verifier, "load_compat", return_value=compat), \
             mock.patch.object(verifier, "check_evidence", side_effect=lambda _root, results, _compat: results):
            results = verifier.run_all(Path("/repo"))
        self.assertEqual(list(results), ["fake"])
        self.assertEqual([result.entry.name for result in results["fake"]], ["alpha"])

    def test_get_short_sha_returns_git_sha_or_unknown(self) -> None:
        with mock.patch("subprocess.check_output", return_value=b"abc123\n"):
            self.assertEqual(verifier.get_short_sha(Path("/repo")), "abc123")
        with mock.patch("subprocess.check_output", side_effect=OSError("git missing")):
            self.assertEqual(verifier.get_short_sha(Path("/repo")), "unknown")


class VerifierOutputTests(unittest.TestCase):
    def test_write_outputs_writes_build_and_docs_reports(self) -> None:
        with tempfile.TemporaryDirectory() as tdir:
            root = Path(tdir)
            written = verifier.write_outputs(root, {"fake": [_result()]}, "abc123", write_docs=True)
            self.assertEqual(set(written), {"json", "md", "docs"})
            self.assertTrue(written["json"].exists())
            self.assertTrue(written["md"].exists())
            self.assertTrue(written["docs"].exists())
            self.assertEqual(json.loads(written["json"].read_text(encoding="utf-8"))["sha"], "abc123")

    def test_parse_args_accepts_repeated_surfaces_and_coverage_prefix(self) -> None:
        args = verifier.parse_args(["--surface", "css", "--surface", "yoga", "--json", "--no-docs"])
        self.assertEqual(args.surface, ["css", "yoga"])
        self.assertTrue(args.json)
        self.assertTrue(args.no_docs)


class VerifierMainTests(unittest.TestCase):
    def test_main_reports_missing_repo_or_conflicting_selection(self) -> None:
        stderr = io.StringIO()
        with tempfile.TemporaryDirectory() as tdir, contextlib.redirect_stderr(stderr):
            rc = verifier.main(["--repo-root", tdir])
        self.assertEqual(rc, 2)
        self.assertIn("compat.json not found", stderr.getvalue())

        with tempfile.TemporaryDirectory() as tdir:
            root = Path(tdir)
            (root / "compat.json").write_text("{}", encoding="utf-8")
            stderr = io.StringIO()
            with contextlib.redirect_stderr(stderr):
                rc = verifier.main(["--repo-root", str(root), "--all", "--surface", "fake"])
        self.assertEqual(rc, 2)
        self.assertIn("pass --all OR --surface", stderr.getvalue())

    def test_main_rejects_unknown_surface_and_delegates_visual_subcommand(self) -> None:
        with tempfile.TemporaryDirectory() as tdir:
            root = Path(tdir)
            (root / "compat.json").write_text("{}", encoding="utf-8")
            stderr = io.StringIO()
            with mock.patch.dict(verifier.ADAPTERS, {}, clear=True), contextlib.redirect_stderr(stderr):
                rc = verifier.main(["--repo-root", str(root), "--surface", "fake"])
        self.assertEqual(rc, 2)
        self.assertIn("no adapter wired", stderr.getvalue())

        with mock.patch.object(verifier.visual_runner, "main", return_value=17) as visual_main:
            self.assertEqual(verifier.main(["visual", "--verify"]), 17)
        visual_main.assert_called_once_with(["visual", "--verify"])

    def test_main_json_prints_report_without_writing_outputs(self) -> None:
        stdout = io.StringIO()
        with tempfile.TemporaryDirectory() as tdir:
            root = Path(tdir)
            (root / "compat.json").write_text("{}", encoding="utf-8")
            with mock.patch.dict(verifier.ADAPTERS, {"fake": FakeAdapter}, clear=True), \
                 mock.patch.object(verifier, "run_surface", return_value=[_result()]), \
                 mock.patch.object(verifier, "get_short_sha", return_value="abc123"), \
                 mock.patch.object(verifier, "write_outputs") as write_outputs, \
                 contextlib.redirect_stdout(stdout):
                rc = verifier.main(["coverage", "--repo-root", str(root), "--surface", "fake", "--json"])
        payload = json.loads(stdout.getvalue())
        self.assertEqual(rc, 0)
        self.assertEqual(payload["sha"], "abc123")
        self.assertIn("fake", payload["surfaces"])
        write_outputs.assert_not_called()

    def test_main_writes_outputs_and_prints_summary(self) -> None:
        stdout = io.StringIO()
        written = {"json": Path("/repo/build/report.json"), "md": Path("/repo/build/report.md")}
        with tempfile.TemporaryDirectory() as tdir:
            root = Path(tdir)
            (root / "compat.json").write_text("{}", encoding="utf-8")
            with mock.patch.dict(verifier.ADAPTERS, {"fake": FakeAdapter}, clear=True), \
                 mock.patch.object(verifier, "run_surface", return_value=[_result()]), \
                 mock.patch.object(verifier, "get_short_sha", return_value="abc123"), \
                 mock.patch.object(verifier, "write_outputs", return_value=written), \
                 contextlib.redirect_stdout(stdout):
                rc = verifier.main(["--repo-root", str(root), "--surface", "fake", "--no-docs"])
        self.assertEqual(rc, 0)
        self.assertIn("# pulp harness coverage @ abc123", stdout.getvalue())
        self.assertIn("wrote json:", stdout.getvalue())


if __name__ == "__main__":
    unittest.main()
