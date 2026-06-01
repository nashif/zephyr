#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0
"""Analyze pull request backlog in the Zephyr GitHub repository.

For every open PR created at least --age days ago the script collects
a set of signals, computes the time since the last *meaningful* activity
(commit pushed, review submitted, or human comment — not label/CI/bot
updates), and classifies each PR into one or more backlog categories.
All findings are written to a self-contained HTML report.

What counts as "meaningful activity"
-------------------------------------
- A commit pushed to the PR branch
- A review submitted (approval, changes-requested, or reviewer comment)
- An issue-level or inline review comment left by a human

What is intentionally excluded
-------------------------------
- Label additions / removals
- Assignee changes
- CI re-runs and status updates
- Cross-references from other issues / PRs
- Any other metadata-only operation that bumps ``updated_at``

Signals collected per PR
------------------------
- Age (days since opened) and meaningful-idle time
- Size (additions + deletions, files changed, commits)
- Number of reviewers requested
- Assignment status (assignee present / absent)
- CI/check-run status (pass / fail / pending) with failing check names
- Comment and discussion activity
- Areas covered by the change (via MAINTAINERS.yml + get_maintainer.py)
- Whether the submitter is a maintainer of any touched area
- Review state breakdown: approvals, changes-requested, commented, dismissed
- Zephyr 2-approval rule coverage (2 approvals required, one from assignee)

Usage
-----
    # Requires a GitHub token with repo:read scope
    export GITHUB_TOKEN=<token>

    python3 scripts/ci/analyze_pr_backlog.py \\
        [--age 14] \\
        [--org zephyrproject-rtos] \\
        [--repo zephyr] \\
        [--maintainer-file MAINTAINERS.yml] \\
        [--max-prs 200] \\
        [--exclude-drafts] \\
        [--output pr_backlog.html] \\
        [--verbose]
"""

import argparse
import collections
import datetime
import html
import json
import os
import pathlib
import sys
import textwrap
import time

from pathlib import Path

# ---------------------------------------------------------------------------
# Third-party imports – graceful error if not installed
# ---------------------------------------------------------------------------
try:
    from github import Auth, Github, GithubException
except ImportError:
    sys.exit(
        "PyGitHub is required.  Install with: pip install PyGithub"
    )

# ---------------------------------------------------------------------------
# get_maintainer integration
# ---------------------------------------------------------------------------
_SCRIPT_DIR = Path(__file__).resolve().parent
_ZEPHYR_BASE = _SCRIPT_DIR.parents[1]
sys.path.insert(0, str(_SCRIPT_DIR.parent))

try:
    from get_maintainer import Maintainers
    _HAS_MAINTAINERS = True
except ImportError:
    _HAS_MAINTAINERS = False

# ---------------------------------------------------------------------------
# PR backlog categories
# ---------------------------------------------------------------------------
CAT_NO_REVIEWER = "no_reviewer"
CAT_NO_REVIEW = "no_review"
CAT_NO_ASSIGNEE = "no_assignee"
CAT_CI_FAILING = "ci_failing"
CAT_CI_PENDING = "ci_pending"
CAT_CHANGES_REQUESTED = "changes_requested"
CAT_AWAITING_SECOND_REVIEW = "awaiting_second_review"
CAT_MANY_AREAS = "many_areas"
CAT_MAINTAINER_SUBMITTED = "maintainer_submitted"
CAT_LARGE_PR = "large_pr"
CAT_DISCUSSION_ACTIVE = "discussion_active"
CAT_DISCUSSION_STALE = "discussion_stale"
CAT_MISSING_ASSIGNEE_APPROVAL = "missing_assignee_approval"
CAT_NEARLY_APPROVED = "nearly_approved"
CAT_NEEDS_REBASE = "needs_rebase"
CAT_DNM = "dnm"
CAT_ARCH_REVIEW = "arch_review"

CATEGORY_META = {
    CAT_NO_REVIEWER: {
        "label": "No reviewer assigned",
        "color": "#e74c3c",
        "description": (
            "No reviewer has been requested.  Nobody is formally "
            "responsible for looking at the PR."
        ),
    },
    CAT_NO_REVIEW: {
        "label": "No reviews at all",
        "color": "#c0392b",
        "description": (
            "The PR has received zero reviews of any kind — no approvals, "
            "no change requests, and no reviewer comments.  It has been "
            "completely ignored since it was opened."
        ),
    },
    CAT_NO_ASSIGNEE: {
        "label": "No assignee",
        "color": "#e67e22",
        "description": (
            "The PR has no assignee.  Zephyr requires an assignee approval "
            "to merge; without one the merge path is unclear."
        ),
    },
    CAT_CI_FAILING: {
        "label": "CI failing",
        "color": "#c0392b",
        "description": (
            "One or more required CI checks are failing.  The PR cannot be "
            "merged until CI is green."
        ),
    },
    CAT_CI_PENDING: {
        "label": "CI pending / not run",
        "color": "#f39c12",
        "description": (
            "CI checks have not finished or have never been triggered."
        ),
    },
    CAT_CHANGES_REQUESTED: {
        "label": "Changes requested (unresolved)",
        "color": "#8e44ad",
        "description": (
            "At least one reviewer requested changes.  Those reviews have "
            "not been dismissed or superseded by a later approval from the "
            "same reviewer."
        ),
    },
    CAT_AWAITING_SECOND_REVIEW: {
        "label": "Awaiting second review",
        "color": "#2980b9",
        "description": (
            "The PR already has one approval but needs a second one to "
            "satisfy the 2-approval policy."
        ),
    },
    CAT_MANY_AREAS: {
        "label": "Spans many code areas (≥ 4)",
        "color": "#16a085",
        "description": (
            "The change touches four or more MAINTAINERS code areas "
            "(Tests, Samples, Boards/SoCs, Release, Documentation, and "
            "MAINTAINERS entries are excluded from the count).  Many "
            "independent owners are involved with potentially unclear "
            "ownership."
        ),
    },
    CAT_MAINTAINER_SUBMITTED: {
        "label": "Submitted by area maintainer",
        "color": "#27ae60",
        "description": (
            "The author is a maintainer of one of the touched areas and is "
            "also the PR's assignee — waiting for external reviews to land "
            "their own change."
        ),
    },
    CAT_LARGE_PR: {
        "label": "Large PR (> 500 lines)",
        "color": "#d35400",
        "description": (
            "The PR changes more than 500 lines.  Large PRs typically take "
            "longer to review."
        ),
    },
    CAT_DISCUSSION_ACTIVE: {
        "label": "Active discussion (unresolved threads)",
        "color": "#7f8c8d",
        "description": (
            "There are open review threads that may be blocking progress."
        ),
    },
    CAT_DISCUSSION_STALE: {
        "label": "Stale discussion (no recent activity)",
        "color": "#95a5a6",
        "description": (
            "The PR has comments but no activity in the last 14 days – "
            "the conversation may have stalled."
        ),
    },
    CAT_MISSING_ASSIGNEE_APPROVAL: {
        "label": "Missing assignee approval",
        "color": "#c0392b",
        "description": (
            "The PR has an assignee but that person has not yet approved it. "
            "Zephyr requires an assignee approval for merge."
        ),
    },
    CAT_NEARLY_APPROVED: {
        "label": "Nearly approved (2 approvals met)",
        "color": "#1abc9c",
        "description": (
            "The PR already has 2+ approvals including the assignee – it may "
            "only be waiting for a final merge trigger."
        ),
    },
    CAT_NEEDS_REBASE: {
        "label": "Needs rebase (merge conflict)",
        "color": "#c0392b",
        "description": (
            "The PR branch has a merge conflict with the target branch.  "
            "The author must rebase or resolve conflicts before this can land."
        ),
    },
    CAT_DNM: {
        "label": "Do Not Merge (DNM)",
        "color": "#2c3e50",
        "description": (
            "The PR carries a DNM (Do Not Merge) label.  It is intentionally "
            "blocked from landing until the label is removed."
        ),
    },
    CAT_ARCH_REVIEW: {
        "label": "Architecture Review",
        "color": "#6c3483",
        "description": (
            "The PR carries an 'Architecture Review' or 'TSC' label, "
            "indicating it requires sign-off from the Architecture Working "
            "Group or the Technical Steering Committee before merging."
        ),
    },
}

# Size thresholds
SIZE_LARGE_LINES = 500
SIZE_MANY_AREAS = 4

# Area names that are considered meta (infrastructure / overhead) and are
# excluded when counting how many *code* areas a PR touches.  The names
# are matched as substrings (case-insensitive) against the area name
# returned by MAINTAINERS.yml.
_META_AREA_SUBSTRINGS = (
    "release notes",
    "documentation",
    "samples",
    "tests",
    "release",
    "maintainers",
    "boards",
    "socs",
    "soc ",
    "board ",
    "platform",
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _gh_connect(token):
    if token:
        return Github(auth=Auth.Token(token))
    return Github()


def _age_days(dt):
    """Return the number of days since *dt* (timezone-aware or naive)."""
    now = datetime.datetime.now(datetime.timezone.utc)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=datetime.timezone.utc)
    return (now - dt).days


def _last_commit_dt(pr):
    """
    Return the committer date of the most-recent commit on the PR branch,
    or None if it cannot be determined.

    Uses .reversed[0] to avoid fetching every page of commits when the PR
    has many commits.
    """
    try:
        last = pr.get_commits().reversed[0]
        dt = last.commit.committer.date
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=datetime.timezone.utc)
        return dt
    except Exception:
        return None


def _latest_reviews(reviews):
    """
    Collapse per-reviewer review history into the *latest* review state.
    Returns a dict: login -> state (APPROVED | CHANGES_REQUESTED | COMMENTED | DISMISSED)
    """
    by_user = {}
    for review in reviews:
        if review.state == "PENDING":
            continue
        login = review.user.login if review.user else "__unknown__"
        by_user[login] = review.state
    return by_user


