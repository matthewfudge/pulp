#!/usr/bin/env bash
# test_install_consumer_cmake.sh — pin the SDK install layout invariants.
#
# Why this test exists
# --------------------
# pulp #2089: `lib/cmake/Pulp/PulpUtils.cmake` did
#   include(${CMAKE_SOURCE_DIR}/tools/cmake/PulpLinkFontconfig.cmake)
# which resolves to the DOWNSTREAM project's source tree, not Pulp's.
# Spectr (and any consumer of `find_package(Pulp REQUIRED)` that calls
# pulp_add_plugin(... STANDALONE ...)) hit a configure-time error:
#   include could not find requested file:
#     /Users/.../spectr/tools/cmake/PulpLinkFontconfig.cmake
#
# The bug only surfaces when Pulp is consumed via an installed SDK —
# in-tree builds of Pulp itself never hit it because CMAKE_SOURCE_DIR
# IS Pulp's source there.
#
# What this test pins (cheap static checks, no real install needed)
# ----------------------------------------------------------------
# 1. Root CMakeLists.txt's `install(FILES … DESTINATION lib/cmake/Pulp)`
#    rule includes `tools/cmake/PulpLinkFontconfig.cmake` so the helper
#    actually lands in the SDK tarball.
# 2. PulpUtils.cmake's include of PulpLinkFontconfig.cmake uses
#    a path that resolves both in-tree AND inside an installed SDK.
#    Specifically: NOT `${CMAKE_SOURCE_DIR}/tools/cmake/...` (the bug),
#    NOT bare `${CMAKE_CURRENT_LIST_DIR}/...` inside a function (resolves
#    to caller's file at function-call time, also broken downstream).
#
# Why static checks instead of a real install + consumer build
# ------------------------------------------------------------
# A real install requires building all SDK targets (pulp-cpp, libwgpu,
# etc.) which is multi-minute. The SHAPE of the bug is purely about
# cmake-script paths that are decidable from source. Release CI does the
# end-to-end install consumer test; this test catches the regression at
# pre-merge time without a heavy build.
#
# Tag [issue-2089] for coverage attribution.

set -euo pipefail

PULP_SRC="$(cd "$(dirname "$0")/.." && pwd)"

PASS=0
FAIL=0
assert() {
  local label="$1" cond="$2"
  if eval "$cond"; then
    echo "  PASS — $label"
    PASS=$((PASS+1))
  else
    echo "  FAIL — $label"
    FAIL=$((FAIL+1))
  fi
}

# ── Check 1: install(FILES …) rule includes PulpLinkFontconfig.cmake ──
ROOT_CMAKE="$PULP_SRC/CMakeLists.txt"
[[ -f "$ROOT_CMAKE" ]] || { echo "FAIL: missing $ROOT_CMAKE"; exit 2; }

# Find the install(FILES … DESTINATION lib/cmake/Pulp) block and confirm
# PulpLinkFontconfig.cmake is in it. Use python for multi-line block parse.
python3 - "$ROOT_CMAKE" <<'PY'
import re, sys
src = open(sys.argv[1]).read()
# Find any install(FILES …) block whose body mentions DESTINATION
# lib/cmake/Pulp. Tolerant of formatting (whitespace, line wraps).
blocks = re.findall(r'install\s*\(\s*FILES(.*?)\)', src, re.DOTALL)
matching = [b for b in blocks if 'lib/cmake/Pulp' in b or 'cmake/Pulp' in b]
if not matching:
    print("  FAIL — no install(FILES …) block targets lib/cmake/Pulp/")
    sys.exit(1)
joined = '\n'.join(matching)
if 'PulpLinkFontconfig.cmake' not in joined:
    print("  FAIL — install(FILES …) block does not list PulpLinkFontconfig.cmake")
    print("         (this is the regression that broke v0.101.5 SDK consumers)")
    sys.exit(1)
print("  PASS — install(FILES …) block ships PulpLinkFontconfig.cmake")
PY
[[ $? -eq 0 ]] && PASS=$((PASS+1)) || FAIL=$((FAIL+1))

# ── Check 2: PulpUtils.cmake uses a stable, file-scope-captured include
#           path for PulpLinkFontconfig.cmake ──────────────────────────
UTILS="$PULP_SRC/tools/cmake/PulpUtils.cmake"
[[ -f "$UTILS" ]] || { echo "FAIL: missing $UTILS"; exit 2; }

# Reject the broken patterns explicitly. Both of these "look right" at a
# glance but break under find_package consumers.
if grep -nE 'include\([^)]*\$\{CMAKE_SOURCE_DIR\}.*PulpLinkFontconfig' "$UTILS" >/dev/null; then
  echo "  FAIL — PulpUtils.cmake still uses \${CMAKE_SOURCE_DIR}/... for PulpLinkFontconfig"
  echo "         (this points at downstream consumer's tree, not Pulp's — see #2089)"
  FAIL=$((FAIL+1))
else
  echo "  PASS — PulpUtils.cmake does NOT use \${CMAKE_SOURCE_DIR}/... for PulpLinkFontconfig"
  PASS=$((PASS+1))
fi

# Inside a function body, ${CMAKE_CURRENT_LIST_DIR} resolves to the
# CALLER's file at execution time. So a bare ${CMAKE_CURRENT_LIST_DIR}/X
# inside `function(_pulp_add_standalone …)` is ALSO broken downstream.
# Detect by checking whether the include line uses our file-scope-captured
# variable instead.
if ! grep -nE 'include\([^)]*\$\{_pulp_utils_cmake_dir\}.*PulpLinkFontconfig' "$UTILS" >/dev/null; then
  echo "  FAIL — PulpUtils.cmake does NOT use the file-scope \${_pulp_utils_cmake_dir}/..."
  echo "         pattern for PulpLinkFontconfig (only that pattern survives both"
  echo "         in-tree and downstream-find_package contexts — see #2089)"
  FAIL=$((FAIL+1))
else
  echo "  PASS — PulpUtils.cmake uses file-scope \${_pulp_utils_cmake_dir}/..."
  PASS=$((PASS+1))
fi

# Check 3: the file-scope variable is actually defined.
if ! grep -nE '^set\(_pulp_utils_cmake_dir' "$UTILS" >/dev/null; then
  echo "  FAIL — _pulp_utils_cmake_dir is referenced but never set at file scope"
  FAIL=$((FAIL+1))
else
  echo "  PASS — _pulp_utils_cmake_dir is set at file scope"
  PASS=$((PASS+1))
fi

echo ""
echo "Result: $PASS pass, $FAIL fail"
[[ "$FAIL" == "0" ]]
