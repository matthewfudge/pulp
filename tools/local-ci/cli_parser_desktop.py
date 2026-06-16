"""Desktop subcommand parser construction for pulp ci-local."""

from __future__ import annotations

import argparse

from cli_parser_video import add_desktop_video_args


def add_desktop_source_args(command_parser: argparse.ArgumentParser) -> None:
    command_parser.add_argument(
        "--source-mode",
        choices=["live", "exact-sha"],
        default="live",
        help="Launch from the live checkout (default) or from an exact-SHA prepared source root.",
    )
    command_parser.add_argument("--branch", help="Branch label to record for desktop source provenance (default: current branch)")
    command_parser.add_argument("--sha", help="Exact commit SHA to prepare for desktop source mode (default: current HEAD)")
    command_parser.add_argument("--prepare-command", help="Optional shell command to run in the prepared source root before launch")
    command_parser.add_argument(
        "--prepare-timeout",
        type=float,
        default=900.0,
        help="Seconds to allow the optional prepare command to run (default: 900)",
    )


def add_desktop_subcommands(sub: argparse._SubParsersAction) -> None:
    p_desktop = sub.add_parser("desktop", help="Desktop automation setup, health, and status")
    desktop_sub = p_desktop.add_subparsers(dest="desktop_command")

    p_desktop_install = desktop_sub.add_parser("install", help="Prepare one desktop automation target")
    p_desktop_install.add_argument("target", help="Desktop target name (for example: mac, ubuntu, windows)")

    p_desktop_doctor = desktop_sub.add_parser("doctor", help="Run health checks for one desktop automation target")
    p_desktop_doctor.add_argument("target", help="Desktop target name (for example: mac, ubuntu, windows)")
    p_desktop_doctor.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_status = desktop_sub.add_parser("status", help="Show desktop automation config and target state")
    p_desktop_status.add_argument("target", nargs="?", help="Optional one-target filter")
    p_desktop_status.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_config = desktop_sub.add_parser("config", help="Show or update desktop automation config")
    desktop_config_sub = p_desktop_config.add_subparsers(dest="desktop_config_command")

    p_desktop_config_show = desktop_config_sub.add_parser("show", help="Show desktop automation config")
    p_desktop_config_show.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_config_set = desktop_config_sub.add_parser("set", help="Set a desktop automation config value")
    p_desktop_config_set.add_argument("key", help="Config key (artifact_root, publish_mode, publish_branch, retention_days, or target.<name>.<field>)")
    p_desktop_config_set.add_argument("value", help="New config value")
    p_desktop_config_set.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_recent = desktop_sub.add_parser("recent", help="Show recent desktop automation runs")
    p_desktop_recent.add_argument("target", nargs="?", help="Optional one-target filter")
    p_desktop_recent.add_argument("--action", help="Optional action filter (for example: smoke)")
    p_desktop_recent.add_argument("--limit", type=int, default=5, help="Number of runs to show (default: 5)")
    p_desktop_recent.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_proof = desktop_sub.add_parser("proof", help="Show successful desktop proofs grouped by target/action/source/SHA")
    p_desktop_proof.add_argument("target", nargs="?", help="Optional one-target filter")
    p_desktop_proof.add_argument("--action", help="Optional action filter (for example: inspect)")
    p_desktop_proof.add_argument(
        "--source-mode",
        choices=["live", "exact-sha", "legacy"],
        help="Optional source-mode filter for desktop proof summaries",
    )
    p_desktop_proof.add_argument("--sha", help="Optional exact full SHA filter")
    p_desktop_proof.add_argument("--branch", help="Optional branch filter")
    p_desktop_proof.add_argument("--limit", type=int, default=10, help="Number of proofs to show (default: 10)")
    p_desktop_proof.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_publish = desktop_sub.add_parser("publish", help="Stage a local HTML/JSON report for recent desktop automation runs")
    p_desktop_publish.add_argument("target", nargs="?", help="Optional one-target filter")
    p_desktop_publish.add_argument("--action", help="Optional action filter (for example: click)")
    p_desktop_publish.add_argument("--limit", type=int, default=5, help="Number of runs to include (default: 5)")
    p_desktop_publish.add_argument("--label", help="Optional report label")
    p_desktop_publish.add_argument("--output", help="Optional report output directory")
    p_desktop_publish.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_cleanup = desktop_sub.add_parser("cleanup", help="Prune old desktop automation bundles")
    p_desktop_cleanup.add_argument("target", nargs="?", help="Optional one-target filter")
    p_desktop_cleanup.add_argument(
        "--older-than-days",
        type=int,
        help="Remove bundles older than N days (default: configured retention)",
    )
    p_desktop_cleanup.add_argument("--keep-last", type=int, default=0, help="Always keep the newest N bundles per filter (default: 0)")
    p_desktop_cleanup.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_smoke = desktop_sub.add_parser("smoke", help="Run a desktop automation smoke action on one target")
    p_desktop_smoke.add_argument("target", help="Desktop target name")
    p_desktop_smoke.add_argument("--command", dest="launch_command", help="Quoted command to launch in the GUI session")
    p_desktop_smoke.add_argument("--bundle-id", help="macOS bundle identifier to launch via `open -b`")
    p_desktop_smoke.add_argument("--label", help="Optional artifact label")
    p_desktop_smoke.add_argument("--output", help="Optional screenshot output path")
    p_desktop_smoke.add_argument(
        "--capture-ui-snapshot",
        action="store_true",
        help="Request a Pulp-owned ViewInspector dump via PULP_VIEW_TREE_OUT and fail if the app does not write it",
    )
    p_desktop_smoke.add_argument("--click", help="Click at content-relative X,Y after launch")
    p_desktop_smoke.add_argument("--click-view-id", help="Click the center of the first visible ViewInspector node with this id")
    p_desktop_smoke.add_argument("--click-view-type", help="Click the center of the first visible ViewInspector node with this type")
    p_desktop_smoke.add_argument("--click-view-text", help="Click the center of the first visible ViewInspector node with this text")
    p_desktop_smoke.add_argument("--click-view-label", help="Click the center of the first visible ViewInspector node with this label")
    p_desktop_smoke.add_argument("--pulp-app-automation", action="store_true", help="Prefer a Pulp-owned in-app automation path for direct launch commands when supported")
    p_desktop_smoke.add_argument("--capture-before", action="store_true", help="Capture a before screenshot when running an interaction")
    p_desktop_smoke.add_argument("--settle-secs", type=float, default=0.5, help="Seconds to wait after an interaction before the final screenshot (default: 0.5)")
    p_desktop_smoke.add_argument("--timeout", type=float, default=15.0, help="Wait timeout in seconds (default: 15)")
    p_desktop_smoke.add_argument("--json", action="store_true", help="Emit machine-readable JSON")
    add_desktop_video_args(p_desktop_smoke)
    add_desktop_source_args(p_desktop_smoke)

    p_desktop_click = desktop_sub.add_parser("click", help="Launch an app, perform one click interaction, and capture before/after evidence")
    p_desktop_click.add_argument("target", help="Desktop target name")
    p_desktop_click.add_argument("--command", dest="launch_command", help="Quoted command to launch in the GUI session")
    p_desktop_click.add_argument("--bundle-id", help="macOS bundle identifier to launch via `open -b`")
    p_desktop_click.add_argument("--label", help="Optional artifact label")
    p_desktop_click.add_argument("--output", help="Optional screenshot output path")
    p_desktop_click.add_argument(
        "--capture-ui-snapshot",
        action="store_true",
        help="Request a Pulp-owned ViewInspector dump via PULP_VIEW_TREE_OUT when using a direct launch command",
    )
    p_desktop_click.add_argument("--click", help="Click at content-relative X,Y after launch")
    p_desktop_click.add_argument("--click-view-id", help="Click the center of the first visible ViewInspector node with this id")
    p_desktop_click.add_argument("--click-view-type", help="Click the center of the first visible ViewInspector node with this type")
    p_desktop_click.add_argument("--click-view-text", help="Click the center of the first visible ViewInspector node with this text")
    p_desktop_click.add_argument("--click-view-label", help="Click the center of the first visible ViewInspector node with this label")
    p_desktop_click.add_argument("--pulp-app-automation", action="store_true", help="Prefer a Pulp-owned in-app automation path for direct launch commands when supported")
    p_desktop_click.add_argument("--settle-secs", type=float, default=0.5, help="Seconds to wait after the interaction before the final screenshot (default: 0.5)")
    p_desktop_click.add_argument("--timeout", type=float, default=15.0, help="Wait timeout in seconds (default: 15)")
    p_desktop_click.add_argument("--json", action="store_true", help="Emit machine-readable JSON")
    add_desktop_video_args(p_desktop_click)
    add_desktop_source_args(p_desktop_click)

    p_desktop_verdict = desktop_sub.add_parser("verdict", help="Record a human review verdict on a desktop run manifest")
    p_desktop_verdict.add_argument("manifest", help="Path to the run manifest.json to update")
    verdict_group = p_desktop_verdict.add_mutually_exclusive_group(required=True)
    verdict_group.add_argument("--approved", action="store_true", help="Mark the proof as approved")
    verdict_group.add_argument("--needs-work", action="store_true", help="Mark the proof as needing work")
    p_desktop_verdict.add_argument("--notes", default="", help="Optional reviewer notes")
    p_desktop_verdict.add_argument("--reviewer", default="", help="Optional reviewer identity")
    p_desktop_verdict.add_argument("--issue-url", default="", help="Optional GitHub review issue URL")
    p_desktop_verdict.add_argument(
        "--comment-issue",
        action="store_true",
        help="Post the verdict summary to --issue-url with gh before updating the local manifest",
    )
    p_desktop_verdict.add_argument("--close-issue", action="store_true", help="Close --issue-url with gh after recording an approved verdict")
    p_desktop_verdict.add_argument(
        "--close-reason",
        default="completed",
        choices=["completed", "not planned"],
        help="GitHub issue close reason for --close-issue",
    )
    p_desktop_verdict.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_review_issue = desktop_sub.add_parser("review-issue", help="Create a local GitHub issue draft from a video review package")
    p_desktop_review_issue.add_argument("path", help="Path to review-package.json or a published report directory")
    p_desktop_review_issue.add_argument("--title", help="Optional issue title override")
    p_desktop_review_issue.add_argument("--repo", help="Optional GitHub repo for the suggested gh issue create command")
    p_desktop_review_issue.add_argument("--body-output", help="Optional markdown body output path")
    p_desktop_review_issue.add_argument("--json-output", help="Optional JSON draft output path")
    p_desktop_review_issue.add_argument(
        "--manifest-map-output",
        help="Optional JSON map output for review-watch after --create succeeds",
    )
    p_desktop_review_issue.add_argument("--check-files", action="store_true", help="Verify attachable MP4 files still exist and fit their recorded budget")
    p_desktop_review_issue.add_argument("--create", action="store_true", help="Create the review issue with gh after writing the local draft")
    p_desktop_review_issue.add_argument("--label", action="append", default=[], help="GitHub label to apply when --create is used; may be repeated")
    p_desktop_review_issue.add_argument("--assignee", action="append", default=[], help="GitHub assignee to apply when --create is used; may be repeated")
    p_desktop_review_issue.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_review_status = desktop_sub.add_parser("review-status", help="Check a video review issue for actionable approval or needs-work feedback")
    p_desktop_review_status.add_argument("issue_url", help="GitHub issue URL or number accepted by gh issue view")
    p_desktop_review_status.add_argument("--repo", help="Optional GitHub repo for gh issue view")
    p_desktop_review_status.add_argument("--manifest", help="Optional run manifest path for the suggested verdict command")
    p_desktop_review_status.add_argument("--close-issue", action="store_true", help="Include --close-issue in the suggested approved verdict command")
    p_desktop_review_status.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_review_watch = desktop_sub.add_parser("review-watch", help="Check open video review issues for actionable approval or needs-work feedback")
    p_desktop_review_watch.add_argument("--repo", help="Optional GitHub repo for gh issue list/view")
    p_desktop_review_watch.add_argument("--label", default="video-review", help="GitHub label to query (default: video-review)")
    p_desktop_review_watch.add_argument("--state", default="open", choices=["open", "closed", "all"], help="Issue state to query (default: open)")
    p_desktop_review_watch.add_argument("--state-file", help="Optional JSON cache that skips unchanged issues by updatedAt")
    p_desktop_review_watch.add_argument("--manifest-map", help="Optional JSON map from issue URL or number to run manifest path")
    p_desktop_review_watch.add_argument("--refresh", action="store_true", help="View every listed issue even when --state-file says it is unchanged")
    p_desktop_review_watch.add_argument("--close-issue", action="store_true", help="Include --close-issue in suggested approved verdict commands")
    p_desktop_review_watch.add_argument("--interval", type=float, default=0.0, help="Seconds between watch iterations (default: one-shot)")
    p_desktop_review_watch.add_argument("--max-iterations", type=int, default=1, help="Maximum watch iterations; use with --interval for short polling windows")
    p_desktop_review_watch.add_argument("--json", action="store_true", help="Emit machine-readable JSON")

    p_desktop_inspect = desktop_sub.add_parser("inspect", help="Launch an app and capture screenshot + available UI state")
    p_desktop_inspect.add_argument("target", help="Desktop target name (for example: mac)")
    p_desktop_inspect.add_argument("--command", dest="launch_command", help="Quoted command to launch in the GUI session")
    p_desktop_inspect.add_argument("--bundle-id", help="macOS bundle identifier to launch via `open -b` for screenshot-only inspect")
    p_desktop_inspect.add_argument("--label", help="Optional artifact label")
    p_desktop_inspect.add_argument("--output", help="Optional screenshot output path")
    p_desktop_inspect.add_argument("--pulp-app-automation", action="store_true", help="Use the Pulp-owned in-app automation path when the target adapter requires it")
    p_desktop_inspect.add_argument("--timeout", type=float, default=15.0, help="Wait timeout in seconds (default: 15)")
    p_desktop_inspect.add_argument("--json", action="store_true", help="Emit machine-readable JSON")
    add_desktop_source_args(p_desktop_inspect)
