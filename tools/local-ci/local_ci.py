#!/usr/bin/env python3
"""Local CI runner for Pulp — validates branches on Mac, Ubuntu, and Windows.

Usage:
    pulp ci-local enqueue [branch]   # Queue current or named branch
    pulp ci-local drain              # Process all pending jobs
    pulp ci-local status             # Show queue and recent results
    pulp ci-local run [branch]       # Enqueue + drain immediately

Targets:
    mac      — always runs locally via validate-build.sh
    ubuntu   — ssh ubuntu, falls back to UTM VM if unreachable
    windows  — ssh win2 (Proxmox), falls back to UTM VM if unreachable
"""

from __future__ import annotations

import argparse
import json
import os
import shlex
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONFIG_PATH = Path(__file__).resolve().parent / "config.json"
QUEUE_PATH = Path(__file__).resolve().parent / "queue.json"
RESULTS_DIR = Path(__file__).resolve().parent / "results"


def load_config() -> dict:
    return json.loads(CONFIG_PATH.read_text())


def load_queue() -> list[dict]:
    if not QUEUE_PATH.exists():
        return []
    return json.loads(QUEUE_PATH.read_text())


def save_queue(queue: list[dict]) -> None:
    QUEUE_PATH.write_text(json.dumps(queue, indent=2) + "\n")


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def current_branch() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "--abbrev-ref", "HEAD"],
        cwd=ROOT, capture_output=True, text=True, check=True,
    )
    return result.stdout.strip()


def current_sha() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=ROOT, capture_output=True, text=True, check=True,
    )
    return result.stdout.strip()


# ── VM Management ────────────────────────────────────────────────────────────

def ssh_reachable(host: str, timeout: int = 5) -> bool:
    """Check if an SSH host is reachable."""
    result = subprocess.run(
        ["ssh", "-o", f"ConnectTimeout={timeout}", "-o", "BatchMode=yes",
         host, "echo", "up"],
        capture_output=True, text=True,
    )
    return result.returncode == 0


def utmctl_vm_status(vm_name: str) -> str | None:
    """Get UTM VM status (started/stopped/None if not found)."""
    result = subprocess.run(
        ["utmctl", "list"], capture_output=True, text=True,
    )
    if result.returncode != 0:
        return None
    for line in result.stdout.splitlines():
        if vm_name in line:
            parts = line.split()
            if len(parts) >= 2:
                return parts[1]  # status column
    return None


def utmctl_start(vm_name: str) -> bool:
    """Start a UTM VM by name."""
    result = subprocess.run(
        ["utmctl", "start", vm_name], capture_output=True, text=True,
    )
    return result.returncode == 0


def ensure_host_reachable(target_name: str, target_cfg: dict, defaults: dict) -> str | None:
    """Ensure a target host is reachable, starting UTM VM if needed.

    Returns the SSH host to use, or None if unreachable.
    Tries: primary host → fallback_host → UTM VM start → poll primary host.
    """
    host = target_cfg["host"]
    fallback_host = target_cfg.get("fallback_host")
    timeout = defaults.get("ssh_timeout_secs", 5)

    # Try primary host first (e.g. local UTM VM)
    print(f"  [{target_name}] Checking ssh {host}...")
    if ssh_reachable(host, timeout):
        print(f"  [{target_name}] {host} is up")
        return host

    # Try fallback host (e.g. Proxmox via Tailscale)
    if fallback_host:
        print(f"  [{target_name}] {host} unreachable, trying fallback ssh {fallback_host}...")
        if ssh_reachable(fallback_host, timeout):
            print(f"  [{target_name}] {fallback_host} is up")
            return fallback_host

    # Both SSH hosts unreachable — try UTM fallback
    fallback = target_cfg.get("utm_fallback")
    if not fallback:
        print(f"  [{target_name}] {host} unreachable, no UTM fallback configured")
        return None

    vm_name = fallback["vm_name"]
    boot_wait = fallback.get("boot_wait_secs", 30)
    ssh_retry = fallback.get("ssh_retry_secs", 60)

    print(f"  [{target_name}] {host} unreachable, checking UTM VM '{vm_name}'...")

    status = utmctl_vm_status(vm_name)
    if status is None:
        print(f"  [{target_name}] UTM VM '{vm_name}' not found")
        return None

    if status != "started":
        print(f"  [{target_name}] Starting UTM VM '{vm_name}'...")
        if not utmctl_start(vm_name):
            print(f"  [{target_name}] Failed to start UTM VM")
            return None
        print(f"  [{target_name}] Waiting {boot_wait}s for boot...")
        time.sleep(boot_wait)

    # Poll SSH until reachable or timeout
    deadline = time.time() + ssh_retry
    attempt = 0
    while time.time() < deadline:
        attempt += 1
        if ssh_reachable(host, timeout):
            print(f"  [{target_name}] {host} up after UTM start (attempt {attempt})")
            return host
        time.sleep(5)

    print(f"  [{target_name}] {host} still unreachable after UTM start")
    return None


