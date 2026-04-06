#!/usr/bin/env python3
"""Tests for tools/audit.py — verifies license and vendor checks work."""

import subprocess
import sys
import tempfile
import os
from pathlib import Path

AUDIT_SCRIPT = Path(__file__).resolve().parent.parent / 'tools' / 'audit.py'


def run_audit(*args, cwd=None):
    result = subprocess.run(
        [sys.executable, str(AUDIT_SCRIPT)] + list(args),
        capture_output=True, text=True, cwd=cwd
    )
    return result.returncode, result.stdout + result.stderr


def test_passes_on_valid_repo():
    """The main repo should pass the audit."""
    root = AUDIT_SCRIPT.parent.parent
    code, output = run_audit(str(root))
    assert code == 0, f"Audit failed on valid repo: {output}"
    assert 'PASSED' in output


def test_fails_missing_license():
    """A directory without LICENSE should fail."""
    with tempfile.TemporaryDirectory() as tmp:
        code, output = run_audit(tmp)
        assert code == 1, f"Expected failure for missing LICENSE: {output}"
        assert 'LICENSE' in output


def test_passes_license_only():
    """--license flag should only check license files."""
    with tempfile.TemporaryDirectory() as tmp:
        # Create minimal license files
        (Path(tmp) / 'LICENSE.md').write_text('MIT')
        (Path(tmp) / 'NOTICE.md').write_text('Third-party notices')
        (Path(tmp) / 'DEPENDENCIES.md').write_text('Dependencies')
        code, output = run_audit(tmp, '--license')
        assert code == 0, f"License-only check failed: {output}"


def test_private_audit_hook():
    """If .private/audit-naming.py exists, it runs automatically."""
    with tempfile.TemporaryDirectory() as tmp:
        # Create minimal valid structure
        (Path(tmp) / 'LICENSE.md').write_text('MIT')
        (Path(tmp) / 'NOTICE.md').write_text('Notices')
        (Path(tmp) / 'DEPENDENCIES.md').write_text('Deps')

        # Create a .private/audit-naming.py that always passes
        private_dir = Path(tmp) / '.private'
        private_dir.mkdir()
        (private_dir / 'audit-naming.py').write_text(
            '#!/usr/bin/env python3\nimport sys\nprint("Private audit ran")\nsys.exit(0)\n'
        )

        code, output = run_audit(tmp)
        assert code == 0, f"Failed with private audit: {output}"
        assert 'extended audit' in output.lower() or 'private' in output.lower()


def test_private_audit_failure_propagates():
    """If the private audit fails, the overall audit should fail."""
    with tempfile.TemporaryDirectory() as tmp:
        (Path(tmp) / 'LICENSE.md').write_text('MIT')
        (Path(tmp) / 'NOTICE.md').write_text('Notices')
        (Path(tmp) / 'DEPENDENCIES.md').write_text('Deps')

        private_dir = Path(tmp) / '.private'
        private_dir.mkdir()
        (private_dir / 'audit-naming.py').write_text(
            '#!/usr/bin/env python3\nimport sys\nprint("VIOLATION: bad name found")\nsys.exit(1)\n'
        )

        code, output = run_audit(tmp)
        assert code == 1, f"Expected failure when private audit fails: {output}"


if __name__ == '__main__':
    tests = [
        test_passes_on_valid_repo,
        test_fails_missing_license,
        test_passes_license_only,
        test_private_audit_hook,
        test_private_audit_failure_propagates,
    ]
    passed = failed = 0
    for test in tests:
        try:
            test()
            print(f'  PASS: {test.__name__}')
            passed += 1
        except AssertionError as e:
            print(f'  FAIL: {test.__name__}: {e}')
            failed += 1
        except Exception as e:
            print(f'  ERROR: {test.__name__}: {e}')
            failed += 1

    print(f'\n{passed} passed, {failed} failed')
    sys.exit(1 if failed else 0)
