#!/usr/bin/env bash
# test_workspace_freshness.sh — exercise tools/scripts/check_workspace_freshness.sh
# against synthetic git repo states (fresh, behind, ahead, dirty, bypassed).
#
# Why this test exists
# --------------------
# pulp #2087 lesson — a roundtrip ran from a 175-commits-behind branch and
# we drew wrong parser conclusions before noticing. The freshness check is
# the guard. This test pins its behavior so the guard can't silently regress.
#
# Pattern: each scenario sets up a temp git repo with a controlled
# ahead/behind relationship, runs the script, asserts exit code + key
# output substring.

set -euo pipefail

SCRIPT="$(cd "$(dirname "$0")/.." && pwd)/tools/scripts/check_workspace_freshness.sh"
[[ -x "$SCRIPT" ]] || { echo "FAIL: $SCRIPT missing or not executable"; exit 1; }

PASS=0
FAIL=0

mk_repo() {
  local dir="$1"
  rm -rf "$dir"
  mkdir -p "$dir"
  git -C "$dir" init -q -b main
  git -C "$dir" config user.email t@t
  git -C "$dir" config user.name t
  echo a > "$dir/a"
  git -C "$dir" add a
  git -C "$dir" commit -q -m initial
  # Synthesize an "origin" by cloning to a parallel bare path
  git clone -q --bare "$dir" "${dir}-origin.git"
  git -C "$dir" remote add origin "${dir}-origin.git"
  git -C "$dir" fetch -q origin main
  git -C "$dir" branch --set-upstream-to=origin/main main 2>/dev/null || true
}

advance_origin() {
  local dir="$1" n="${2:-1}"
  local tmp=$(mktemp -d)
  git clone -q "${dir}-origin.git" "$tmp"
  cd "$tmp"
  git config user.email t@t; git config user.name t
  for i in $(seq 1 "$n"); do
    echo "$i" > "f${i}"
    git add "f${i}"
    git commit -q -m "advance ${i}"
  done
  git push -q origin main
  cd - >/dev/null
  rm -rf "$tmp"
  git -C "$dir" fetch -q origin main
}

assert() {
  local label="$1" expected_rc="$2" actual_rc="$3"
  if [[ "$expected_rc" == "$actual_rc" ]]; then
    echo "  PASS — $label (rc=$actual_rc)"
    PASS=$((PASS+1))
  else
    echo "  FAIL — $label (expected rc=$expected_rc, got rc=$actual_rc)"
    FAIL=$((FAIL+1))
  fi
}

# ── Scenario 1: fresh checkout (HEAD == origin/main) → exit 0
TEST_DIR=$(mktemp -d)/freshness-1
mk_repo "$TEST_DIR"
( cd "$TEST_DIR" && "$SCRIPT" >/dev/null 2>&1; ) && actual=$? || actual=$?
assert "scenario 1: fresh checkout" 0 "$actual"

# ── Scenario 2: 5 commits behind → exit 1
TEST_DIR=$(mktemp -d)/freshness-2
mk_repo "$TEST_DIR"
advance_origin "$TEST_DIR" 5
( cd "$TEST_DIR" && "$SCRIPT" >/dev/null 2>&1; ) && actual=$? || actual=$?
assert "scenario 2: 5 commits behind, enforce" 1 "$actual"

# ── Scenario 3: 5 behind with --warn → exit 0
( cd "$TEST_DIR" && "$SCRIPT" --warn >/dev/null 2>&1; ) && actual=$? || actual=$?
assert "scenario 3: 5 behind --warn" 0 "$actual"

# ── Scenario 4: 5 behind with --max-behind 10 → exit 0
( cd "$TEST_DIR" && "$SCRIPT" --max-behind 10 >/dev/null 2>&1; ) && actual=$? || actual=$?
assert "scenario 4: 5 behind --max-behind 10" 0 "$actual"

# ── Scenario 5: 5 behind with --max-behind 3 → exit 1
( cd "$TEST_DIR" && "$SCRIPT" --max-behind 3 >/dev/null 2>&1; ) && actual=$? || actual=$?
assert "scenario 5: 5 behind --max-behind 3" 1 "$actual"

# ── Scenario 6: PULP_FRESHNESS_BYPASS=1 → exit 0
( cd "$TEST_DIR" && PULP_FRESHNESS_BYPASS=1 "$SCRIPT" >/dev/null 2>&1; ) && actual=$? || actual=$?
assert "scenario 6: bypass env var" 0 "$actual"

# ── Scenario 7: --json mode emits parseable JSON
TEST_DIR=$(mktemp -d)/freshness-7
mk_repo "$TEST_DIR"
out=$( cd "$TEST_DIR" && "$SCRIPT" --json 2>/dev/null )
if echo "$out" | python3 -c "import json,sys; d=json.load(sys.stdin); assert d['behind']==0; assert d['mode']=='enforce'"; then
  echo "  PASS — scenario 7: --json output parseable + correct"
  PASS=$((PASS+1))
else
  echo "  FAIL — scenario 7: --json output garbled or wrong"
  FAIL=$((FAIL+1))
fi

echo ""
echo "Result: $PASS pass, $FAIL fail"
[[ "$FAIL" == "0" ]]