def _ci_details(pr):
    """
    Return (status, failing_checks) for the PR head commit.

    status        -- 'pass', 'fail', 'pending', or 'unknown'
    failing_checks -- sorted, deduplicated list of check/context names
                      that are currently failing
    """
    try:
        commit = pr.get_commits().reversed[0]
    except Exception:
        return "unknown", []

    states = []
    failing = []

    # Combined commit status (older Checks API)
    try:
        combined = commit.get_combined_status()
        for s in combined.statuses:
            if s.state in ("failure", "error"):
                failing.append(s.context)
                states.append("failure")
            elif s.state == "pending":
                states.append("pending")
            else:
                states.append("success")
    except Exception:
        pass

    # Check runs (GitHub Actions / newer API).
    # Keep only the latest run per check name (highest ID) so that cancelled
    # runs that were superseded by a newer run on the same commit do not
    # incorrectly appear as failures (workflows with cancel-in-progress:true
    # produce a cancelled run followed by a fresh successful run).
    try:
        latest_runs = {}
        for run in commit.get_check_runs():
            if run.name not in latest_runs or run.id > latest_runs[run.name].id:
                latest_runs[run.name] = run
        for run in latest_runs.values():
            if run.status == "completed":
                if run.conclusion in (
                    "failure", "action_required", "timed_out", "cancelled"
                ):
                    failing.append(run.name)
                    states.append("failure")
                else:
                    states.append("success")
            elif run.status in ("in_progress", "queued"):
                states.append("pending")
    except Exception:
        pass

    # Deduplicate while preserving first-seen order
    seen = set()
    unique_failing = []
    for name in failing:
        if name not in seen:
            seen.add(name)
            unique_failing.append(name)

    if not states:
        return "unknown", unique_failing
    if any(s in ("failure", "error") for s in states):
        return "fail", unique_failing
    if any(s == "pending" for s in states):
        return "pending", unique_failing
    return "pass", unique_failing


def _is_meta_area(name):
    """Return True when *name* matches a meta area that should not count
    toward the 'many areas' code-complexity threshold."""
    low = name.lower()
    return any(sub in low for sub in _META_AREA_SUBSTRINGS)


def _maintainers_for_pr(maint_obj, pr_files):
    """
    Return (areas, all_maintainers_set) for the given list of changed file paths.
    areas is a list of Area objects.
    """
    if not _HAS_MAINTAINERS or maint_obj is None:
        return [], set()

    seen_areas = {}
    all_maintainers = set()
    for f in pr_files:
        try:
            file_areas = maint_obj.path2areas(f)
        except Exception:
            continue
        for area in file_areas:
            seen_areas[area.name] = area
            all_maintainers.update(area.maintainers)

    return list(seen_areas.values()), all_maintainers


def _analyze_pr(pr, maint_obj, verbose=False):
    """
    Collect all signals for a single PR.
    Returns a dict with all findings and a list of category keys.
    """
    if verbose:
        draft_tag = " [DRAFT]" if pr.draft else ""
        print(f"  Analyzing PR #{pr.number}{draft_tag}: {pr.title[:60]}", flush=True)

    # ---- Basic metadata ----
    now = datetime.datetime.now(datetime.timezone.utc)
    is_draft = bool(pr.draft)
    created_at = pr.created_at.replace(tzinfo=datetime.timezone.utc) \
        if pr.created_at.tzinfo is None else pr.created_at
    updated_at = pr.updated_at.replace(tzinfo=datetime.timezone.utc) \
        if pr.updated_at.tzinfo is None else pr.updated_at

    age_days = (now - created_at).days

    # ---- Size ----
    total_lines = pr.additions + pr.deletions
    num_files = pr.changed_files
    num_commits = pr.commits

    # ---- Labels ----
    label_names = [lbl.name for lbl in pr.labels]
    has_dnm = any(lbl.upper() == "DNM" for lbl in label_names)
    has_arch_review = any(
        lbl in ("Architecture Review", "TSC") for lbl in label_names
    )

    # ---- Mergeability ----
    try:
        mergeable_state = pr.mergeable_state or "unknown"
    except Exception:
        mergeable_state = "unknown"
    needs_rebase = mergeable_state == "dirty"

    # ---- Assignees / reviewers ----
    assignees = [a.login for a in pr.assignees]
    try:
        requested_reviewers = [r.login for r in pr.get_review_requests()[0]]
    except Exception:
        requested_reviewers = []

    # ---- Reviews ----
    fork_deleted = False
    try:
        all_reviews = list(pr.get_reviews())
    except GithubException as exc:
        if exc.status == 404:
            fork_deleted = True
            if verbose:
                print(
                    f"    Source fork deleted for PR #{pr.number}; "
                    "review/commit data unavailable.",
                    flush=True,
                )
        all_reviews = []
    except Exception:
        all_reviews = []
    latest = _latest_reviews(all_reviews)
    approved_by = [u for u, s in latest.items() if s == "APPROVED"]
    changes_by = [u for u, s in latest.items() if s == "CHANGES_REQUESTED"]
    num_approvals = len(approved_by)
    num_changes_requested = len(changes_by)

    # ---- CI ----
    ci, ci_failing_checks = _ci_details(pr)

    # ---- Comments / discussion ----
    try:
        issue_comments_pg = pr.get_issue_comments()
        issue_comments_list = list(issue_comments_pg)
    except Exception:
        issue_comments_list = []
    try:
        review_comments_pg = pr.get_review_comments()
        review_comments_list = list(review_comments_pg)
    except Exception:
        review_comments_list = []
    total_comments = len(issue_comments_list) + len(review_comments_list)

    last_comment_dt = None
    for c in issue_comments_list:
        dt = c.created_at.replace(tzinfo=datetime.timezone.utc) \
            if c.created_at.tzinfo is None else c.created_at
        if last_comment_dt is None or dt > last_comment_dt:
            last_comment_dt = dt
    for c in review_comments_list:
        dt = c.created_at.replace(tzinfo=datetime.timezone.utc) \
            if c.created_at.tzinfo is None else c.created_at
        if last_comment_dt is None or dt > last_comment_dt:
            last_comment_dt = dt

    comment_idle_days = (now - last_comment_dt).days if last_comment_dt else None

    # ---- Last meaningful activity ----
    # "Meaningful" = a commit pushed to the branch, a review submitted, or
    # a human comment.  Deliberately excludes label/assignee changes, CI
    # reruns, and any other metadata-only touches that bump updated_at.
    last_commit = _last_commit_dt(pr)
    meaningful_candidates = [created_at]
    if last_commit:
        meaningful_candidates.append(last_commit)
    if last_comment_dt:
        meaningful_candidates.append(last_comment_dt)
    for rev in all_reviews:
        if rev.submitted_at:
            rdt = rev.submitted_at
            if rdt.tzinfo is None:
                rdt = rdt.replace(tzinfo=datetime.timezone.utc)
            meaningful_candidates.append(rdt)
    last_meaningful_dt = max(meaningful_candidates)
    meaningful_idle_days = (now - last_meaningful_dt).days

    # ---- MAINTAINERS.yml areas ----
    try:
        pr_file_paths = [f.filename for f in pr.get_files()]
    except Exception:
        pr_file_paths = []

    areas, all_area_maintainers = _maintainers_for_pr(maint_obj, pr_file_paths)
    area_names = [a.name for a in areas]
    num_areas = len(areas)
    non_meta_areas = [a for a in area_names if not _is_meta_area(a)]
    num_non_meta_areas = len(non_meta_areas)

    submitter_is_maintainer = pr.user.login in all_area_maintainers \
        if pr.user else False

    # ---- Assignee approval check ----
    assignee_approved = any(a in approved_by for a in assignees) if assignees else False

    # ---- 2-approval rule ----
    two_approvals_met = num_approvals >= 2 and (not assignees or assignee_approved)

    # ---- Categorize ----
    categories = []

    # These categories apply regardless of approval state
    if has_dnm:
        categories.append(CAT_DNM)
    if needs_rebase:
        categories.append(CAT_NEEDS_REBASE)
    if has_arch_review:
        categories.append(CAT_ARCH_REVIEW)

    if two_approvals_met:
        categories.append(CAT_NEARLY_APPROVED)
    else:
        if not requested_reviewers and not all_reviews:
            categories.append(CAT_NO_REVIEWER)
        if not all_reviews:
            categories.append(CAT_NO_REVIEW)
        if not assignees:
            categories.append(CAT_NO_ASSIGNEE)
        elif not assignee_approved:
            categories.append(CAT_MISSING_ASSIGNEE_APPROVAL)

        if ci == "fail":
            categories.append(CAT_CI_FAILING)
        elif ci in ("pending", "unknown"):
            categories.append(CAT_CI_PENDING)

        if changes_by:
            categories.append(CAT_CHANGES_REQUESTED)
        elif num_approvals == 1:
            categories.append(CAT_AWAITING_SECOND_REVIEW)

        if num_non_meta_areas >= SIZE_MANY_AREAS:
            categories.append(CAT_MANY_AREAS)

        if submitter_is_maintainer and pr.user and pr.user.login in assignees:
            categories.append(CAT_MAINTAINER_SUBMITTED)

        if total_lines > SIZE_LARGE_LINES:
            categories.append(CAT_LARGE_PR)

        if total_comments > 0:
            if comment_idle_days is not None and comment_idle_days > 14:
                categories.append(CAT_DISCUSSION_STALE)
            elif total_comments > 0:
                categories.append(CAT_DISCUSSION_ACTIVE)
    # ---- Collect area maintainer details ----
    area_details = []
    for area in areas:
        area_details.append({
            "name": area.name,
            "maintainers": area.maintainers,
            "status": area.status,
        })

    return {
        "number": pr.number,
        "title": pr.title,
        "url": pr.html_url,
        "author": pr.user.login if pr.user else "unknown",
        "created_at": created_at.strftime("%Y-%m-%d"),
        "updated_at": updated_at.strftime("%Y-%m-%d"),
        "age_days": age_days,
        "meaningful_idle_days": meaningful_idle_days,
        "last_meaningful_dt": last_meaningful_dt.strftime("%Y-%m-%d"),
        "additions": pr.additions,
        "deletions": pr.deletions,
        "total_lines": total_lines,
        "num_files": num_files,
        "num_commits": num_commits,
        "labels": label_names,
        "assignees": assignees,
        "requested_reviewers": requested_reviewers,
        "approved_by": approved_by,
        "changes_requested_by": changes_by,
        "num_approvals": num_approvals,
        "num_changes_requested": num_changes_requested,
        "assignee_approved": assignee_approved,
        "two_approvals_met": two_approvals_met,
        "draft": is_draft,
        "fork_deleted": fork_deleted,
        "ci": ci,
        "ci_failing_checks": ci_failing_checks,
        "total_comments": total_comments,
        "comment_idle_days": comment_idle_days,
        "areas": area_names,
        "num_areas": num_areas,
        "non_meta_areas": non_meta_areas,
        "num_non_meta_areas": num_non_meta_areas,
        "area_details": area_details,
        "all_area_maintainers": sorted(all_area_maintainers),
        "submitter_is_maintainer": submitter_is_maintainer,
        "mergeable_state": mergeable_state,
        "needs_rebase": needs_rebase,
        "has_dnm": has_dnm,
        "has_arch_review": has_arch_review,
        "categories": categories,
    }