# ── Validation Runners ───────────────────────────────────────────────────────

def run_local_validation(branch: str, exclude_tests: str = "") -> dict:
    """Run validate-build.sh locally."""
    print(f"  [mac] Running local validation on {branch}...")
    start = time.time()

    cmd = ["./validate-build.sh", "--quiet"]
    if exclude_tests:
        cmd += ["--exclude-regex", exclude_tests]

    result = subprocess.run(
        cmd,
        cwd=ROOT, capture_output=True, text=True, timeout=1800,  # 30 min
    )

    elapsed = round(time.time() - start, 1)
    return {
        "target": "mac",
        "status": "pass" if result.returncode == 0 else "fail",
        "exit_code": result.returncode,
        "duration_secs": elapsed,
        "stdout_tail": result.stdout[-2000:] if result.stdout else "",
        "stderr_tail": result.stderr[-2000:] if result.stderr else "",
    }


def run_ssh_validation(target_name: str, host: str, repo_path: str,
                       branch: str, is_windows: bool = False,
                       exclude_tests: str = "") -> dict:
    """Run validation on a remote host over SSH."""
    print(f"  [{target_name}] Running validation on {host}:{repo_path}...")
    start = time.time()

    if is_windows:
        # Windows: cmake finds MSVC via VS registry automatically
        remote_cmd = (
            f"cd /d {repo_path} && "
            f"git fetch origin && "
            f"(git checkout {branch} 2>nul || git checkout -b {branch} origin/{branch}) && "
            f"git reset --hard origin/{branch} && "
            f"cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && "
            f"cmake --build build --config Release && "
            f"ctest --test-dir build --output-on-failure -C Release"
            + (f" --exclude-regex \"{exclude_tests}\"" if exclude_tests else "")
        )
        cmd = ["ssh", host, f'cmd /c "{remote_cmd}"']
    else:
        # Unix: use validate-build.sh
        repo_q = shlex.quote(repo_path)
        branch_q = shlex.quote(branch)
        remote_cmd = (
            f"set -e; "
            f"export GIT_LFS_SKIP_SMUDGE=1; "
            f"cd {repo_q}; "
            f"git fetch origin; "
            f"if git show-ref --verify --quiet refs/heads/{branch_q}; then "
            f"git checkout {branch_q}; "
            f"else "
            f"git checkout -b {branch_q} origin/{branch_q}; "
            f"fi; "
            f"git reset --hard origin/{branch_q}; "
            f"./validate-build.sh --quiet"
            + (f" --exclude-regex {shlex.quote(exclude_tests)}" if exclude_tests else "")
        )
        cmd = ["ssh", host, "bash", "-c", shlex.quote(remote_cmd)]

    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=1800,  # 30 min for full builds
        )
        elapsed = round(time.time() - start, 1)
        return {
            "target": target_name,
            "status": "pass" if result.returncode == 0 else "fail",
            "exit_code": result.returncode,
            "duration_secs": elapsed,
            "stdout_tail": result.stdout[-2000:] if result.stdout else "",
            "stderr_tail": result.stderr[-2000:] if result.stderr else "",
        }
    except subprocess.TimeoutExpired:
        return {
            "target": target_name,
            "status": "timeout",
            "exit_code": -1,
            "duration_secs": round(time.time() - start, 1),
            "stdout_tail": "",
            "stderr_tail": "Validation timed out after 600s",
        }


# ── Job Processing ───────────────────────────────────────────────────────────

def process_job(job: dict, config: dict) -> dict:
    """Process a single validation job across all targets."""
    branch = job["branch"]
    targets = config["targets"]
    defaults = config["defaults"]
    results = []

    print(f"\n=== Validating branch: {branch} ===\n")

    # Mac (local)
    mac_cfg = targets.get("mac", {})
    if mac_cfg.get("enabled", True):
        results.append(run_local_validation(branch, mac_cfg.get("exclude_tests", "")))

    # Ubuntu
    ubuntu_cfg = targets.get("ubuntu")
    if ubuntu_cfg and ubuntu_cfg.get("enabled", True):
        host = ensure_host_reachable("ubuntu", ubuntu_cfg, defaults)
        if host:
            results.append(run_ssh_validation(
                "ubuntu", host, ubuntu_cfg["repo_path"], branch,
                exclude_tests=ubuntu_cfg.get("exclude_tests", "")))
        else:
            results.append({
                "target": "ubuntu",
                "status": "unreachable",
                "exit_code": -1,
                "duration_secs": 0,
            })

    # Windows — try win2 (Proxmox) first, UTM fallback
    win_cfg = targets.get("windows")
    if win_cfg and win_cfg.get("enabled", True):
        host = ensure_host_reachable("windows", win_cfg, defaults)
        if host:
            results.append(run_ssh_validation(
                "windows", host, win_cfg["repo_path"], branch,
                is_windows=True,
                exclude_tests=win_cfg.get("exclude_tests", "")))
        else:
            results.append({
                "target": "windows",
                "status": "unreachable",
                "exit_code": -1,
                "duration_secs": 0,
            })

    return {
        "branch": branch,
        "sha": job.get("sha", ""),
        "queued_at": job.get("queued_at", ""),
        "completed_at": now_iso(),
        "results": results,
        "overall": "pass" if all(r["status"] == "pass" for r in results) else "fail",
    }


