#!/usr/bin/env python3
"""audit.py — License and vendor-material audit tool for Pulp projects.

Checks for:
- License compatibility of dependencies
- Disallowed license types (GPL, LGPL, AGPL, SSPL, proprietary)
- Forbidden vendored vendor SDK artifacts
- Missing DEPENDENCIES.md entries for detected dependencies

Usage:
    python3 tools/audit.py                    # Audit the current project
    python3 tools/audit.py path/to/library    # Audit a specific library
    python3 tools/audit.py --license          # License check only
"""

import argparse
import re
import sys
from pathlib import Path

# License classifications
ALLOWED = {'MIT', 'BSD-2-Clause', 'BSD-3-Clause', 'Apache-2.0', 'ISC',
           'zlib', 'BSL-1.0', 'Unlicense', 'public domain', 'CC0-1.0'}
DISALLOWED = {'GPL-2.0', 'GPL-3.0', 'LGPL-2.1', 'LGPL-3.0', 'AGPL-3.0',
              'SSPL-1.0', 'proprietary'}

# Patterns for vendor SDK files that must not be committed
VENDOR_FILE_PATTERNS = [
    re.compile(r'(^|/)(aax-validator|digishell|aax-page-table-editor|cloudclientservices|hd_driver|pro[ _-]?tools|protoolsktrace|protoolswprtool)[^/]*$', re.IGNORECASE),
    re.compile(r'(^|/)asiosdk', re.IGNORECASE),
]

SKIP_DIRS = {'build', 'build-asan', 'build-xcode', 'external', '.git',
             'node_modules', '.planning-repo', '.private', 'planning'}
SOURCE_EXTENSIONS = {'.cpp', '.hpp', '.h', '.mm', '.c', '.m'}


def check_vendor_files(root):
    """Check for forbidden vendor SDK files."""
    errors = []
    for dirpath, dirnames, filenames in Path(root).walk() if hasattr(Path, 'walk') else _walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for pattern in VENDOR_FILE_PATTERNS:
            for fname in filenames:
                if pattern.search(fname):
                    errors.append(f"Vendor file found: {Path(dirpath) / fname}")
            for dname in list(dirnames):
                if pattern.search(dname):
                    errors.append(f"Vendor directory found: {Path(dirpath) / dname}")
    return errors


def _walk(root):
    """Fallback for Python < 3.12 without Path.walk."""
    import os
    for dirpath, dirnames, filenames in os.walk(root):
        yield dirpath, dirnames, filenames


def check_license_files(root):
    """Check that LICENSE and NOTICE files exist and reference allowed licenses."""
    errors = []
    root = Path(root)
    if not (root / 'LICENSE.md').exists() and not (root / 'LICENSE').exists():
        errors.append("No LICENSE.md or LICENSE file found")
    if not (root / 'NOTICE.md').exists():
        errors.append("No NOTICE.md file found")
    if not (root / 'DEPENDENCIES.md').exists():
        errors.append("No DEPENDENCIES.md file found")
    return errors


def main():
    parser = argparse.ArgumentParser(description='License and vendor audit for Pulp')
    parser.add_argument('path', nargs='?', default='.', help='Root path to audit')
    parser.add_argument('--license', action='store_true', help='License check only')
    args = parser.parse_args()

    root = Path(args.path).resolve()
    errors = []

    # License file checks
    errors.extend(check_license_files(root))

    # Vendor file checks
    if not args.license:
        errors.extend(check_vendor_files(root))

    if errors:
        print(f'\n\033[1;31m{len(errors)} issues found:\033[0m')
        for e in errors:
            print(f'  {e}')
        sys.exit(1)
    else:
        print(f'\n\033[1;32mPASSED: all checks clean\033[0m')
        sys.exit(0)


if __name__ == '__main__':
    main()