# ---------------------------------------------------------------------------
# HTML report
# ---------------------------------------------------------------------------

_HTML_TEMPLATE = """\
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Zephyr PR Backlog Analysis</title>
<style>
  :root {{
    --bg: #f4f6f9;
    --card: #ffffff;
    --border: #dde1e7;
    --text: #2c3e50;
    --muted: #7f8c8d;
    --link: #2980b9;
    --header-bg: #2c3e50;
    --header-fg: #ecf0f1;
  }}
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto,
    sans-serif; background: var(--bg); color: var(--text); font-size: 14px; }}
  header {{ background: var(--header-bg); color: var(--header-fg);
    padding: 18px 32px; }}
  header h1 {{ font-size: 1.5rem; font-weight: 600; }}
  header p {{ margin-top: 4px; opacity: 0.7; font-size: 0.85rem; }}
  main {{ max-width: 1400px; margin: 0 auto; padding: 24px 24px 64px; }}

  /* ---- summary cards ---- */
  .summary-grid {{ display: grid;
    grid-template-columns: repeat(auto-fill, minmax(170px, 1fr));
    gap: 14px; margin-bottom: 28px; }}
  .card {{ background: var(--card); border: 1px solid var(--border);
    border-radius: 8px; padding: 16px; text-align: center; }}
  .card .num {{ font-size: 2rem; font-weight: 700; line-height: 1; }}
  .card .lbl {{ margin-top: 6px; color: var(--muted); font-size: 0.78rem;
    text-transform: uppercase; letter-spacing: .04em; }}
  .card .delta {{ display:block; margin-top:4px; font-size:0.72rem;
    font-weight:600; }}
  .delta-good  {{ color: #27ae60; }}
  .delta-bad   {{ color: #e74c3c; }}
  .delta-neutral {{ color: var(--muted); }}

  /* ---- trend history table ---- */
  .trend-tbl {{ width:100%; border-collapse:collapse; font-size:0.82rem; }}
  .trend-tbl th {{ background:var(--header-bg); color:var(--header-fg);
    padding:6px 10px; text-align:right; white-space:nowrap; }}
  .trend-tbl th:first-child {{ text-align:left; }}
  .trend-tbl td {{ padding:5px 10px; border-bottom:1px solid var(--border);
    text-align:right; white-space:nowrap; }}
  .trend-tbl td:first-child {{ text-align:left; }}
  .trend-tbl tr:nth-child(even) td {{ background:#f9fafb; }}
  .trend-tbl .delta {{ display:inline; font-size:0.70rem; margin-left:3px; }}

  /* ---- category breakdown ---- */
  .section-title {{ font-size: 1.1rem; font-weight: 600; margin: 28px 0 12px;
    border-bottom: 2px solid var(--border); padding-bottom: 6px; }}
  .cat-grid {{ display: grid;
    grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));
    gap: 12px; margin-bottom: 28px; }}
  .cat-card {{ background: var(--card); border-left: 5px solid #ccc;
    border-radius: 6px; padding: 12px 14px; }}
  .cat-card .cat-label {{ font-weight: 600; font-size: 0.9rem; }}
  .cat-card .cat-count {{ font-size: 1.5rem; font-weight: 700; float: right;
    margin-top: -2px; }}
  .cat-card .cat-desc {{ margin-top: 6px; font-size: 0.78rem;
    color: var(--muted); clear: both; }}

  /* ---- pattern chart ---- */
  .bar-chart {{ background: var(--card); border: 1px solid var(--border);
    border-radius: 8px; padding: 18px 20px; margin-bottom: 28px; }}
  .bar-row {{ display: flex; align-items: center; margin-bottom: 8px;
    gap: 10px; }}
  .bar-name {{ width: 260px; white-space: nowrap; overflow: hidden;
    text-overflow: ellipsis; font-size: 0.82rem; flex-shrink: 0; }}
  .bar-outer {{ flex: 1; background: #ecf0f1; border-radius: 4px;
    height: 18px; overflow: hidden; }}
  .bar-inner {{ height: 100%; border-radius: 4px; }}
  .bar-val {{ width: 40px; text-align: right; font-size: 0.82rem;
    font-weight: 600; color: var(--muted); flex-shrink: 0; }}

  /* ---- PR table ---- */
  .filters {{ display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 14px;
    align-items: center; }}
  .filters input {{ border: 1px solid var(--border); border-radius: 4px;
    padding: 5px 10px; font-size: 0.82rem; width: 220px; }}
  .filters select {{ border: 1px solid var(--border); border-radius: 4px;
    padding: 5px 8px; font-size: 0.82rem; }}
  .filters label {{ font-size: 0.82rem; color: var(--muted); }}
  table {{ width: 100%; border-collapse: collapse;
    background: var(--card); border-radius: 8px; overflow: hidden;
    border: 1px solid var(--border); font-size: 0.82rem; }}
  thead {{ background: var(--header-bg); color: var(--header-fg); }}
  thead th {{ padding: 10px 12px; text-align: left; font-weight: 500;
    font-size: 0.78rem; text-transform: uppercase; letter-spacing: .04em;
    cursor: pointer; white-space: nowrap; user-select: none; }}
  thead th:hover {{ background: #3d5068; }}
  thead th.sort-asc::after {{ content: " ▲"; }}
  thead th.sort-desc::after {{ content: " ▼"; }}
  tbody tr {{ border-top: 1px solid var(--border); }}
  tbody tr:hover {{ background: #f8f9fc; }}
  td {{ padding: 9px 12px; vertical-align: top; }}
  td.num {{ text-align: right; font-variant-numeric: tabular-nums; }}
  a {{ color: var(--link); text-decoration: none; }}
  a:hover {{ text-decoration: underline; }}

  /* ---- badges ---- */
  .badge {{ display: inline-block; border-radius: 3px; padding: 1px 6px;
    font-size: 0.72rem; font-weight: 600; color: #fff;
    white-space: nowrap; margin: 1px 1px 1px 0; }}
  .ci-pass {{ background: #27ae60; }}
  .ci-fail {{ background: #c0392b; }}
  .ci-pending {{ background: #f39c12; }}
  .ci-unknown {{ background: #95a5a6; }}
  .draft-badge {{ background: #7f8c8d; letter-spacing: .05em; }}
  .fork-del-badge {{ background: #c0392b; letter-spacing: .03em; }}
  .age-old {{ color: #c0392b; font-weight: 700; }}
  .age-mid {{ color: #e67e22; font-weight: 600; }}

  /* ---- tooltip / detail expand ---- */
  details summary {{ cursor: pointer; color: var(--link); font-size: 0.78rem; }}
  details[open] summary {{ margin-bottom: 4px; }}
  .area-list {{ font-size: 0.72rem; color: var(--muted); margin-top: 3px; }}
  .area-chip {{
    display: inline-block;
    border-radius: 3px;
    padding: 1px 5px;
    font-size: 0.70rem;
    font-weight: 500;
    margin: 1px 2px 1px 0;
    white-space: nowrap;
    color: #fff;
  }}

  /* ---- co-occurrence heatmap ---- */
  .heatmap-wrapper {{ overflow-x: auto; margin-bottom: 28px; }}
  .heatmap-wrapper table {{ font-size: 0.7rem; min-width: 600px; }}
  .heatmap-wrapper thead th {{ font-size: 0.68rem; writing-mode: vertical-rl;
    min-width: 24px; max-width: 24px; height: 110px; padding: 4px 2px;
    overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }}
  .heatmap-wrapper tbody td {{ width: 24px; height: 24px; text-align: center;
    padding: 0; font-size: 0.68rem; }}
  .hm0 {{ background: #f7f7f7; }}
  .hm1 {{ background: #c6e0f5; }}
  .hm2 {{ background: #7ab8e8; }}
  .hm3 {{ background: #2e86de; color:#fff; }}
  .hm4 {{ background: #1a5276; color:#fff; }}

  /* ---- two-row PR table ---- */
  .pr-title-row td {{
    padding: 10px 12px 2px;
    border-top: 2px solid var(--border);
  }}
  .pr-meta-row td {{
    padding: 2px 12px 8px;
    border-top: none;
    background: #fafbfc;
  }}
  .pr-title-cell {{
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    max-width: 0;   /* makes overflow:hidden + text-overflow work in td */
  }}
  .pr-num-link {{
    font-weight: 700;
    color: var(--muted);
    margin-right: 10px;
    font-size: 0.82rem;
  }}
  .pr-title-link {{
    font-weight: 500;
    margin-right: 12px;
  }}
  .pr-by {{
    font-size: 0.78rem;
    color: var(--muted);
  }}
  /* ---- CI details ---- */
  .ci-checks-list {{
    font-size: 0.7rem;
    color: var(--muted);
    cursor: pointer;
    margin-top: 2px;
  }}
  /* ---- simple (non-interactive) tables ---- */
  .simple-table {{
    width: 100%;
    border-collapse: collapse;
    background: var(--card);
    border-radius: 8px;
    overflow: hidden;
    border: 1px solid var(--border);
    font-size: 0.82rem;
    margin-bottom: 28px;
  }}
  .simple-table thead {{
    background: var(--header-bg);
    color: var(--header-fg);
  }}
  .simple-table thead th {{
    padding: 10px 12px;
    text-align: left;
    font-weight: 500;
    font-size: 0.78rem;
    text-transform: uppercase;
    letter-spacing: .04em;
    white-space: nowrap;
  }}
  .simple-table tbody tr {{
    border-top: 1px solid var(--border);
  }}
  .simple-table tbody tr:hover {{
    background: #f8f9fc;
  }}
  .simple-table td {{
    padding: 8px 12px;
    vertical-align: top;
  }}
  .simple-table td.num {{
    text-align: right;
    font-variant-numeric: tabular-nums;
    white-space: nowrap;
  }}
  .pr-link-list a {{
    display: inline-block;
    margin: 1px 3px 1px 0;
  }}
  /* ---- scrollable PR table wrapper ---- */
  .table-scroll {{
    overflow-x: auto;
    margin-bottom: 28px;
  }}
  /* ---- sticky table header ---- */
  thead {{ position: sticky; top: 0; z-index: 2; }}

  /* ---- responsive ---- */
  @media(max-width:800px) {{
    .bar-name {{ width: 140px; }}
    .summary-grid {{ grid-template-columns: repeat(2,1fr); }}
  }}
</style>
</head>
<body>
<header>
  <h1>Zephyr PR Backlog Analysis</h1>
  <p>Repository: <strong>{org}/{repo}</strong> &nbsp;|&nbsp;
     Age threshold: <strong>{age_days} days</strong> &nbsp;|&nbsp;
     Generated: <strong>{generated}</strong> &nbsp;|&nbsp;
     PRs analysed: <strong>{total_prs}</strong></p>
</header>
<main>

<!-- ======================== SUMMARY CARDS ======================== -->
<div class="section-title">Overview</div>
<div class="summary-grid">
  {summary_cards}
</div>

<!-- ======================== CATEGORY BREAKDOWN ======================== -->
<div class="section-title">PR Backlog Categories</div>
<div class="cat-grid">
  {cat_cards}
</div>

<!-- ======================== PATTERN BAR CHART ======================== -->
<div class="section-title">Category Distribution</div>
<div class="bar-chart">
  {bar_chart}
</div>

<!-- ======================== CO-OCCURRENCE HEATMAP ======================== -->
<div class="section-title">Category Co-occurrence Heatmap</div>
<p style="font-size:.8rem;color:var(--muted);margin-bottom:8px;">
  Each cell shows how many PRs share both categories (row × column).
  Darker = more overlap.</p>
<div class="heatmap-wrapper">
  {heatmap}
</div>

<!-- ======================== PR TABLE ======================== -->
<div class="section-title">PR Detail Table</div>
<div class="filters">
  <label>Filter:&nbsp;</label>
  <input id="filter-text" type="search" placeholder="PR #, title, author…"
    oninput="applyFilters()">
  <select id="filter-cat" onchange="applyFilters()">
    <option value="">All categories</option>
    {cat_options}
  </select>
  <select id="filter-ci" onchange="applyFilters()">
    <option value="">All CI states</option>
    <option>pass</option><option>fail</option>
    <option>pending</option><option>unknown</option>
  </select>
  <select id="filter-areas" onchange="applyFilters()">
    <option value="">Any # code areas</option>
    <option value="1">1 code area</option>
    <option value="2">2 code areas</option>
    <option value="3">3 code areas</option>
    <option value="4+">4+ code areas</option>
  </select>
  <select id="filter-draft" onchange="applyFilters()">
    <option value="">All PRs (incl. drafts)</option>
    <option value="no">Hide drafts</option>
    <option value="only">Drafts only</option>
  </select>
  <span id="row-count" style="font-size:.8rem;color:var(--muted);
    margin-left:8px;"></span>
</div>
<div class="table-scroll">
<table id="pr-table">
  <thead>
    <tr>
      <th data-col="0" title="Days since created">Age</th>
      <th data-col="1" title="Days since last commit, review, or comment (excludes label/CI/bot updates)">Meaningful Idle</th>
      <th data-col="2">Size (±lines)</th>
      <th data-col="3">Files</th>
      <th data-col="4">Areas</th>
      <th data-col="5">CI</th>
      <th data-col="6">Approvals</th>
      <th data-col="7" title="Number of reviewers with unresolved change requests">Chg Req</th>
      <th data-col="8">Assignee</th>
      <th data-col="9">Comments</th>
      <th data-col="10">Categories</th>
    </tr>
  </thead>
  <tbody id="pr-tbody">
    {pr_rows}
  </tbody>
</table>
</div>

<!-- ======================== BY ASSIGNEE ======================== -->
<div class="section-title">PRs by Assignee</div>
<p style="font-size:.8rem;color:var(--muted);margin-bottom:8px;">
  Sorted by total PRs in the backlog.  “(unassigned)” rows are PRs with no
  assignee set.</p>
{assignee_table}

<!-- ======================== TOP CHANGE REQUESTERS ======================== -->
<div class="section-title">Top Reviewers Requesting Changes</div>
<p style="font-size:.8rem;color:var(--muted);margin-bottom:8px;">
  Reviewers who have the most unresolved change requests across backlog PRs,
  sorted by number of PRs blocked.
  &ldquo;Sole blocker&rdquo; means no other reviewer has approved that PR yet.</p>
{change_requesters_table}

<!-- ======================== CI WORKFLOW FAILURES ======================== -->
<div class="section-title">Most-Failing CI Workflows</div>
<p style="font-size:.8rem;color:var(--muted);margin-bottom:8px;">
  Workflows and check contexts that are failing on the most delayed PRs.
  Only PRs where CI details are accessible via the API are included.</p>
{ci_workflow_table}

<!-- ======================== AREAS MOST INVOLVED ======================== -->
<div class="section-title">Top Code Areas by PR Involvement</div>
<p style="font-size:.8rem;color:var(--muted);margin-bottom:8px;">
  Meta areas (Tests, Samples, Boards/SoCs, Release, Documentation,
  MAINTAINERS entries) are excluded from this chart and from the
  &ldquo;Spans many code areas&rdquo; category count.</p>
<div class="bar-chart">
  {area_chart}
</div>

<!-- ======================== TOP AUTHORS ======================== -->
<div class="section-title">Authors with Most PRs in the Backlog</div>
<div class="bar-chart">
  {author_chart}
</div>

<!-- ======================== TREND HISTORY ======================== -->
{trend_section}

</main>
<script>
/* ---- raw data for client-side filtering ---- */
const PR_DATA = {pr_json};

/* ---- sorting ---- */
let sortCol = 3;   // age
let sortDir = -1;  // descending

function sortTable(col) {{
  if (sortCol === col) {{ sortDir *= -1; }} else {{ sortCol = col; sortDir = -1; }}
  document.querySelectorAll("thead th").forEach((th, i) => {{
    th.classList.remove("sort-asc","sort-desc");
    if (i === col) th.classList.add(sortDir > 0 ? "sort-asc" : "sort-desc");
  }});
  applyFilters();
}}

document.querySelectorAll("thead th[data-col]").forEach(th => {{
  th.addEventListener("click", () => sortTable(+th.dataset.col));
}});

/* ---- filtering + rendering ---- */
function applyFilters() {{
  const txt = document.getElementById("filter-text").value.toLowerCase();
  const cat = document.getElementById("filter-cat").value;
  const ci  = document.getElementById("filter-ci").value;
  const areas = document.getElementById("filter-areas").value;
  const draft = document.getElementById("filter-draft").value;

  let rows = PR_DATA.filter(p => {{
    if (txt && !(
      String(p.number).includes(txt) ||
      p.title.toLowerCase().includes(txt) ||
      p.author.toLowerCase().includes(txt)
    )) return false;
    if (cat && !p.categories.includes(cat)) return false;
    if (ci  && p.ci !== ci) return false;
    if (areas === "4+" && p.num_non_meta_areas < 4) return false;
    if (areas && areas !== "4+" && p.num_non_meta_areas !== +areas) return false;
    if (draft === "no" && p.draft) return false;
    if (draft === "only" && !p.draft) return false;
    return true;
  }});

  /* sort */
  const keys = ["age_days","meaningful_idle_days","total_lines","num_files",
    "num_areas","ci","num_approvals","num_changes_requested","assignees","total_comments","categories"];
  rows.sort((a,b) => {{
    let av = a[keys[sortCol]], bv = b[keys[sortCol]];
    if (Array.isArray(av)) av = av.length;
    if (Array.isArray(bv)) bv = bv.length;
    if (typeof av === "string") return sortDir * av.localeCompare(bv);
    return sortDir * ((av||0) - (bv||0));
  }});

  document.getElementById("row-count").textContent = rows.length + " PRs";
  document.getElementById("pr-tbody").innerHTML = rows.map(rowHtml).join("");
}}

function ciClass(ci) {{
  return {{pass:"ci-pass",fail:"ci-fail",pending:"ci-pending",
    unknown:"ci-unknown"}}[ci] || "ci-unknown";
}}

function ageClass(d) {{
  return d > 60 ? "age-old" : d > 30 ? "age-mid" : "";
}}

const CAT_COLORS = {cat_colors_json};
const CAT_LABELS = {cat_labels_json};

/* Derive a stable hue from an arbitrary string (djb2-style hash). */
function strHue(s) {{
  let h = 0;
  for (let i = 0; i < s.length; i++) h = (Math.imul(31, h) + s.charCodeAt(i)) | 0;
  return ((h >>> 0) % 360);
}}
function areaColor(name) {{
  const hue = strHue(name);
  return `hsl(${{hue}},42%,38%)`;
}}
function areaChip(name) {{
  return `<span class="area-chip" style="background:${{areaColor(name)}}" title="${{escHtml(name)}}">${{escHtml(name)}}</span>`;
}}

function rowHtml(p) {{
  const cats = p.categories.map(c =>
    `<span class="badge" style="background:${{CAT_COLORS[c]||'#95a5a6'}}">${{
      CAT_LABELS[c]||c}}</span>`).join(" ");
  const assignee = p.assignees.length
    ? p.assignees.join(", ")
    : '<span style="color:#c0392b">none</span>';
  const areas = p.areas.length
    ? `<details><summary>${{p.num_areas}} area(s)</summary>
       <div class="area-list">${{p.areas.map(areaChip).join("")}}</div></details>`
    : '<span style="color:var(--muted)">—</span>';
  const ageC = ageClass(p.age_days);
  const idleC = ageClass(p.meaningful_idle_days);
  const ciCls = ciClass(p.ci);
  const ciBadge = `<span class="badge ${{ciCls}}">${{p.ci}}</span>`;
  const ciCell = (p.ci_failing_checks && p.ci_failing_checks.length)
    ? ciBadge +
      `<details><summary class="ci-checks-list">${{p.ci_failing_checks.length}} failing</summary>` +
      `<div class="area-list">${{p.ci_failing_checks.map(escHtml).join("<br>")}}</div></details>`
    : ciBadge;
  const da = `data-cats="${{p.categories.join(" ")}}" data-ci="${{p.ci}}" data-areas="${{p.num_areas}}" data-draft="${{p.draft}}"`;
  return (
    `<tr class="pr-title-row" ${{da}}>` +
    `<td class="pr-title-cell" colspan="11">` +
    `<a class="pr-num-link" href="${{p.url}}" target="_blank">#${{p.number}}</a>` +
    (p.draft ? `<span class="badge draft-badge">DRAFT</span>` : "") +
    (p.fork_deleted ? `<span class="badge fork-del-badge" title="Source fork deleted; review/commit data unavailable">⚠ fork deleted</span>` : "") +
    `<a class="pr-title-link" href="${{p.url}}" target="_blank">${{escHtml(p.title)}}</a>` +
    `<span class="pr-by">by ${{escHtml(p.author)}}</span>` +
    `</td></tr>` +
    `<tr class="pr-meta-row" ${{da}}>` +
    `<td class="num ${{ageC}}">${{p.age_days}}d</td>` +
    `<td class="num ${{idleC}}" title="Last meaningful activity: ${{p.last_meaningful_dt}}">${{p.meaningful_idle_days}}d</td>` +
    `<td class="num">+${{p.additions}}/−${{p.deletions}}</td>` +
    `<td class="num">${{p.num_files}}</td>` +
    `<td>${{areas}}</td>` +
    `<td>${{ciCell}}</td>` +
    `<td class="num">${{p.num_approvals}}</td>` +
    `<td class="num">${{p.num_changes_requested || 0}}</td>` +
    `<td>${{assignee}}</td>` +
    `<td class="num">${{p.total_comments}}</td>` +
    `<td style="min-width:200px">${{cats}}</td>` +
    `</tr>`
  );
}}

function escHtml(s) {{
  return s.replace(/&/g,"&amp;").replace(/</g,"&lt;")
          .replace(/>/g,"&gt;").replace(/"/g,"&quot;");
}}

/* initialise */
applyFilters();

/* default sort indicator */
document.querySelectorAll("thead th[data-col]").forEach((th, i) => {{
  if (i === 0) th.classList.add("sort-desc");
}});
</script>
</body>
</html>
"""