def save_result(result: dict) -> Path:
    """Save a job result to the results directory."""
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    ts = datetime.now().strftime("%Y%m%d-%H%M%S")
    branch_slug = result["branch"].replace("/", "-")
    path = RESULTS_DIR / f"{ts}-{branch_slug}.json"
    path.write_text(json.dumps(result, indent=2) + "\n")
    return path


# ── CLI Commands ─────────────────────────────────────────────────────────────

def cmd_enqueue(args: argparse.Namespace) -> int:
    branch = args.branch or current_branch()
    sha = current_sha()

    queue = load_queue()

    # Don't double-queue same branch+sha
    for job in queue:
        if job["branch"] == branch and job.get("sha") == sha:
            print(f"Already queued: {branch} @ {sha}")
            return 0

    job = {
        "branch": branch,
        "sha": sha,
        "queued_at": now_iso(),
        "status": "pending",
    }
    queue.append(job)
    save_queue(queue)
    print(f"Enqueued: {branch} @ {sha}")
    return 0


def cmd_drain(_args: argparse.Namespace) -> int:
    queue = load_queue()
    config = load_config()

    pending = [j for j in queue if j["status"] == "pending"]
    if not pending:
        print("No pending jobs.")
        return 0

    print(f"Processing {len(pending)} pending job(s)...\n")

    any_failure = False
    for job in pending:
        result = process_job(job, config)
        result_path = save_result(result)
        job["status"] = "completed"
        job["result_file"] = str(result_path)
        save_queue(queue)

        # Print summary
        print(f"\n--- Result: {result['branch']} ---")
        for r in result["results"]:
            icon = "PASS" if r["status"] == "pass" else r["status"].upper()
            dur = r.get("duration_secs", 0)
            print(f"  {r['target']:10s}  {icon:12s}  {dur}s")
        print(f"  {'overall':10s}  {result['overall'].upper()}")
        print(f"  Saved: {result_path}\n")

        if result["overall"] != "pass":
            any_failure = True

    # Clean completed jobs older than the last 10
    completed = [j for j in queue if j["status"] == "completed"]
    if len(completed) > 10:
        to_remove = completed[:-10]
        queue = [j for j in queue if j not in to_remove]
        save_queue(queue)

    return 1 if any_failure else 0


def cmd_run(args: argparse.Namespace) -> int:
    args.branch = args.branch or current_branch()
    cmd_enqueue(args)
    return cmd_drain(args)


def cmd_status(_args: argparse.Namespace) -> int:
    queue = load_queue()

    pending = [j for j in queue if j["status"] == "pending"]
    completed = [j for j in queue if j["status"] == "completed"]

    if pending:
        print(f"Pending ({len(pending)}):")
        for j in pending:
            print(f"  {j['branch']} @ {j.get('sha', '?')}  queued {j['queued_at']}")
    else:
        print("No pending jobs.")

    if completed:
        print(f"\nRecent ({len(completed)}):")
        for j in completed[-5:]:
            rf = j.get("result_file", "")
            if rf and Path(rf).exists():
                r = json.loads(Path(rf).read_text())
                overall = r.get("overall", "?").upper()
                targets = ", ".join(
                    f"{t['target']}={t['status']}" for t in r.get("results", []))
                print(f"  {j['branch']} @ {j.get('sha', '?')}  {overall}  [{targets}]")
            else:
                print(f"  {j['branch']} @ {j.get('sha', '?')}  (result file missing)")

    # Show VM status
    print("\nVM Status:")
    for vm in ["Ubuntu 24.04 desktop", "Windows"]:
        status = utmctl_vm_status(vm)
        print(f"  {vm}: {status or 'not found'}")

    for host in ["ubuntu", "win2"]:
        reachable = "up" if ssh_reachable(host, 3) else "down"
        print(f"  ssh {host}: {reachable}")

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Local CI runner for Pulp",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    sub = parser.add_subparsers(dest="command")

    p_enqueue = sub.add_parser("enqueue", help="Queue a branch for validation")
    p_enqueue.add_argument("branch", nargs="?", help="Branch name (default: current)")

    p_drain = sub.add_parser("drain", help="Process all pending jobs")

    p_run = sub.add_parser("run", help="Enqueue + drain immediately")
    p_run.add_argument("branch", nargs="?", help="Branch name (default: current)")

    p_status = sub.add_parser("status", help="Show queue and results")

    args = parser.parse_args()

    commands = {
        "enqueue": cmd_enqueue,
        "drain": cmd_drain,
        "run": cmd_run,
        "status": cmd_status,
    }

    if args.command in commands:
        return commands[args.command](args)

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
