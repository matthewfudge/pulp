"""Desktop video-proof review commands (human verdict on a run manifest)."""

from __future__ import annotations

import argparse
from collections.abc import Callable
import json
from pathlib import Path
import subprocess


def cmd_desktop_verdict(
    args: argparse.Namespace,
    *,
    now_iso_fn: Callable[[], str],
    atomic_write_text_fn: Callable[[Path, str], None],
    run_fn: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
    print_fn: Callable[[str], None] = print,
) -> int:
    manifest_path = Path(args.manifest).expanduser()
    if not manifest_path.is_file():
        print_fn(f"Error: desktop run manifest not found: {manifest_path}")
        return 1
    try:
        manifest = json.loads(manifest_path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        print_fn(f"Error: could not read desktop run manifest: {exc}")
        return 1
    status = "approved" if getattr(args, "approved", False) else "needs-work"
    reviewed_at = now_iso_fn()
    run_label = str(manifest.get("label") or manifest_path.parent.name)
    review = {
        "status": status,
        "reviewed_at": reviewed_at,
    }
    if args.notes:
        review["notes"] = args.notes
    if args.reviewer:
        review["reviewer"] = args.reviewer
    if args.issue_url:
        review["issue_url"] = args.issue_url
    if status == "approved":
        review["close_review_issue"] = True
    else:
        review["close_review_issue"] = False
        review["follow_up_required"] = True
    markdown_path = manifest_path.parent / "review-verdict.md"
    json_path = manifest_path.parent / "review-verdict.json"
    if status == "approved":
        summary_comment = f"Approved desktop video proof `{run_label}`."
        if args.issue_url:
            summary_comment += f" Review issue: {args.issue_url}."
        if args.notes:
            summary_comment += f" Notes: {args.notes}"
        follow_up = None
    else:
        summary_comment = f"Desktop video proof `{run_label}` needs another iteration."
        if args.notes:
            summary_comment += f" Requested change: {args.notes}"
        follow_up = {
            "kind": "same-issue-checklist",
            "text": f"- [ ] Re-record `{run_label}` after addressing: {args.notes or 'reviewer feedback'}",
        }
    comment_lines = [summary_comment]
    if follow_up:
        comment_lines.extend(["", "Follow-up:", follow_up["text"]])
    issue_comment_body = "\n".join(comment_lines)
    comment_result = None
    if getattr(args, "comment_issue", False) and not getattr(args, "close_issue", False):
        if not args.issue_url:
            print_fn("Error: --comment-issue requires --issue-url")
            return 1
        comment_argv = [
            "gh",
            "issue",
            "comment",
            args.issue_url,
            "--body",
            issue_comment_body,
        ]
        comment_process = run_fn(comment_argv, capture_output=True, text=True, check=False)
        comment_result = {
            "command": comment_argv,
            "returncode": int(comment_process.returncode),
            "stdout": comment_process.stdout,
            "stderr": comment_process.stderr,
        }
        if comment_process.returncode != 0:
            detail = (comment_process.stderr or comment_process.stdout or "gh issue comment failed").strip()
            print_fn(f"Error: {detail}")
            return 1
    close_result = None
    if getattr(args, "close_issue", False):
        if status != "approved":
            print_fn("Error: --close-issue requires --approved")
            return 1
        if not args.issue_url:
            print_fn("Error: --close-issue requires --issue-url")
            return 1
        close_argv = [
            "gh",
            "issue",
            "close",
            args.issue_url,
            "--comment",
            issue_comment_body,
            "--reason",
            getattr(args, "close_reason", "completed") or "completed",
        ]
        close_process = run_fn(close_argv, capture_output=True, text=True, check=False)
        close_result = {
            "command": close_argv,
            "returncode": int(close_process.returncode),
            "stdout": close_process.stdout,
            "stderr": close_process.stderr,
        }
        if close_process.returncode != 0:
            detail = (close_process.stderr or close_process.stdout or "gh issue close failed").strip()
            print_fn(f"Error: {detail}")
            return 1
    verdict_payload = {
        "kind": "desktop-video-proof-verdict",
        "manifest": str(manifest_path),
        "status": status,
        "reviewed_at": reviewed_at,
        "reviewer": args.reviewer or None,
        "issue_url": args.issue_url or None,
        "notes": args.notes or None,
        "close_review_issue": bool(review["close_review_issue"]),
        "follow_up_required": bool(review.get("follow_up_required", False)),
        "summary_comment": summary_comment,
        "follow_up": follow_up,
        "issue_comment": comment_result,
        "issue_close": close_result,
    }
    markdown_lines = [
        f"# Desktop Video Proof Verdict: {status}",
        "",
        f"- Manifest: `{manifest_path}`",
        f"- Run: `{run_label}`",
        f"- Reviewed at: `{reviewed_at}`",
    ]
    if args.reviewer:
        markdown_lines.append(f"- Reviewer: `{args.reviewer}`")
    if args.issue_url:
        markdown_lines.append(f"- Review issue: {args.issue_url}")
    if args.notes:
        markdown_lines.append(f"- Notes: {args.notes}")
    markdown_lines.extend(
        [
            "",
            "## Issue Comment",
            "",
            summary_comment,
        ]
    )
    if follow_up:
        markdown_lines.extend(["", "## Follow-up", "", follow_up["text"]])
    if status == "approved":
        markdown_lines.extend(["", "## Closeout", "", "Close the review issue after posting the summary comment."])
    else:
        markdown_lines.extend(["", "## Closeout", "", "Keep the review issue open until a replacement proof is recorded."])
    atomic_write_text_fn(markdown_path, "\n".join(markdown_lines) + "\n")
    atomic_write_text_fn(json_path, json.dumps(verdict_payload, indent=2) + "\n")
    review["verdict_markdown"] = str(markdown_path)
    review["verdict_json"] = str(json_path)
    review["summary_comment"] = summary_comment
    review["issue_comment_body"] = issue_comment_body
    if follow_up:
        review["follow_up"] = follow_up
    if comment_result:
        review["issue_comment"] = comment_result
    if close_result:
        review["issue_close"] = close_result
    manifest["review"] = review
    atomic_write_text_fn(manifest_path, json.dumps(manifest, indent=2) + "\n")
    payload = {
        "manifest": str(manifest_path),
        "review": review,
        "verdict_markdown": str(markdown_path),
        "verdict_json": str(json_path),
    }
    if getattr(args, "json", False):
        print_fn(json.dumps(payload, indent=2))
    else:
        print_fn(f"Desktop proof verdict recorded: {status}")
        print_fn(f"  manifest: {manifest_path}")
        print_fn(f"  verdict_markdown: {markdown_path}")
        print_fn(f"  verdict_json: {json_path}")
        if args.issue_url:
            print_fn(f"  issue_url: {args.issue_url}")
        if comment_result:
            print_fn("  issue_comment: posted")
        if close_result:
            print_fn("  issue_close: closed")
    return 0