def _summary_card(num, label, color="var(--text)", delta_html=""):
    return (
        f'<div class="card">'
        f'<div class="num" style="color:{color}">{num}</div>'
        f'<div class="lbl">{html.escape(label)}</div>'
        f'{delta_html}'
        f'</div>'
    )


def _delta_html(current, prev, lower_is_better=True):
    """Return an HTML <span> showing the change from *prev* to *current*.

    Returns an empty string when *prev* is None (no previous run).
    """
    if prev is None:
        return ""
    try:
        diff = current - prev
    except TypeError:
        return ""
    if diff == 0:
        return '<span class="delta delta-neutral">—</span>'
    improving = (diff < 0) == lower_is_better
    cls = "delta-good" if improving else "delta-bad"
    arrow = "▼" if diff < 0 else "▲"
    if isinstance(current, float):
        label = f"{diff:+.1f}"
    else:
        label = f"{diff:+d}"
    return f'<span class="delta {cls}">{arrow} {label}</span>'


def _trend_table_html(history):
    """Render an HTML table summarising all historical run snapshots.

    *history* is a list of snapshot dicts, oldest first.  The table is
    displayed newest-first with per-cell deltas vs. the previous run.
    Returns an empty-state paragraph when there is only one entry (nothing
    to compare yet) or no data.
    """
    if len(history) < 2:
        return (
            '<p style="color:var(--muted);font-size:.85rem;margin-top:8px;">'
            'Trend data will appear here after two or more runs.</p>'
        )

    cols = [
        ("Date",          "generated",        None),
        ("Total PRs",     "total",             True),
        ("Avg Age (d)",   "avg_age",           True),
        ("Avg Idle (d)",  "avg_idle",          True),
        ("CI Failing",    "ci_fail",           True),
        ("Changes Req",   "changes_req",       True),
        ("Needs Rebase",  "num_needs_rebase",  True),
        ("DNM",           "num_dnm",           True),
        ("Arch Review",   "num_arch_review",   True),
        ("Nearly Done",   "nearly_done",       False),
        ("No Assignee",   "no_assignee",       True),
    ]

    header = "".join(f"<th>{c[0]}</th>" for c in cols)

    rows = []
    for i in range(len(history) - 1, -1, -1):
        snap = history[i]
        prev = history[i - 1] if i > 0 else None
        cells = []
        for (_, key, lower_is_better) in cols:
            val = snap.get(key, "")
            if lower_is_better is None:
                cells.append(f"<td>{html.escape(str(val))}</td>")
            else:
                dh = _delta_html(val, prev.get(key) if prev else None,
                                 lower_is_better=lower_is_better)
                if isinstance(val, float):
                    cells.append(f"<td>{val:.1f}{dh}</td>")
                else:
                    cells.append(f"<td>{val}{dh}</td>")
        rows.append(f"<tr>{''.join(cells)}</tr>")

    return (
        '<div style="overflow-x:auto">'
        f'<table class="trend-tbl">'
        f'<thead><tr>{header}</tr></thead>'
        f'<tbody>{"".join(rows)}</tbody>'
        '</table></div>'
    )


def _bar_chart_rows(counts, max_val, color_fn=None):
    rows = []
    for name, val in counts:
        pct = (val / max_val * 100) if max_val else 0
        color = color_fn(name) if color_fn else "#3498db"
        rows.append(
            f'<div class="bar-row">'
            f'<div class="bar-name" title="{html.escape(name)}">'
            f'{html.escape(name)}</div>'
            f'<div class="bar-outer">'
            f'<div class="bar-inner" style="width:{pct:.1f}%;'
            f'background:{color}"></div>'
            f'</div>'
            f'<div class="bar-val">{val}</div>'
            f'</div>'
        )
    return "\n".join(rows)


def _heatmap_html(pr_data_list):
    """Build a category co-occurrence heatmap table."""
    cat_keys = list(CATEGORY_META.keys())
    co = collections.Counter()
    for pr in pr_data_list:
        cats = pr["categories"]
        for i, c1 in enumerate(cat_keys):
            for c2 in cat_keys[i:]:
                if c1 in cats and c2 in cats:
                    co[(c1, c2)] += 1
                    if c1 != c2:
                        co[(c2, c1)] += 1

    max_val = max(co.values()) if co else 1

    short = {k: v["label"][:20] for k, v in CATEGORY_META.items()}

    def hm_class(v):
        if v == 0:
            return "hm0"
        if v <= max_val * 0.25:
            return "hm1"
        if v <= max_val * 0.5:
            return "hm2"
        if v <= max_val * 0.75:
            return "hm3"
        return "hm4"

    lines = ['<table>',
             '<thead><tr><th></th>']
    for k in cat_keys:
        lines.append(
            f'<th title="{html.escape(CATEGORY_META[k]["label"])}">'
            f'{html.escape(short[k])}</th>'
        )
    lines.append('</tr></thead><tbody>')
    for r in cat_keys:
        lines.append(
            f'<tr><td style="white-space:nowrap;font-size:.72rem;'
            f'padding-right:6px">{html.escape(short[r])}</td>'
        )
        for c in cat_keys:
            v = co.get((r, c), 0)
            title = f'{CATEGORY_META[r]["label"]} ∩ {CATEGORY_META[c]["label"]}: {v}'
            lines.append(
                f'<td class="{hm_class(v)}" title="{html.escape(title)}">'
                f'{"" if v == 0 else v}</td>'
            )
        lines.append('</tr>')
    lines.append('</tbody></table>')
    return "\n".join(lines)


def _area_color(name):
    """Derive a stable HSL background color from an area name (djb2 hash)."""
    h = 0
    for ch in name:
        h = (31 * h + ord(ch)) & 0xFFFFFFFF
    hue = h % 360
    return f"hsl({hue},42%,38%)"


def _area_chip_html(name):
    escaped = html.escape(name)
    color = _area_color(name)
    return (
        f'<span class="area-chip" style="background:{color}" title="{escaped}">'
        f'{escaped}</span>'
    )


def _ci_cell_html(ci, ci_failing_checks, ci_cls):
    """Build the HTML for the CI status cell."""
    badge = f'<span class="badge {ci_cls}">{ci}</span>'
    if not ci_failing_checks:
        return badge
    items = "<br>".join(html.escape(c) for c in ci_failing_checks)
    return (
        f'{badge}'
        f'<details><summary class="ci-checks-list">'
        f'{len(ci_failing_checks)} failing</summary>'
        f'<div class="area-list">{items}</div></details>'
    )


def _pr_row_html(pr):
    """Render a PR as two <tr> elements: title row + metric row."""
    cats_html = " ".join(
        f'<span class="badge" style="background:'
        f'{CATEGORY_META[c]["color"]}">'
        f'{html.escape(CATEGORY_META[c]["label"])}</span>'
        for c in pr["categories"]
    )
    if pr["assignees"]:
        assignee_html = html.escape(", ".join(pr["assignees"]))
    else:
        assignee_html = '<span style="color:#c0392b">none</span>'

    if pr["areas"]:
        area_html = (
            f'<details><summary>{pr["num_areas"]} area(s)</summary>'
            f'<div class="area-list">'
            + "".join(_area_chip_html(a) for a in pr["areas"])
            + "</div></details>"
        )
    else:
        area_html = '<span style="color:var(--muted)">—</span>'

    ci_cls = {"pass": "ci-pass", "fail": "ci-fail",
              "pending": "ci-pending", "unknown": "ci-unknown"}.get(pr["ci"], "ci-unknown")
    ci_html = _ci_cell_html(pr["ci"], pr["ci_failing_checks"], ci_cls)

    age_cls = "age-old" if pr["age_days"] > 60 else \
              "age-mid" if pr["age_days"] > 30 else ""
    idle_cls = "age-old" if pr["meaningful_idle_days"] > 60 else \
               "age-mid" if pr["meaningful_idle_days"] > 30 else ""

    data_attrs = (
        f'data-cats="{html.escape(" ".join(pr["categories"]))}" '
        f'data-ci="{pr["ci"]}" data-areas="{pr["num_areas"]}"'
    )
    return (
        # ---- Title row: spans all 11 metric columns ----
        f'<tr class="pr-title-row" {data_attrs}>'
        f'<td class="pr-title-cell" colspan="11">'
        f'<a class="pr-num-link" href="{pr["url"]}" target="_blank">'
        f'#{pr["number"]}</a>'
        + (f'<span class="badge draft-badge">DRAFT</span>' if pr["draft"] else '')
        + (f'<span class="badge fork-del-badge" title="Source fork deleted; review/commit data unavailable">⚠ fork deleted</span>' if pr["fork_deleted"] else '')
        + f'<a class="pr-title-link" href="{pr["url"]}" target="_blank">'
        f'{html.escape(pr["title"])}</a>'
        f'<span class="pr-by">by {html.escape(pr["author"])}</span>'
        f'</td>'
        f'</tr>'
        # ---- Metric row ----
        f'<tr class="pr-meta-row" {data_attrs}>'
        f'<td class="num {age_cls}">{pr["age_days"]}d</td>'
        f'<td class="num {idle_cls}" title="Last meaningful activity: {pr["last_meaningful_dt"]}">{pr["meaningful_idle_days"]}d</td>'
        f'<td class="num">+{pr["additions"]}/\u2212{pr["deletions"]}</td>'
        f'<td class="num">{pr["num_files"]}</td>'
        f'<td>{area_html}</td>'
        f'<td>{ci_html}</td>'
        f'<td class="num">{pr["num_approvals"]}</td>'
        f'<td class="num">{pr["num_changes_requested"]}</td>'
        f'<td>{assignee_html}</td>'
        f'<td class="num">{pr["total_comments"]}</td>'
        f'<td style="min-width:200px">{cats_html}</td>'
        f'</tr>'
    )


def _change_requesters_table(sorted_reviewers):
    """Render the top change-requesters summary table."""
    if not sorted_reviewers:
        return '<p style="color:var(--muted);font-size:.82rem">No data.</p>'
    rows = []
    for login, d in sorted_reviewers[:30]:
        pr_links = " ".join(
            f'<a href="{url}" target="_blank" title="{html.escape(title[:80])}">'
            f'#{num}</a>'
            for num, url, title in d["prs"]
        )
        avg_age = f'{d["total_age"] / d["total"]:.0f}d' if d["total"] else "\u2014"
        rows.append(
            f'<tr>'
            f'<td><strong>{html.escape(login)}</strong></td>'
            f'<td class="num">{d["total"]}</td>'
            f'<td class="num">{d["ci_pass"] or "\u2014"}</td>'
            f'<td class="num">{d["also_approved"] or "\u2014"}</td>'
            f'<td class="num">{d["no_other_approvals"] or "\u2014"}</td>'
            f'<td class="num">{avg_age}</td>'
            f'<td class="pr-link-list">{pr_links}</td>'
            f'</tr>'
        )
    return (
        '<table class="simple-table">'
        '<thead><tr>'
        '<th>Reviewer</th>'
        '<th title="PRs where this reviewer has an unresolved change request">'
        '# PRs blocked</th>'
        '<th title="Of those, PRs where CI is currently passing">'
        '# CI passing</th>'
        '<th title="PRs where at least one other reviewer has already approved">'
        '# Others approved</th>'
        '<th title="PRs where no other reviewer has approved yet — '
        'only this reviewer\'s feedback is pending">'
        '# Sole blocker</th>'
        '<th title="Average age (days) of PRs blocked by this reviewer\'s change request">'
        'Avg PR age</th>'
        '<th>PRs</th>'
        '</tr></thead>'
        '<tbody>' + "\n".join(rows) + '</tbody>'
        '</table>'
    )


def _assignee_table(sorted_assignees):
    """Render the by-assignee summary table."""
    if not sorted_assignees:
        return '<p style="color:var(--muted);font-size:.82rem">No data.</p>'
    rows = []
    for login, d in sorted_assignees[:30]:
        pr_links = " ".join(
            f'<a href="{url}" target="_blank" title="{html.escape(title[:80])}">'
            f'#{num}</a>'
            for num, url, title in d["prs"]
        )
        rows.append(
            f'<tr>'
            f'<td><strong>{html.escape(login)}</strong></td>'
            f'<td class="num">{d["total"]}</td>'
            f'<td class="num">{d["ci_fail"] or "\u2014"}</td>'
            f'<td class="num">{d["changes_req"] or "\u2014"}</td>'
            f'<td class="num">{d["missing_approval"] or "\u2014"}</td>'
            f'<td class="num">{d["nearly_approved"] or "\u2014"}</td>'
            f'<td class="pr-link-list">{pr_links}</td>'
            f'</tr>'
        )
    return (
        '<table class="simple-table">'
        '<thead><tr>'
        '<th>Assignee</th>'
        '<th title="Total PRs assigned to this person"># PRs</th>'
        '<th title="Of those, PRs where CI is failing"># CI Fail</th>'
        '<th title="PRs with unresolved change-request reviews"># Changes Req.</th>'
        '<th title="PRs where the assignee has not yet approved">'
        '# Awaiting Own Approval</th>'
        '<th title="PRs that already meet the 2-approval rule">'
        '# Nearly Approved</th>'
        '<th>PRs</th>'
        '</tr></thead>'
        '<tbody>' + "\n".join(rows) + '</tbody>'
        '</table>'
    )


def _ci_workflow_table(workflow_counter, workflow_prs):
    """Render the CI workflow failures table."""
    if not workflow_counter:
        return (
            '<p style="color:var(--muted);font-size:.82rem">'
            'No CI failure details recorded '
            '(check runs may not be accessible with the current token).</p>'
        )
    rows = []
    for name, count in workflow_counter.most_common(30):
        pr_links = " ".join(
            f'<a href="{url}" target="_blank">#{num}</a>'
            for num, url in workflow_prs[name]
        )
        rows.append(
            f'<tr>'
            f'<td><code>{html.escape(name)}</code></td>'
            f'<td class="num">{count}</td>'
            f'<td class="pr-link-list">{pr_links}</td>'
            f'</tr>'
        )
    return (
        '<table class="simple-table">'
        '<thead><tr>'
        '<th>Workflow / Check</th>'
        '<th title="Number of PRs on which this check is failing">'
        '# PRs Failing</th>'
        '<th>PRs</th>'
        '</tr></thead>'
        '<tbody>' + "\n".join(rows) + '</tbody>'
        '</table>'
    )


def render_html(org, repo, age_days, pr_data_list, generated, history=None):
    """Render the full HTML report.

    Returns a ``(html_string, snapshot)`` tuple where *snapshot* is a dict
    capturing the key statistics of this run so it can be persisted to a
    history file by the caller.

    *history* is an optional list of previously saved snapshot dicts (oldest
    first).  When provided, summary cards show deltas vs. the most recent
    prior run and a trend table is rendered.
    """
    total = len(pr_data_list)
    if history is None:
        history = []

    # ---- Summary stats ----
    no_assignee = sum(1 for p in pr_data_list if not p["assignees"])
    ci_fail = sum(1 for p in pr_data_list if p["ci"] == "fail")
    changes_req = sum(1 for p in pr_data_list if p["changes_requested_by"])
    nearly_done = sum(1 for p in pr_data_list if p["two_approvals_met"])
    large_prs = sum(1 for p in pr_data_list if p["total_lines"] > SIZE_LARGE_LINES)
    many_areas = sum(1 for p in pr_data_list if p["num_non_meta_areas"] >= SIZE_MANY_AREAS)
    avg_age = sum(p["age_days"] for p in pr_data_list) / total if total else 0.0
    avg_idle = sum(p["meaningful_idle_days"] for p in pr_data_list) / total if total else 0.0
    maint_submitted = sum(1 for p in pr_data_list if p["submitter_is_maintainer"])
    num_drafts = sum(1 for p in pr_data_list if p["draft"])
    num_needs_rebase = sum(1 for p in pr_data_list if p["needs_rebase"])
    num_dnm = sum(1 for p in pr_data_list if p["has_dnm"])
    num_arch_review = sum(1 for p in pr_data_list if p["has_arch_review"])

    # ---- Build current-run snapshot (saved by caller to history file) ----
    snapshot = {
        "timestamp": datetime.datetime.now(
            datetime.timezone.utc).isoformat(),
        "generated": generated,
        "total": total,
        "avg_age": round(avg_age, 1),
        "avg_idle": round(avg_idle, 1),
        "no_assignee": no_assignee,
        "ci_fail": ci_fail,
        "changes_req": changes_req,
        "nearly_done": nearly_done,
        "large_prs": large_prs,
        "many_areas": many_areas,
        "maint_submitted": maint_submitted,
        "num_drafts": num_drafts,
        "num_needs_rebase": num_needs_rebase,
        "num_dnm": num_dnm,
        "num_arch_review": num_arch_review,
    }

    # ---- Deltas vs. the most recent prior run ----
    prev = history[-1] if history else None

    def _d(cur, key, lib=True):
        """Shorthand: delta_html comparing *cur* to prev[key]."""
        return _delta_html(cur, prev.get(key) if prev else None,
                           lower_is_better=lib)

    # ---- Summary cards ----
    cards = [
        _summary_card(total, "PRs", "#2c3e50",
                      _d(total, "total")),
        _summary_card(f"{avg_age:.0f}d", "Avg age", "#e67e22",
                      _d(avg_age, "avg_age")),
        _summary_card(f"{avg_idle:.0f}d", "Avg meaningful idle", "#e67e22",
                      _d(avg_idle, "avg_idle")),
        _summary_card(no_assignee, "No assignee", "#e74c3c",
                      _d(no_assignee, "no_assignee")),
        _summary_card(ci_fail, "CI failing", "#c0392b",
                      _d(ci_fail, "ci_fail")),
        _summary_card(changes_req, "Changes requested", "#8e44ad",
                      _d(changes_req, "changes_req")),
        _summary_card(nearly_done, "Nearly approved", "#1abc9c",
                      _d(nearly_done, "nearly_done", lib=False)),
        _summary_card(large_prs, "Large PRs", "#d35400",
                      _d(large_prs, "large_prs")),
        _summary_card(many_areas, "Many code areas (≥ 4)", "#16a085",
                      _d(many_areas, "many_areas")),
        _summary_card(maint_submitted, "Maintainer author", "#27ae60",
                      _d(maint_submitted, "maint_submitted", lib=False)),
        _summary_card(num_drafts, "Draft PRs", "#7f8c8d",
                      _d(num_drafts, "num_drafts")),
        _summary_card(num_needs_rebase, "Needs rebase", "#c0392b",
                      _d(num_needs_rebase, "num_needs_rebase")),
        _summary_card(num_dnm, "Do Not Merge", "#2c3e50",
                      _d(num_dnm, "num_dnm")),
        _summary_card(num_arch_review, "Architecture Review", "#6c3483",
                      _d(num_arch_review, "num_arch_review")),
    ]

    # ---- Category cards + bar chart ----
    cat_counts = collections.Counter()
    for p in pr_data_list:
        for c in p["categories"]:
            cat_counts[c] += 1

    cat_cards_html = []
    for key, meta in CATEGORY_META.items():
        cnt = cat_counts.get(key, 0)
        cat_cards_html.append(
            f'<div class="cat-card" style="border-left-color:{meta["color"]}">'
            f'<span class="cat-count" style="color:{meta["color"]}">{cnt}</span>'
            f'<div class="cat-label">{html.escape(meta["label"])}</div>'
            f'<div class="cat-desc">{html.escape(meta["description"])}</div>'
            f'</div>'
        )

    sorted_cats = sorted(
        [(CATEGORY_META[k]["label"], v)
         for k, v in cat_counts.items()],
        key=lambda x: x[1], reverse=True
    )
    max_cat = sorted_cats[0][1] if sorted_cats else 1
    key_by_label = {CATEGORY_META[k]["label"]: k for k in CATEGORY_META}
    bar_html = _bar_chart_rows(
        sorted_cats, max_cat,
        color_fn=lambda name: CATEGORY_META[key_by_label.get(name, "")]["color"]
        if key_by_label.get(name) else "#3498db"
    )

    # ---- Category filter options ----
    cat_option_html = "\n".join(
        f'<option value="{k}">{html.escape(CATEGORY_META[k]["label"])}</option>'
        for k in CATEGORY_META
    )

    # ---- Heatmap ----
    heatmap_html = _heatmap_html(pr_data_list)

    # ---- PR rows (server-rendered initial state, sorted by age desc) ----
    sorted_prs = sorted(pr_data_list, key=lambda p: p["age_days"], reverse=True)
    pr_rows_html = "\n".join(_pr_row_html(p) for p in sorted_prs)

    # ---- Area chart (code areas only, meta areas excluded) ----
    area_counter = collections.Counter()
    for p in pr_data_list:
        for a in p["non_meta_areas"]:
            area_counter[a] += 1
    top_areas = area_counter.most_common(20)
    max_area = top_areas[0][1] if top_areas else 1
    area_chart_html = _bar_chart_rows(top_areas, max_area)

    # ---- Author chart ----
    author_counter = collections.Counter(p["author"] for p in pr_data_list)
    top_authors = author_counter.most_common(20)
    max_author = top_authors[0][1] if top_authors else 1
    author_chart_html = _bar_chart_rows(top_authors, max_author)

    # ---- Assignee table ----
    assignee_data = collections.defaultdict(lambda: {
        "total": 0,
        "ci_fail": 0,
        "changes_req": 0,
        "missing_approval": 0,
        "nearly_approved": 0,
        "prs": [],
    })
    for p in pr_data_list:
        buckets = p["assignees"] if p["assignees"] else ["(unassigned)"]
        for a in buckets:
            d = assignee_data[a]
            d["total"] += 1
            if p["ci"] == "fail":
                d["ci_fail"] += 1
            if CAT_CHANGES_REQUESTED in p["categories"]:
                d["changes_req"] += 1
            if CAT_MISSING_ASSIGNEE_APPROVAL in p["categories"]:
                d["missing_approval"] += 1
            if CAT_NEARLY_APPROVED in p["categories"]:
                d["nearly_approved"] += 1
            d["prs"].append((p["number"], p["url"], p["title"]))
    sorted_assignees = sorted(
        assignee_data.items(),
        key=lambda x: x[1]["total"],
        reverse=True,
    )
    assignee_table_html = _assignee_table(sorted_assignees)

    # ---- Change-requesters table ----
    reviewer_data = collections.defaultdict(lambda: {
        "total": 0,
        "ci_pass": 0,
        "also_approved": 0,
        "no_other_approvals": 0,
        "total_age": 0,
        "prs": [],
    })
    for p in pr_data_list:
        for reviewer in p["changes_requested_by"]:
            d = reviewer_data[reviewer]
            d["total"] += 1
            d["total_age"] += p["age_days"]
            if p["ci"] == "pass":
                d["ci_pass"] += 1
            other_approvals = [u for u in p["approved_by"] if u != reviewer]
            if other_approvals:
                d["also_approved"] += 1
            else:
                d["no_other_approvals"] += 1
            d["prs"].append((p["number"], p["url"], p["title"]))
    sorted_reviewers = sorted(
        reviewer_data.items(),
        key=lambda x: x[1]["total"],
        reverse=True,
    )
    change_requesters_table_html = _change_requesters_table(sorted_reviewers)

    # ---- CI workflow failures table ----
    workflow_counter = collections.Counter()
    workflow_prs = collections.defaultdict(list)
    for p in pr_data_list:
        for check in p["ci_failing_checks"]:
            workflow_counter[check] += 1
            workflow_prs[check].append((p["number"], p["url"]))
    ci_workflow_table_html = _ci_workflow_table(workflow_counter, workflow_prs)

    # ---- JSON for client-side filtering ----
    pr_json = json.dumps(
        [{
            "number": p["number"],
            "title": p["title"],
            "url": p["url"],
            "author": p["author"],
            "age_days": p["age_days"],
            "meaningful_idle_days": p["meaningful_idle_days"],
            "last_meaningful_dt": p["last_meaningful_dt"],
            "additions": p["additions"],
            "deletions": p["deletions"],
            "total_lines": p["total_lines"],
            "num_files": p["num_files"],
            "num_areas": p["num_areas"],
            "num_non_meta_areas": p["num_non_meta_areas"],
            "areas": p["areas"],
            "non_meta_areas": p["non_meta_areas"],
            "ci": p["ci"],
            "ci_failing_checks": p["ci_failing_checks"],
            "num_approvals": p["num_approvals"],
            "num_changes_requested": p["num_changes_requested"],
            "assignees": p["assignees"],
            "total_comments": p["total_comments"],
            "categories": p["categories"],
            "two_approvals_met": p["two_approvals_met"],
            "draft": p["draft"],
            "fork_deleted": p["fork_deleted"],
        }
        for p in pr_data_list],
        indent=None
    )

    cat_colors_json = json.dumps(
        {k: v["color"] for k, v in CATEGORY_META.items()}
    )
    cat_labels_json = json.dumps(
        {k: v["label"] for k, v in CATEGORY_META.items()}
    )

    # ---- Trend history section ----
    # Include the current run in the table so this run's values are visible
    # even before the caller writes the file.
    all_runs = list(history) + [snapshot]
    if len(all_runs) >= 2:
        trend_section = (
            '<div class="section-title">Backlog Trend History</div>\n'
            '<p style="font-size:.8rem;color:var(--muted);margin-bottom:8px;">'
            'Each row is one saved run.  Arrows show change vs. the '
            'previous run; green = improving, red = worsening.</p>\n'
            + _trend_table_html(all_runs)
        )
    else:
        trend_section = ""

    report = _HTML_TEMPLATE.format(
        org=html.escape(org),
        repo=html.escape(repo),
        age_days=age_days,
        generated=html.escape(generated),
        total_prs=total,
        summary_cards="\n".join(cards),
        trend_section=trend_section,
        cat_cards="\n".join(cat_cards_html),
        bar_chart=bar_html,
        cat_options=cat_option_html,
        heatmap=heatmap_html,
        pr_rows=pr_rows_html,
        area_chart=area_chart_html,
        author_chart=author_chart_html,
        assignee_table=assignee_table_html,
        change_requesters_table=change_requesters_table_html,
        ci_workflow_table=ci_workflow_table_html,
        pr_json=pr_json,
        cat_colors_json=cat_colors_json,
        cat_labels_json=cat_labels_json,
    )
    return report, snapshot


# ---------------------------------------------------------------------------
# Single-PR debug helper
# ---------------------------------------------------------------------------

def _debug_single_pr(gh_repo, pr_number, args):
    """Fetch one PR by number, run _analyze_pr, and print the result as JSON."""
    maint_obj = None
    if _HAS_MAINTAINERS:
        mf = pathlib.Path(args.maintainer_file)
        if mf.exists():
            try:
                maint_obj = Maintainers(str(mf))
            except Exception as exc:
                print(
                    f"WARNING: Could not load maintainer file: {exc}",
                    file=sys.stderr,
                )
        else:
            print(
                f"WARNING: Maintainer file not found: {mf}",
                file=sys.stderr,
            )

    try:
        pr = gh_repo.get_pull(pr_number)
    except GithubException as exc:
        sys.exit(f"ERROR: Could not fetch PR #{pr_number}: {exc}")

    print(
        f"Analysing PR #{pr.number}: {pr.title}",
        file=sys.stderr,
        flush=True,
    )

    data = _analyze_pr(pr, maint_obj, verbose=True)
    print(json.dumps(data, indent=2, default=str))


# ---------------------------------------------------------------------------
# Run-history helpers
# ---------------------------------------------------------------------------

def _load_history(path):
    """Load run history from *path*.  Returns an empty list on any error."""
    p = pathlib.Path(path)
    if not p.exists():
        return []
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except Exception as exc:
        print(
            f"WARNING: Could not load history file {path}: {exc}",
            file=sys.stderr,
        )
        return []


def _save_snapshot(path, snapshot, history):
    """Append *snapshot* to *history* and persist to *path*."""
    updated = list(history) + [snapshot]
    try:
        pathlib.Path(path).write_text(
            json.dumps(updated, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
    except Exception as exc:
        print(
            f"WARNING: Could not write history file {path}: {exc}",
            file=sys.stderr,
        )


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False,
    )
    parser.add_argument(
        "--age",
        type=int,
        default=14,
        metavar="DAYS",
        help="Include PRs created at least DAYS days ago (default: 14).",
    )
    parser.add_argument(
        "--org",
        default="zephyrproject-rtos",
        help="GitHub organisation (default: zephyrproject-rtos).",
    )
    parser.add_argument(
        "--repo",
        default="zephyr",
        help="GitHub repository name (default: zephyr).",
    )
    parser.add_argument(
        "--maintainer-file",
        default=str(_ZEPHYR_BASE / "MAINTAINERS.yml"),
        metavar="FILE",
        help="Path to MAINTAINERS.yml (default: auto-detected from tree).",
    )
    parser.add_argument(
        "--max-prs",
        type=int,
        default=200,
        metavar="N",
        help="Maximum number of PRs to analyse (default: 200).",
    )
    parser.add_argument(
        "--output",
        default="pr_backlog.html",
        metavar="FILE",
        help="Output HTML file (default: pr_backlog.html).",
    )
    parser.add_argument(
        "--token",
        default=os.environ.get("GITHUB_TOKEN"),
        metavar="TOKEN",
        help="GitHub personal access token (default: $GITHUB_TOKEN).",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        default=False,
        help="Print progress to stdout.",
    )
    parser.add_argument(
        "--exclude-drafts",
        action="store_true",
        default=False,
        help="Skip draft PRs entirely instead of including them marked as drafts.",
    )
    parser.add_argument(
        "--pr",
        type=int,
        default=None,
        metavar="NUMBER",
        help=(
            "Analyse a single PR by number and print all collected data to "
            "stdout as JSON (for debugging / data inspection).  "
            "No HTML report is written."
        ),
    )
    parser.add_argument(
        "--history",
        default=None,
        metavar="FILE",
        help=(
            "Path to a JSON file used to persist run history.  "
            "If the file exists, previous snapshots are loaded and the "
            "report shows trend indicators comparing this run to the last "
            "saved run.  After rendering, the current run's statistics are "
            "appended to the file (created if it does not exist).  "
            "Has no effect when --pr is used."
        ),
    )
    return parser.parse_args()


def main():
    args = parse_args()

    if not args.token:
        print(
            "WARNING: No GitHub token found.  "
            "Set GITHUB_TOKEN or pass --token to avoid rate limiting.",
            file=sys.stderr,
        )

    # ---- Connect ----
    if args.verbose:
        print(f"Connecting to GitHub ({args.org}/{args.repo})…", flush=True)
    gh = _gh_connect(args.token)
    gh_repo = gh.get_repo(f"{args.org}/{args.repo}")

    # ---- Single-PR debug mode ----
    if args.pr is not None:
        _debug_single_pr(gh_repo, args.pr, args)
        return

    # ---- Load MAINTAINERS.yml ----
    maint_obj = None
    if _HAS_MAINTAINERS:
        mf = pathlib.Path(args.maintainer_file)
        if mf.exists():
            if args.verbose:
                print(f"Loading maintainer file: {mf}", flush=True)
            try:
                maint_obj = Maintainers(str(mf))
            except Exception as exc:
                print(f"WARNING: Could not load maintainer file: {exc}", file=sys.stderr)
        else:
            print(
                f"WARNING: Maintainer file not found: {mf}",
                file=sys.stderr,
            )

    # ---- Fetch PRs ----
    cutoff = datetime.datetime.now(datetime.timezone.utc) - datetime.timedelta(days=args.age)

    if args.verbose:
        print(
            f"Fetching open PRs created before {cutoff.date()} "
            f"(older than {args.age} days)…",
            flush=True,
        )

    pr_data_list = []
    fetched = 0
    stop_reason = "exhausted"
    _pull_iter = iter(gh_repo.get_pulls(
        state="open", sort="created", direction="asc"
    ))
    while True:
        try:
            pr = next(_pull_iter)
        except StopIteration:
            break
        except GithubException as exc:
            print(
                f"WARNING: GitHub API error while paginating PRs ({exc.status}); "
                f"stopping after {fetched} PRs fetched.  "
                "Try re-running — this is usually transient.",
                file=sys.stderr,
            )
            stop_reason = f"api_error_{exc.status}"
            break

        created = pr.created_at
        if created.tzinfo is None:
            created = created.replace(tzinfo=datetime.timezone.utc)

        # PRs created after the cutoff are too recent — stop
        if created > cutoff:
            stop_reason = "cutoff"
            break

        # Skip draft PRs when requested (before counting toward max_prs)
        if args.exclude_drafts and pr.draft:
            if args.verbose:
                print(f"  Skipping draft PR #{pr.number}", flush=True)
            continue

        if fetched >= args.max_prs:
            stop_reason = "max_prs"
            if args.verbose:
                print(f"Reached --max-prs={args.max_prs} limit.", flush=True)
            break

        try:
            data = _analyze_pr(pr, maint_obj, verbose=args.verbose)
            pr_data_list.append(data)
        except GithubException as exc:
            print(f"  Skipping PR #{pr.number}: {exc}", file=sys.stderr)
        except Exception as exc:
            print(f"  Error on PR #{pr.number}: {exc}", file=sys.stderr)

        fetched += 1

        # Respect secondary rate limit: brief pause every 10 PRs
        if fetched % 10 == 0:
            time.sleep(2)

    if args.verbose:
        reasons = {
            "exhausted": "no more open PRs",
            "cutoff": f"reached age cutoff ({cutoff.date()}: PRs created after this are too recent)",
            "max_prs": f"reached --max-prs={args.max_prs} limit",
        }
        print(
            f"Loop stopped: {reasons.get(stop_reason, stop_reason)}. "
            f"Analysed {len(pr_data_list)} PRs.",
            flush=True,
        )

    if not pr_data_list:
        print(
            "No PRs matched the criteria.  "
            "Try --age with a smaller value or check your token permissions.",
            file=sys.stderr,
        )
        sys.exit(1)

    # ---- Load run history (if requested) ----
    history = []
    if args.history:
        history = _load_history(args.history)
        if args.verbose and history:
            print(
                f"Loaded {len(history)} historical run(s) from {args.history}",
                flush=True,
            )

    # ---- Render HTML ----
    generated = datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%d %H:%M UTC"
    )
    report, snapshot = render_html(
        args.org, args.repo, args.age, pr_data_list, generated,
        history=history,
    )

    out = pathlib.Path(args.output)
    out.write_text(report, encoding="utf-8")
    print(f"Report written to {out.resolve()}")

    # ---- Persist snapshot to history file ----
    if args.history:
        _save_snapshot(args.history, snapshot, history)
        if args.verbose:
            print(
                f"Snapshot appended to history file: {args.history}",
                flush=True,
            )


if __name__ == "__main__":
    main()
