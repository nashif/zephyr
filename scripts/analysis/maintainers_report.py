#!/usr/bin/env python3
"""Analyze MAINTAINERS.yml and generate a browsable HTML report."""

import os
import sys
import glob
import re
import json
import time
import argparse
import urllib.request
import urllib.error
import pathlib
from pathlib import Path
from collections import defaultdict, Counter
import datetime

try:
    import yaml
except ImportError:
    print("pyyaml not found, installing...")
    os.system("pip install pyyaml -q")
    import yaml

REPO_ROOT = Path(__file__).parent.parent.parent
MAINTAINERS_FILE = REPO_ROOT / "MAINTAINERS.yml"
OUTPUT_FILE = REPO_ROOT / "maintainers_report.html"

# Directories excluded from total-repo-file counts (build artefacts etc.)
_REPO_EXCLUDE_DIRS = {
    ".git", ".github", "twister-out", "build", "__pycache__",
    ".tox", ".mypy_cache", ".pytest_cache", "_build",
}


def load_maintainers(path=None):
    """Load and parse a MAINTAINERS.yml file, stripping the keep-sorted markers.

    *path* overrides the default MAINTAINERS_FILE when supplied.
    """
    target = Path(path) if path else MAINTAINERS_FILE
    with open(target, "r") as f:
        content = f.read()
    # Remove zephyr-keep-sorted markers
    content = re.sub(r'#\s*zephyr-keep-sorted-(start|stop).*\n', '', content)
    return yaml.safe_load(content)


def count_files_for_globs(files_list, repo_root, *, collect=False):
    """Count (and optionally return) actual files matching the given glob patterns."""
    matched = set()
    if not files_list:
        return (0, set()) if collect else 0
    for pattern in files_list:
        full_pattern = str(repo_root / pattern)
        try:
            matches = glob.glob(full_pattern, recursive=True)
            for m in matches:
                if os.path.isfile(m):
                    matched.add(m)
                elif os.path.isdir(m):
                    for root, dirs, files in os.walk(m):
                        dirs[:] = [d for d in dirs if d not in _REPO_EXCLUDE_DIRS]
                        for f in files:
                            matched.add(os.path.join(root, f))
        except Exception:
            pass
    return (len(matched), matched) if collect else len(matched)


def has_path_type(files_list, keywords):
    """Check if any file path contains given keywords."""
    if not files_list:
        return False
    for f in files_list:
        f_lower = f.lower()
        if any(kw in f_lower for kw in keywords):
            return True
    return False


def classify_area(name, _files=None):
    """Classify area by name only. Returns one of: General | Driver | Platform | West Project."""
    # Strip surrounding quotes that YAML sometimes preserves in the key
    bare = name.strip('"').strip("'")
    if re.match(r'West project:', bare, re.IGNORECASE):
        return "West Project"
    if re.match(r'Drivers?:', bare, re.IGNORECASE):
        return "Driver"
    if re.search(r'\bPlatforms?\b', bare, re.IGNORECASE):
        return "Platform"
    return "General"


# Health thresholds per type: (excellent, good, fair, poor)
# General and Driver are stricter — they need all 6 criteria ideally.
_HEALTH_THRESHOLDS = {
    "General":      (5, 4, 3, 2),
    "Driver":       (5, 4, 3, 2),
    "Platform":     (4, 3, 2, 1),
    "West Project": (3, 2, 1, 0),
}


def health_level(score, area_type):
    """Return (label, css_class) based on score and type-adjusted thresholds."""
    excellent, good, fair, poor = _HEALTH_THRESHOLDS.get(area_type, (5, 4, 3, 2))
    if score >= excellent:
        return "Excellent", "badge-excellent"
    elif score >= good:
        return "Good", "badge-good"
    elif score >= fair:
        return "Fair", "badge-fair"
    elif score >= poor:
        return "Poor", "badge-poor"
    else:
        return "Critical", "badge-critical"


def analyze_areas(data):
    """Analyze all areas and return structured data."""
    areas = []
    all_maintainers = set()
    all_collaborators = set()

    for area_name, props in data.items():
        if not isinstance(props, dict):
            continue

        maintainers = props.get("maintainers", []) or []
        collaborators = props.get("collaborators", []) or []
        files = props.get("files", []) or []
        files_regex = props.get("files-regex", []) or []
        files_exclude = props.get("files-exclude", []) or []
        status = props.get("status", "unknown")
        labels = props.get("labels", []) or []
        tests_ids = props.get("tests", []) or []
        description = props.get("description", "")
        file_groups = props.get("file-groups", []) or []

        all_maintainers.update(maintainers)
        all_collaborators.update(collaborators)

        # Combine all file patterns for coverage check
        all_file_patterns = files + files_regex

        # Health checks
        has_maintainer = len(maintainers) >= 1
        has_two_collabs = len(collaborators) >= 2
        has_samples = has_path_type(files + files_regex, ["sample", "samples"])
        has_tests = has_path_type(files + files_regex, ["test", "tests"])
        has_docs = has_path_type(files + files_regex, ["doc/", "docs/", ".rst", ".md"])
        has_test_ids = len(tests_ids) > 0
        has_labels = len(labels) > 0

        # Count file patterns
        file_pattern_count = len(files) + len(files_regex)

        # Classify area type
        area_type = classify_area(area_name)

        # Score health (0-6) — raw count of satisfied criteria
        health_score = sum([
            has_maintainer,
            has_two_collabs,
            has_samples,
            has_tests,
            has_docs,
            has_test_ids,
        ])

        area = {
            "name": area_name,
            "area_type": area_type,
            "status": status,
            "maintainers": maintainers,
            "collaborators": collaborators,
            "files": files,
            "files_regex": files_regex,
            "files_exclude": files_exclude,
            "labels": labels,
            "tests_ids": tests_ids,
            "description": description,
            "file_groups": file_groups,
            "has_maintainer": has_maintainer,
            "has_two_collabs": has_two_collabs,
            "has_samples": has_samples,
            "has_tests": has_tests,
            "has_docs": has_docs,
            "has_test_ids": has_test_ids,
            "has_labels": has_labels,
            "health_score": health_score,
            "file_pattern_count": file_pattern_count,
            "num_maintainers": len(maintainers),
            "num_collaborators": len(collaborators),
        }
        areas.append(area)

    return areas, all_maintainers, all_collaborators


def count_actual_files(areas):
    """Count actual files for each area using glob patterns; also build global covered set."""
    print("Counting files for each area (this may take a moment)...")
    global_covered: set = set()
    for i, area in enumerate(areas):
        if i % 50 == 0:
            print(f"  Processing area {i+1}/{len(areas)}: {area['name'][:40]}")
        cnt, matched = count_files_for_globs(area["files"], REPO_ROOT, collect=True)
        area["file_count"] = cnt
        global_covered.update(matched)
    return areas, global_covered


def count_total_repo_files(repo_root):
    """Walk the repo and count all tracked files (excluding build artefacts)."""
    total = 0
    for root, dirs, files in os.walk(repo_root):
        dirs[:] = [d for d in dirs if d not in _REPO_EXCLUDE_DIRS]
        total += len(files)
    return total


# ---------------------------------------------------------------------------
# GitHub activity querying
# ---------------------------------------------------------------------------

_GH_REPO = "zephyrproject-rtos/zephyr"
_GH_API  = "https://api.github.com"

# Minimum remaining quota before proactively sleeping (per resource bucket)
_QUOTA_WARN_THRESHOLD = 5


class _RateLimiter:
    """
    Tracks GitHub API rate-limit state across requests and handles waiting
    gracefully.

    GitHub has two rate-limit categories that matter here:
      * "core"   — REST endpoints (5 000/hour authenticated, 60/hour anonymous)
      * "search" — Search API   (30/minute authenticated, 10/minute anonymous)

    The class also enforces an inter-request delay on search endpoints to avoid
    triggering the search secondary rate limit.
    """

    def __init__(self, retry: bool, search_delay: float = 1.0):
        self.retry        = retry
        self.search_delay = search_delay   # seconds between search API calls
        self._buckets: dict = {}           # resource -> {remaining, reset}
        self._last_search_ts: float = 0.0  # time of last search request

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _record(self, headers):
        """Update internal state from an http.client.HTTPMessage (or plain dict)."""
        def _h(name):
            # urllib gives us an HTTPMessage; plain dicts are used in tests
            return headers.get(name) or headers.get(name.lower())

        resource  = _h("X-RateLimit-Resource") or "core"
        remaining = _h("X-RateLimit-Remaining")
        reset     = _h("X-RateLimit-Reset")
        if remaining is not None:
            self._buckets[resource] = {
                "remaining": int(remaining),
                "reset":     int(reset) if reset else 0,
            }

    def _sleep_until(self, reset_epoch: int, reason: str = ""):
        wait = max(1, reset_epoch - int(datetime.datetime.utcnow().timestamp())) + 2
        reset_str = datetime.datetime.utcfromtimestamp(reset_epoch).strftime("%H:%M:%S UTC")
        print(f"\n  [rate-limit] {reason}Sleeping {wait}s until {reset_str} ...",
              flush=True)
        time.sleep(wait)

    # ------------------------------------------------------------------
    # Called by _gh_get
    # ------------------------------------------------------------------

    def throttle_search(self):
        """Enforce minimum spacing between search API calls."""
        if self.search_delay <= 0:
            return
        elapsed = time.monotonic() - self._last_search_ts
        if elapsed < self.search_delay:
            time.sleep(self.search_delay - elapsed)
        self._last_search_ts = time.monotonic()

    def after_success(self, headers, is_search: bool):
        """
        Called after every successful response.  Records headers and
        proactively waits if the quota for this bucket is nearly exhausted.
        """
        self._record(headers)
        if is_search:
            self._last_search_ts = time.monotonic()
        resource = "search" if is_search else "core"
        info = self._buckets.get(resource)
        if info and info["remaining"] <= _QUOTA_WARN_THRESHOLD and info["reset"]:
            if self.retry:
                self._sleep_until(
                    info["reset"],
                    f"{resource} quota nearly exhausted ({info['remaining']} left). ",
                )
            else:
                reset_str = datetime.datetime.utcfromtimestamp(
                    info["reset"]
                ).strftime("%H:%M:%S UTC")
                print(
                    f"\n  [warning] {resource} quota low: "
                    f"{info['remaining']} remaining, resets {reset_str}. "
                    "Use --retry to auto-wait.",
                    flush=True,
                )

    def on_rate_limit_error(self, exc, url: str) -> bool:
        """
        Called on HTTP 403/429.  Returns True if the caller should retry
        the same request, False if it should abort.
        """
        retry_after = exc.headers.get("Retry-After")
        reset       = exc.headers.get("X-RateLimit-Reset")
        resource    = exc.headers.get("X-RateLimit-Resource", "unknown")

        # Update internal state so summary is correct
        if reset:
            self._buckets[resource] = {"remaining": 0, "reset": int(reset)}

        if not self.retry:
            reset_info = ""
            if reset:
                reset_dt = datetime.datetime.utcfromtimestamp(int(reset))
                reset_info = f", resets at {reset_dt.strftime('%H:%M:%S UTC')}"
            print(
                f"\n  [rate-limit] HTTP {exc.code} — {resource} quota exhausted"
                f"{reset_info}. Re-run with --retry to auto-wait.",
                flush=True,
            )
            return False   # abort

        # --retry mode: figure out how long to wait
        if retry_after:
            wait = int(retry_after) + 1
            print(
                f"\n  [rate-limit] HTTP {exc.code} — Retry-After: {retry_after}s. "
                f"Sleeping {wait}s ...",
                flush=True,
            )
            time.sleep(wait)
        elif reset:
            self._sleep_until(
                int(reset),
                f"HTTP {exc.code} from {url}. ",
            )
        else:
            print(
                f"\n  [rate-limit] HTTP {exc.code} — no reset info, sleeping 60s ...",
                flush=True,
            )
            time.sleep(60)
        return True   # retry

    def summary(self) -> str:
        """Human-readable rate-limit status for all known buckets."""
        if not self._buckets:
            return "    (no rate-limit data received)"
        lines = []
        for res, info in sorted(self._buckets.items()):
            reset_str = ""
            if info["reset"]:
                reset_dt = datetime.datetime.utcfromtimestamp(info["reset"])
                reset_str = f", resets {reset_dt.strftime('%H:%M:%S UTC')}"
            lines.append(f"    {res}: {info['remaining']} remaining{reset_str}")
        return "\n".join(lines)


def _gh_get(path, token, limiter=None, *, max_retries=3):
    """Make an authenticated GitHub API GET request; return parsed JSON or None.

    Returns ``"RATE_LIMITED"`` when the quota is exhausted and ``--retry``
    is not active (or retries are exceeded).
    """
    url       = f"{_GH_API}{path}"
    is_search = path.startswith("/search/")

    # Throttle search requests to avoid secondary rate limits
    if limiter and is_search:
        limiter.throttle_search()

    for attempt in range(max_retries):
        req = urllib.request.Request(url)
        req.add_header("Accept",               "application/vnd.github+json")
        req.add_header("X-GitHub-Api-Version", "2022-11-28")
        if token:
            req.add_header("Authorization", f"Bearer {token}")
        try:
            with urllib.request.urlopen(req, timeout=20) as resp:
                data = json.loads(resp.read().decode())
                if limiter:
                    limiter.after_success(resp.headers, is_search)
                return data
        except urllib.error.HTTPError as e:
            if e.code in (403, 429):
                if limiter:
                    should_retry = limiter.on_rate_limit_error(e, url)
                    if should_retry and attempt < max_retries - 1:
                        continue
                return "RATE_LIMITED"
            if e.code == 404:
                return None
            print(f"  [warning] GitHub API {url}: HTTP {e.code}")
            return None
        except Exception as exc:
            print(f"  [warning] GitHub API {url}: {exc}")
            return None
    return "RATE_LIMITED"


def _latest_activity(username, token, since_days, limiter=None):
    """
    Query the zephyrproject-rtos/zephyr repo for a user's most recent activity.
    Checks: commits authored, PRs opened, PR reviews — all scoped to _GH_REPO.

    Returns a dict:
      last_commit   : ISO date string or None
      last_pr       : ISO date string or None
      last_review   : ISO date string or None
      last_activity : latest of the three (ISO) or None
      is_inactive   : True when last_activity is older than since_days (or absent)
      rate_limited  : True if we hit an unrecoverable rate limit
    """
    result = {
        "last_commit":   None,
        "last_pr":       None,
        "last_review":   None,
        "last_activity": None,
        "is_inactive":   True,
        "rate_limited":  False,
    }
    cutoff = (
        datetime.datetime.utcnow() - datetime.timedelta(days=since_days)
    ).strftime("%Y-%m-%dT%H:%M:%SZ")

    # --- most recent commit authored in this repo ---
    data = _gh_get(
        f"/repos/{_GH_REPO}/commits?author={username}&per_page=1",
        token, limiter,
    )
    if data == "RATE_LIMITED":
        result["rate_limited"] = True
        return result
    if data and isinstance(data, list) and data:
        result["last_commit"] = (
            data[0].get("commit", {}).get("author", {}).get("date")
        )

    # --- most recent PR authored in this repo (search API, repo-scoped) ---
    data = _gh_get(
        f"/search/issues"
        f"?q=author:{username}+repo:{_GH_REPO}+type:pr"
        f"&sort=updated&order=desc&per_page=1",
        token, limiter,
    )
    if data == "RATE_LIMITED":
        result["rate_limited"] = True
        return result
    if data and isinstance(data, dict) and data.get("items"):
        result["last_pr"] = data["items"][0].get("updated_at")

    # --- most recent PR review in this repo (search API, repo-scoped) ---
    data = _gh_get(
        f"/search/issues"
        f"?q=reviewed-by:{username}+repo:{_GH_REPO}+type:pr"
        f"&sort=updated&order=desc&per_page=1",
        token, limiter,
    )
    if data == "RATE_LIMITED":
        result["rate_limited"] = True
        return result
    if data and isinstance(data, dict) and data.get("items"):
        result["last_review"] = data["items"][0].get("updated_at")

    # Determine overall latest
    dates = [
        d for d in (result["last_commit"], result["last_pr"], result["last_review"])
        if d
    ]
    if dates:
        result["last_activity"] = max(dates)
        result["is_inactive"] = result["last_activity"] < cutoff
    return result


def fetch_activity(usernames, token, since_days, retry=False, search_delay=1.0):
    """
    Query GitHub for each username in the zephyr repo.
    Returns a dict: username -> activity_dict.

    Rate limiting is handled by ``_RateLimiter``:
      * Proactively sleeps when quota drops below _QUOTA_WARN_THRESHOLD.
      * On 403/429 waits for the reset window when ``retry=True``,
        otherwise stops early and reports how to resume.
    """
    limiter  = _RateLimiter(retry=retry, search_delay=search_delay)
    activity = {}
    total    = len(usernames)
    stopped_early = False

    for i, user in enumerate(sorted(usernames), 1):
        print(f"  [{i}/{total}] {user}", end="", flush=True)
        info = _latest_activity(user, token, since_days, limiter)
        activity[user] = info
        if info["rate_limited"]:
            print(" — unrecoverable rate limit, stopping.")
            stopped_early = True
            break
        status = "inactive" if info["is_inactive"] else "active"
        last   = info["last_activity"] or "never"
        print(f" → {status} (last: {last})")

    print(f"  Rate-limit status after queries:\n{limiter.summary()}")
    if stopped_early:
        remaining = total - len(activity)
        print(f"  NOTE: {remaining} user(s) were not checked due to rate limiting.")
        print("  Re-run with --retry to automatically wait for quota resets.")
    return activity


def compute_stats(areas, all_maintainers, all_collaborators, global_covered=None, total_repo_files=0, activity=None):
    """Compute summary statistics."""
    total = len(areas)
    status_counts = Counter(a["status"] for a in areas)
    maintained = status_counts.get("maintained", 0)
    odd_fixes = status_counts.get("odd fixes", 0)
    obsolete = status_counts.get("obsolete", 0)
    unknown = total - maintained - odd_fixes - obsolete

    no_maintainer = sum(1 for a in areas if not a["has_maintainer"])
    no_collab = sum(1 for a in areas if len(a["collaborators"]) == 0)
    one_collab = sum(1 for a in areas if len(a["collaborators"]) == 1)
    two_plus_collab = sum(1 for a in areas if a["has_two_collabs"])

    has_samples = sum(1 for a in areas if a["has_samples"])
    has_tests = sum(1 for a in areas if a["has_tests"])
    has_docs = sum(1 for a in areas if a["has_docs"])
    has_test_ids = sum(1 for a in areas if a["has_test_ids"])
    has_labels = sum(1 for a in areas if a["has_labels"])

    # Health score distribution
    health_dist = Counter(a["health_score"] for a in areas)

    # Top maintainers by area count
    maintainer_area_count = Counter()
    for a in areas:
        for m in a["maintainers"]:
            maintainer_area_count[m] += 1

    # Top collaborators by area count
    collab_area_count = Counter()
    for a in areas:
        for c in a["collaborators"]:
            collab_area_count[c] += 1

    # Areas with most maintainers
    multi_maintainer = sorted([a for a in areas if a["num_maintainers"] > 1],
                              key=lambda x: x["num_maintainers"], reverse=True)

    # Areas with highest file patterns
    most_files = sorted(areas, key=lambda x: x["file_pattern_count"], reverse=True)[:20]

    # Per-type breakdown
    type_counts = Counter(a["area_type"] for a in areas)
    type_stats = {}
    for atype in ["General", "Driver", "Platform", "West Project"]:
        typed = [a for a in areas if a["area_type"] == atype]
        excellent_t, good_t, fair_t, poor_t = _HEALTH_THRESHOLDS.get(atype, (5, 4, 3, 2))
        type_stats[atype] = {
            "count": len(typed),
            "maintained": sum(1 for a in typed if a["status"] == "maintained"),
            "no_maintainer": sum(1 for a in typed if not a["has_maintainer"]),
            "two_plus_collab": sum(1 for a in typed if a["has_two_collabs"]),
            "has_test_ids": sum(1 for a in typed if a["has_test_ids"]),
            "has_docs": sum(1 for a in typed if a["has_docs"]),
            "has_samples": sum(1 for a in typed if a["has_samples"]),
            "avg_health": (sum(a["health_score"] for a in typed) / len(typed)) if typed else 0,
            "critical": sum(1 for a in typed if health_level(a["health_score"], atype)[0] in ("Critical", "Poor")),
        }

    # Areas sorted by health score (worst first), weighted by type criticality
    # General and Driver areas have stricter thresholds so they surface first
    def worst_sort_key(a):
        type_weight = {"General": 0, "Driver": 1, "Platform": 2, "West Project": 3}
        return (type_weight.get(a["area_type"], 99), a["health_score"])

    worst_health = [
        a for a in sorted(areas, key=worst_sort_key)
        if health_level(a["health_score"], a["area_type"])[0] in ("Critical", "Poor")
    ]

    return {
        "total": total,
        "maintained": maintained,
        "odd_fixes": odd_fixes,
        "obsolete": obsolete,
        "unknown": unknown,
        "unique_maintainers": len(all_maintainers),
        "unique_collaborators": len(all_collaborators),
        "all_maintainers": sorted(all_maintainers),
        "all_collaborators": sorted(all_collaborators),
        "no_maintainer": no_maintainer,
        "no_collab": no_collab,
        "one_collab": one_collab,
        "two_plus_collab": two_plus_collab,
        "has_samples": has_samples,
        "has_tests": has_tests,
        "has_docs": has_docs,
        "has_test_ids": has_test_ids,
        "has_labels": has_labels,
        "health_dist": dict(health_dist),
        "top_maintainers": maintainer_area_count.most_common(20),
        "top_collaborators": collab_area_count.most_common(20),
        "multi_maintainer": multi_maintainer,
        "most_files": most_files,
        "worst_health": worst_health,
        "type_counts": dict(type_counts),
        "type_stats": type_stats,
        # file coverage
        "covered_files": len(global_covered) if global_covered is not None else 0,
        "total_repo_files": total_repo_files,
        "coverage_pct": (len(global_covered) / total_repo_files * 100) if total_repo_files else 0.0,
        # GitHub activity
        "activity": activity or {},
        "since_days": 365,  # overridden by main() after this call
    }


def health_badge(score, area_type="General"):
    label, cls = health_level(score, area_type)
    return f'<span class="badge {cls}">{label} ({score}/6)</span>'


TYPE_COLORS = {
    "General":      ("#eff6ff", "#1d4ed8", "#bfdbfe"),  # blue
    "Driver":       ("#f0fdf4", "#15803d", "#bbf7d0"),  # green
    "Platform":     ("#fdf4ff", "#7e22ce", "#e9d5ff"),  # purple
    "West Project": ("#fff7ed", "#c2410c", "#fed7aa"),  # orange
}


def type_badge(area_type):
    bg, fg, border = TYPE_COLORS.get(area_type, ("#f1f5f9", "#475569", "#e2e8f0"))
    return (f'<span class="badge" style="background:{bg};color:{fg};'
            f'border:1px solid {border};">{area_type}</span>')


def status_badge(status):
    cls = {
        "maintained": "badge-maintained",
        "odd fixes": "badge-odd",
        "obsolete": "badge-obsolete",
    }.get(status, "badge-unknown")
    return f'<span class="badge {cls}">{status}</span>'


def check_icon(val):
    return '<span class="check">✓</span>' if val else '<span class="cross">✗</span>'


def generate_html(areas, stats, trend_html="", prev_snapshot=None):
    """Generate the full HTML report."""
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    total_files = sum(a.get("file_count", 0) for a in areas)
    avg_health = sum(a["health_score"] for a in areas) / len(areas) if areas else 0
    covered_files    = stats.get("covered_files", 0)
    total_repo_files = stats.get("total_repo_files", 0)
    coverage_pct     = stats.get("coverage_pct", 0.0)
    activity         = stats.get("activity", {})
    since_days       = stats.get("since_days", 365)

    # Build per-person area lists
    maintainer_areas = defaultdict(list)
    for a in areas:
        for m in a["maintainers"]:
            maintainer_areas[m].append(a["name"])

    collab_areas = defaultdict(list)
    for a in areas:
        for c in a["collaborators"]:
            collab_areas[c].append(a["name"])

    # Build areas table rows
    area_rows = []
    for a in sorted(areas, key=lambda x: (x["area_type"], x["name"])):
        maintainers_str = ", ".join(
            f'<a href="#person-{m}" class="person-link">{m}</a>' for m in a["maintainers"]
        ) or '<em>None</em>'
        collaborators_str = ", ".join(
            f'<a href="#person-{c}" class="person-link">{c}</a>' for c in a["collaborators"]
        ) or '<em>None</em>'
        files_str = "<br>".join(f'<code>{f}</code>' for f in a["files"][:5])
        if len(a["files"]) > 5:
            files_str += f"<br><em>+{len(a['files'])-5} more</em>"
        if a["files_regex"]:
            files_str += "<br>" + "<br>".join(
                f'<code class="regex">{r}</code>' for r in a["files_regex"][:3]
            )
            if len(a["files_regex"]) > 3:
                files_str += f"<br><em>+{len(a['files_regex'])-3} more regex</em>"
        tests_str = ", ".join(f'<code>{t}</code>' for t in a["tests_ids"]) or ""
        labels_str = " ".join(
            f'<span class="label-tag">{l}</span>' for l in a["labels"]
        ) or ""

        area_id = re.sub(r'[^a-zA-Z0-9]', '-', a["name"])
        area_rows.append(f"""
        <tr id="area-{area_id}" class="area-row" data-status="{a['status']}" data-health="{a['health_score']}" data-type="{a['area_type']}">
          <td><a href="#area-{area_id}" class="area-link">{a['name']}</a></td>
          <td>{type_badge(a['area_type'])}</td>
          <td>{status_badge(a['status'])}</td>
          <td>{maintainers_str}</td>
          <td>{collaborators_str}</td>
          <td class="files-cell">{files_str}</td>
          <td class="center">{a.get('file_count', 0):,}</td>
          <td class="center">{check_icon(a['has_samples'])}</td>
          <td class="center">{check_icon(a['has_tests'])}</td>
          <td class="center">{check_icon(a['has_docs'])}</td>
          <td class="center">{check_icon(a['has_test_ids'])}</td>
          <td>{tests_str}</td>
          <td class="health-col">{health_badge(a['health_score'], a['area_type'])}</td>
        </tr>""")

    # People table rows (maintainers)
    def _activity_cell(username):
        if not activity:
            return ""
        info = activity.get(username)
        if info is None:
            return '<td><span class="badge badge-unknown">no data</span></td>'
        last = info.get("last_activity") or ""
        last_short = last[:10] if last else "never"
        if info.get("is_inactive"):
            return (f'<td><span class="badge badge-inactive" title="Last activity: {last_short}">'
                    f'Inactive ({last_short})</span></td>')
        return (f'<td><span class="badge badge-active" title="Last activity: {last_short}">'
                f'Active ({last_short})</span></td>')

    inactive_count = sum(1 for u, info in activity.items() if info.get("is_inactive")) if activity else 0

    people_rows = []
    for person in stats["all_maintainers"]:
        m_areas = maintainer_areas.get(person, [])
        c_areas = collab_areas.get(person, [])
        total_involvement = len(m_areas) + len(c_areas)
        m_links = ", ".join(
            f'<a href="#area-{re.sub(r"[^a-zA-Z0-9]", "-", a)}">{a}</a>'
            for a in m_areas[:5]
        )
        if len(m_areas) > 5:
            m_links += f" <em>+{len(m_areas)-5} more</em>"
        info = activity.get(person, {}) if activity else {}
        row_class = ' class="inactive-row"' if info.get("is_inactive") else ""
        people_rows.append(f"""
        <tr id="person-{person}"{row_class}>
          <td><strong>{person}</strong></td>
          <td class="center">{len(m_areas)}</td>
          <td class="center">{len(c_areas)}</td>
          <td class="center">{total_involvement}</td>
          {_activity_cell(person)}
          <td>{m_links or '<em>None</em>'}</td>
        </tr>""")

    # Worst health areas
    worst_rows = []
    for a in stats["worst_health"]:
        area_id = re.sub(r'[^a-zA-Z0-9]', '-', a["name"])
        issues = []
        if not a["has_maintainer"]: issues.append("No maintainer")
        if not a["has_two_collabs"]: issues.append(f"Only {a['num_collaborators']} collaborator(s)")
        if not a["has_samples"]: issues.append("No sample paths")
        if not a["has_tests"]: issues.append("No test paths")
        if not a["has_docs"]: issues.append("No doc paths")
        if not a["has_test_ids"]: issues.append("No test IDs")
        worst_rows.append(f"""
        <tr>
          <td><a href="#area-{area_id}">{a['name']}</a></td>
          <td>{type_badge(a['area_type'])}</td>
          <td>{status_badge(a['status'])}</td>
          <td>{health_badge(a['health_score'], a['area_type'])}</td>
          <td>{", ".join(issues)}</td>
        </tr>""")

    # Top maintainers table
    top_m_rows = "".join(
        f'<tr><td><a href="#person-{m}">{m}</a></td><td class="center">{cnt}</td></tr>'
        for m, cnt in stats["top_maintainers"]
    )

    # Top collaborators table
    top_c_rows = "".join(
        f'<tr><td>{c}</td><td class="center">{cnt}</td></tr>'
        for c, cnt in stats["top_collaborators"]
    )

    # Health distribution
    health_dist_rows = "".join(
        f'<tr><td>{score}/6</td><td class="center">{cnt}</td>'
        f'<td><div class="bar" style="width:{min(cnt*3,300)}px">&nbsp;</div></td></tr>'
        for score, cnt in sorted(stats["health_dist"].items())
    )

    # Status distribution bar chart data
    status_data = {
        "maintained": stats["maintained"],
        "odd fixes": stats["odd_fixes"],
        "obsolete": stats["obsolete"],
        "unknown": stats["unknown"],
    }

    def _delta(current_val, snap_key, lower_is_better=False):
        """Return a coloured arrow span comparing current_val to the previous run."""
        if prev_snapshot is None:
            return ""
        prev_val = prev_snapshot.get(snap_key)
        return _maintainers_delta_html(current_val, prev_val,
                                       lower_is_better=lower_is_better)

    if activity:
        _inactive_delta = _delta(inactive_count, "inactive_count",
                                 lower_is_better=True)
        inactive_card = (
            f'<div class="stat-card red">'
            f'<div class="number">{inactive_count}{_inactive_delta}</div>'
            f'<div class="label">Inactive Contributors (>{since_days}d)</div>'
            f'</div>'
        )
    else:
        inactive_card = ""

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Zephyr MAINTAINERS.yml Analysis Report</title>
<style>
  :root {{
    --primary: #2563eb;
    --primary-dark: #1d4ed8;
    --success: #16a34a;
    --warning: #d97706;
    --danger: #dc2626;
    --neutral: #6b7280;
    --bg: #f8fafc;
    --card-bg: #ffffff;
    --border: #e2e8f0;
    --text: #1e293b;
    --text-muted: #64748b;
    --code-bg: #f1f5f9;
  }}

  * {{ box-sizing: border-box; margin: 0; padding: 0; }}

  body {{
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: var(--bg);
    color: var(--text);
    font-size: 14px;
    line-height: 1.5;
  }}

  header {{
    background: linear-gradient(135deg, #1e3a5f 0%, #2563eb 100%);
    color: white;
    padding: 2rem;
    text-align: center;
  }}

  header h1 {{ font-size: 2rem; margin-bottom: 0.5rem; }}
  header p {{ opacity: 0.85; font-size: 1rem; }}

  nav {{
    background: #1e3a5f;
    padding: 0.5rem 2rem;
    display: flex;
    gap: 1rem;
    flex-wrap: wrap;
    position: sticky;
    top: 0;
    z-index: 100;
    box-shadow: 0 2px 8px rgba(0,0,0,0.3);
  }}

  nav a {{
    color: #93c5fd;
    text-decoration: none;
    padding: 0.25rem 0.75rem;
    border-radius: 4px;
    font-size: 0.85rem;
    transition: background 0.15s;
  }}

  nav a:hover {{ background: rgba(255,255,255,0.15); color: white; }}

  main {{
    max-width: 1600px;
    margin: 0 auto;
    padding: 1.5rem;
  }}

  section {{
    margin-bottom: 2.5rem;
  }}

  h2 {{
    font-size: 1.4rem;
    color: #1e3a5f;
    margin-bottom: 1rem;
    padding-bottom: 0.5rem;
    border-bottom: 2px solid var(--primary);
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }}

  h3 {{ font-size: 1.1rem; color: var(--text); margin-bottom: 0.75rem; }}

  /* Stats cards */
  .stats-grid {{
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));
    gap: 1rem;
    margin-bottom: 1.5rem;
  }}

  .stat-card {{
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 1.25rem;
    text-align: center;
    box-shadow: 0 1px 3px rgba(0,0,0,0.06);
    transition: box-shadow 0.2s;
  }}

  .stat-card:hover {{ box-shadow: 0 4px 12px rgba(0,0,0,0.1); }}

  .stat-card .number {{
    font-size: 2.2rem;
    font-weight: 700;
    line-height: 1;
    margin-bottom: 0.4rem;
  }}

  .stat-card .label {{
    font-size: 0.78rem;
    color: var(--text-muted);
    text-transform: uppercase;
    letter-spacing: 0.05em;
  }}

  .stat-card.blue .number {{ color: var(--primary); }}
  .stat-card.green .number {{ color: var(--success); }}
  .stat-card.orange .number {{ color: var(--warning); }}
  .stat-card.red .number {{ color: var(--danger); }}
  .stat-card.gray .number {{ color: var(--neutral); }}

  /* Highlight boxes */
  .highlights {{
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(320px, 1fr));
    gap: 1rem;
    margin-bottom: 2rem;
  }}

  .highlight-box {{
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 1rem 1.25rem;
    box-shadow: 0 1px 3px rgba(0,0,0,0.06);
  }}

  .highlight-box h3 {{
    font-size: 0.9rem;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-muted);
    margin-bottom: 0.75rem;
  }}

  /* Progress bars */
  .progress-row {{
    display: flex;
    align-items: center;
    gap: 0.75rem;
    margin-bottom: 0.5rem;
  }}

  .progress-label {{ width: 140px; font-size: 0.82rem; flex-shrink: 0; }}
  .progress-bar-bg {{
    flex: 1;
    height: 8px;
    background: #e2e8f0;
    border-radius: 4px;
    overflow: hidden;
  }}
  .progress-bar-fill {{
    height: 100%;
    border-radius: 4px;
    transition: width 0.3s;
  }}
  .progress-val {{ font-size: 0.8rem; color: var(--text-muted); width: 50px; text-align: right; }}

  /* Badges */
  .badge {{
    display: inline-block;
    padding: 0.15rem 0.55rem;
    border-radius: 9999px;
    font-size: 0.72rem;
    font-weight: 600;
    white-space: nowrap;
  }}

  .badge-maintained {{ background: #dcfce7; color: #15803d; }}
  .badge-odd {{ background: #fef9c3; color: #854d0e; }}
  .badge-obsolete {{ background: #fee2e2; color: #991b1b; }}
  .badge-unknown {{ background: #f1f5f9; color: #475569; }}

  .badge-excellent {{ background: #dcfce7; color: #15803d; }}
  .badge-good {{ background: #d1fae5; color: #065f46; }}
  .badge-fair {{ background: #fef9c3; color: #854d0e; }}
  .badge-poor {{ background: #fed7aa; color: #9a3412; }}
  .badge-critical {{ background: #fee2e2; color: #991b1b; }}
  .badge-info {{ background: #e0f2fe; color: #0369a1; }}
  .badge-active {{ background: #dcfce7; color: #15803d; }}
  .badge-inactive {{ background: #fee2e2; color: #991b1b; font-weight: 600; }}
  .inactive-row {{ background: #fff5f5 !important; }}

  .health-col {{
    white-space: nowrap;
    min-width: 150px;
  }}

  .label-tag {{
    display: inline-block;
    background: #eff6ff;
    color: #1d4ed8;
    border: 1px solid #bfdbfe;
    padding: 0.1rem 0.4rem;
    border-radius: 4px;
    font-size: 0.68rem;
    margin: 1px;
  }}

  /* Tables */
  .table-wrapper {{
    overflow-x: auto;
    border-radius: 8px;
    border: 1px solid var(--border);
    box-shadow: 0 1px 3px rgba(0,0,0,0.06);
  }}

  table {{
    width: 100%;
    border-collapse: collapse;
    background: var(--card-bg);
    font-size: 0.82rem;
  }}

  th {{
    background: #1e3a5f;
    color: white;
    padding: 0.6rem 0.75rem;
    text-align: left;
    font-weight: 600;
    font-size: 0.78rem;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    white-space: nowrap;
    cursor: pointer;
    user-select: none;
  }}

  th:hover {{ background: #2563eb; }}

  td {{
    padding: 0.5rem 0.75rem;
    border-bottom: 1px solid #f1f5f9;
    vertical-align: top;
  }}

  tr:last-child td {{ border-bottom: none; }}
  tr:nth-child(even) {{ background: #fafbfc; }}
  tr:hover {{ background: #eff6ff !important; }}

  .center {{ text-align: center; }}

  .check {{ color: var(--success); font-weight: bold; font-size: 1rem; }}
  .cross {{ color: #cbd5e1; font-size: 1rem; }}

  code {{
    background: var(--code-bg);
    padding: 0.1rem 0.35rem;
    border-radius: 3px;
    font-family: 'Courier New', monospace;
    font-size: 0.78rem;
    color: #be185d;
  }}

  code.regex {{ color: #7c3aed; }}

  .files-cell {{ max-width: 280px; word-break: break-all; }}

  .area-link, .person-link {{
    color: var(--primary);
    text-decoration: none;
    font-weight: 500;
  }}
  .area-link:hover, .person-link:hover {{ text-decoration: underline; }}

  /* Filter controls */
  .controls {{
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 0.75rem 1rem;
    margin-bottom: 1rem;
    display: flex;
    gap: 0.75rem;
    align-items: center;
    flex-wrap: wrap;
  }}

  .controls label {{ font-size: 0.82rem; color: var(--text-muted); }}

  .controls select, .controls input {{
    padding: 0.3rem 0.6rem;
    border: 1px solid var(--border);
    border-radius: 4px;
    font-size: 0.82rem;
    background: white;
    color: var(--text);
  }}

  .controls input {{ width: 220px; }}

  /* Bar chart */
  .bar {{
    display: inline-block;
    height: 14px;
    background: linear-gradient(90deg, #2563eb, #60a5fa);
    border-radius: 2px;
    min-width: 4px;
  }}

  /* Mini chart */
  .mini-chart {{
    display: flex;
    gap: 3px;
    align-items: flex-end;
    height: 60px;
    padding: 0.25rem 0;
  }}

  .mini-bar {{
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 2px;
  }}

  .mini-bar .bar-fill {{
    width: 36px;
    border-radius: 2px 2px 0 0;
  }}

  .mini-bar .bar-label {{
    font-size: 0.65rem;
    color: var(--text-muted);
    text-align: center;
  }}

  /* Two-column layout */
  .two-col {{
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 1.5rem;
  }}

  @media (max-width: 900px) {{
    .two-col {{ grid-template-columns: 1fr; }}
  }}

  /* Tooltip */
  [title] {{ cursor: help; border-bottom: 1px dotted var(--text-muted); }}

  /* Alert box */
  .alert {{
    padding: 0.75rem 1rem;
    border-radius: 6px;
    margin-bottom: 1rem;
    font-size: 0.85rem;
    border-left: 4px solid;
  }}
  .alert-warning {{ background: #fffbeb; border-color: #f59e0b; color: #92400e; }}
  .alert-info {{ background: #eff6ff; border-color: #3b82f6; color: #1e40af; }}

  /* Scrolltop */
  #scrolltop {{
    position: fixed;
    bottom: 1.5rem;
    right: 1.5rem;
    background: var(--primary);
    color: white;
    border: none;
    border-radius: 50%;
    width: 40px;
    height: 40px;
    font-size: 1.2rem;
    cursor: pointer;
    display: none;
    box-shadow: 0 4px 12px rgba(0,0,0,0.2);
    z-index: 999;
  }}

  #scrolltop:hover {{ background: var(--primary-dark); }}

  footer {{
    text-align: center;
    padding: 1.5rem;
    color: var(--text-muted);
    font-size: 0.8rem;
    border-top: 1px solid var(--border);
    margin-top: 2rem;
  }}
</style>
</head>
<body>

<header>
  <h1>&#128269; Zephyr MAINTAINERS.yml Analysis</h1>
  <p>Generated on {now} &bull; Repository: zephyrproject-rtos/zephyr &bull; Branch: topic/maintainer/analysis</p>
</header>

<nav>
  <a href="#highlights">&#127942; Highlights</a>
  <a href="#overview">&#128202; Overview</a>
  <a href="#by-type">&#127959; By Type</a>
  <a href="#health">&#10084; Health</a>
  <a href="#areas">&#128196; All Areas</a>
  <a href="#people">&#128101; People</a>
  <a href="#worst">&#9888; Needs Attention</a>
  <a href="#top-maintainers">&#127947; Top Maintainers</a>
  <a href="#top-collaborators">&#129309; Top Collaborators</a>
  <a href="#trend">&#128200; Trend</a>
</nav>

<main>

<!-- HIGHLIGHTS -->
<section id="highlights">
  <h2>&#127942; Key Highlights</h2>

  <div class="stats-grid">
    <div class="stat-card blue">
      <div class="number">{stats['total']}{_delta(stats['total'], 'total')}</div>
      <div class="label">Total Areas</div>
    </div>
    <div class="stat-card green">
      <div class="number">{stats['maintained']}{_delta(stats['maintained'], 'maintained')}</div>
      <div class="label">Maintained</div>
    </div>
    <div class="stat-card orange">
      <div class="number">{stats['odd_fixes']}{_delta(stats['odd_fixes'], 'odd_fixes')}</div>
      <div class="label">Odd Fixes</div>
    </div>
    <div class="stat-card blue" style="border-top:3px solid #1d4ed8;">
      <div class="number">{stats['type_counts'].get('General', 0)}{_delta(stats['type_counts'].get('General', 0), 'general_areas')}</div>
      <div class="label">General Areas</div>
    </div>
    <div class="stat-card green" style="border-top:3px solid #15803d;">
      <div class="number">{stats['type_counts'].get('Driver', 0)}{_delta(stats['type_counts'].get('Driver', 0), 'driver_areas')}</div>
      <div class="label">Driver Areas</div>
    </div>
    <div class="stat-card gray" style="border-top:3px solid #7e22ce;">
      <div class="number">{stats['type_counts'].get('Platform', 0)}{_delta(stats['type_counts'].get('Platform', 0), 'platform_areas')}</div>
      <div class="label">Platform Areas</div>
    </div>
    <div class="stat-card orange" style="border-top:3px solid #c2410c;">
      <div class="number">{stats['type_counts'].get('West Project', 0)}{_delta(stats['type_counts'].get('West Project', 0), 'west_areas')}</div>
      <div class="label">West Project Areas</div>
    </div>
    <div class="stat-card gray">
      <div class="number">{stats['unique_maintainers']}{_delta(stats['unique_maintainers'], 'unique_maintainers')}</div>
      <div class="label">Unique Maintainers</div>
    </div>
    <div class="stat-card blue">
      <div class="number">{stats['unique_collaborators']}{_delta(stats['unique_collaborators'], 'unique_collaborators')}</div>
      <div class="label">Unique Collaborators</div>
    </div>
    <div class="stat-card red">
      <div class="number">{stats['no_maintainer']}{_delta(stats['no_maintainer'], 'no_maintainer', lower_is_better=True)}</div>
      <div class="label">Areas Without Maintainer</div>
    </div>
    <div class="stat-card orange">
      <div class="number">{stats['no_collab']}{_delta(stats['no_collab'], 'no_collab', lower_is_better=True)}</div>
      <div class="label">Areas Without Collaborators</div>
    </div>
    <div class="stat-card green">
      <div class="number">{stats['has_test_ids']}{_delta(stats['has_test_ids'], 'has_test_ids')}</div>
      <div class="label">Areas With Test IDs</div>
    </div>
    <div class="stat-card blue">
      <div class="number">{covered_files:,}{_delta(covered_files, 'covered_files')}</div>
      <div class="label">Files Covered by Globs</div>
    </div>
    <div class="stat-card gray">
      <div class="number">{total_repo_files:,}{_delta(total_repo_files, 'total_repo_files')}</div>
      <div class="label">Total Repo Files</div>
    </div>
    <div class="stat-card {'green' if coverage_pct >= 70 else ('orange' if coverage_pct >= 40 else 'red')}">
      <div class="number">{coverage_pct:.1f}%{_delta(coverage_pct, 'coverage_pct')}</div>
      <div class="label">Repo Coverage</div>
    </div>
    {inactive_card}
    <div class="stat-card green">
      <div class="number">{avg_health:.1f}/6{_delta(avg_health, 'avg_health')}</div>
      <div class="label">Avg Health Score</div>
    </div>
  </div>
</section>

<!-- OVERVIEW -->
<section id="overview">
  <h2>&#128202; Overview &amp; Coverage</h2>

  <div class="highlights">
    <div class="highlight-box">
      <h3>Area Status Distribution</h3>
      <div class="progress-row">
        <span class="progress-label">Maintained</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{100*stats['maintained']//stats['total']}%; background:#16a34a;"></div>
        </div>
        <span class="progress-val">{stats['maintained']} ({100*stats['maintained']//stats['total']}%)</span>
      </div>
      <div class="progress-row">
        <span class="progress-label">Odd Fixes</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{100*stats['odd_fixes']//stats['total']}%; background:#d97706;"></div>
        </div>
        <span class="progress-val">{stats['odd_fixes']} ({100*stats['odd_fixes']//stats['total']}%)</span>
      </div>
      <div class="progress-row">
        <span class="progress-label">Obsolete</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{max(1, 100*stats['obsolete']//stats['total'])}%; background:#dc2626;"></div>
        </div>
        <span class="progress-val">{stats['obsolete']} ({100*stats['obsolete']//stats['total']}%)</span>
      </div>
      <div class="progress-row">
        <span class="progress-label">Unknown</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{max(1,100*stats['unknown']//stats['total'])}%; background:#6b7280;"></div>
        </div>
        <span class="progress-val">{stats['unknown']} ({100*stats['unknown']//stats['total']}%)</span>
      </div>
    </div>

    <div class="highlight-box">
      <h3>Collaborator Coverage</h3>
      <div class="progress-row">
        <span class="progress-label">2+ Collaborators</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{100*stats['two_plus_collab']//stats['total']}%; background:#2563eb;"></div>
        </div>
        <span class="progress-val">{stats['two_plus_collab']} ({100*stats['two_plus_collab']//stats['total']}%)</span>
      </div>
      <div class="progress-row">
        <span class="progress-label">1 Collaborator</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{100*stats['one_collab']//stats['total']}%; background:#60a5fa;"></div>
        </div>
        <span class="progress-val">{stats['one_collab']} ({100*stats['one_collab']//stats['total']}%)</span>
      </div>
      <div class="progress-row">
        <span class="progress-label">No Collaborators</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{100*stats['no_collab']//stats['total']}%; background:#dc2626;"></div>
        </div>
        <span class="progress-val">{stats['no_collab']} ({100*stats['no_collab']//stats['total']}%)</span>
      </div>
    </div>

    <div class="highlight-box">
      <h3>Area Documentation &amp; Test Coverage</h3>
      <div class="progress-row">
        <span class="progress-label">Has Samples</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{100*stats['has_samples']//stats['total']}%; background:#7c3aed;"></div>
        </div>
        <span class="progress-val">{stats['has_samples']} ({100*stats['has_samples']//stats['total']}%)</span>
      </div>
      <div class="progress-row">
        <span class="progress-label">Has Tests Paths</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{100*stats['has_tests']//stats['total']}%; background:#0891b2;"></div>
        </div>
        <span class="progress-val">{stats['has_tests']} ({100*stats['has_tests']//stats['total']}%)</span>
      </div>
      <div class="progress-row">
        <span class="progress-label">Has Doc Paths</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{100*stats['has_docs']//stats['total']}%; background:#059669;"></div>
        </div>
        <span class="progress-val">{stats['has_docs']} ({100*stats['has_docs']//stats['total']}%)</span>
      </div>
      <div class="progress-row">
        <span class="progress-label">Has Test IDs</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{100*stats['has_test_ids']//stats['total']}%; background:#d97706;"></div>
        </div>
        <span class="progress-val">{stats['has_test_ids']} ({100*stats['has_test_ids']//stats['total']}%)</span>
      </div>
      <div class="progress-row">
        <span class="progress-label">Has Labels</span>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" style="width:{100*stats['has_labels']//stats['total']}%; background:#16a34a;"></div>
        </div>
        <span class="progress-val">{stats['has_labels']} ({100*stats['has_labels']//stats['total']}%)</span>
      </div>
    </div>

    <div class="highlight-box">
      <h3>Health Score Distribution</h3>
      <table style="width:100%; border:none; box-shadow:none;">
        <tr><th style="background:#1e3a5f; color:white; padding:0.3rem 0.5rem;">Score</th>
            <th style="background:#1e3a5f; color:white; padding:0.3rem 0.5rem;">Areas</th>
            <th style="background:#1e3a5f; color:white; padding:0.3rem 0.5rem;">Distribution</th></tr>
        {health_dist_rows}
      </table>
      <p style="font-size:0.75rem; color:var(--text-muted); margin-top:0.5rem;">
        Score based on: maintainer, 2+ collabs, sample paths, test paths, doc paths, test IDs.
      </p>
    </div>
  </div>
</section>

</section>

<!-- BY TYPE -->
<section id="by-type">
  <h2>&#127959; Areas by Type</h2>
  <div class="alert alert-info">
    Areas are classified into four types. <strong>General</strong> and <strong>Driver</strong> areas use stricter health
    thresholds (Excellent &ge;5, Good &ge;4) because they are critical to the project.
    <strong>Platform</strong> areas use relaxed thresholds (Excellent &ge;4, Good &ge;3).
    <strong>West Project</strong> areas use the most relaxed thresholds (Excellent &ge;3).
  </div>
  <div class="table-wrapper">
    <table>
      <thead>
        <tr>
          <th>Type</th>
          <th>Count</th>
          <th>Maintained</th>
          <th>No Maintainer</th>
          <th>2+ Collabs</th>
          <th>Has Test IDs</th>
          <th>Has Docs</th>
          <th>Has Samples</th>
          <th>Avg Health</th>
          <th>Critical / Poor</th>
          <th>Health Thresholds</th>
        </tr>
      </thead>
      <tbody>
        {chr(10).join(
          f'''<tr>
            <td>{type_badge(atype)}</td>
            <td class="center">{ts["count"]}</td>
            <td class="center">{ts["maintained"]} ({ts["maintained"]*100//ts["count"] if ts["count"] else 0}%)</td>
            <td class="center" style="color:{'#dc2626' if ts['no_maintainer'] else 'inherit'}">{ts["no_maintainer"]}</td>
            <td class="center">{ts["two_plus_collab"]} ({ts["two_plus_collab"]*100//ts["count"] if ts["count"] else 0}%)</td>
            <td class="center">{ts["has_test_ids"]} ({ts["has_test_ids"]*100//ts["count"] if ts["count"] else 0}%)</td>
            <td class="center">{ts["has_docs"]} ({ts["has_docs"]*100//ts["count"] if ts["count"] else 0}%)</td>
            <td class="center">{ts["has_samples"]} ({ts["has_samples"]*100//ts["count"] if ts["count"] else 0}%)</td>
            <td class="center">{ts["avg_health"]:.1f}/6</td>
            <td class="center" style="color:{'#dc2626' if ts['critical'] else 'inherit'}">{ts["critical"]}</td>
            <td><small>Excellent&ge;{_HEALTH_THRESHOLDS[atype][0]}, Good&ge;{_HEALTH_THRESHOLDS[atype][1]}, Fair&ge;{_HEALTH_THRESHOLDS[atype][2]}, Poor&ge;{_HEALTH_THRESHOLDS[atype][3]}</small></td>
          </tr>'''
          for atype, ts in stats["type_stats"].items() if ts["count"] > 0
        )}
      </tbody>
    </table>
  </div>
</section>

<!-- HEALTH SUMMARY -->
<section id="health">
  <h2>&#10084; Health Criteria Summary</h2>
  <div class="alert alert-info">
    Health score is computed as the sum of 6 boolean criteria: has maintainer, has 2+ collaborators,
    has sample file paths, has test file paths, has documentation file paths, has test identifiers (tests: key).
  </div>
  <div class="two-col">
    <div>
      <h3>Criteria Breakdown</h3>
      <div class="table-wrapper">
        <table>
          <tr>
            <th>Criterion</th>
            <th>Passing Areas</th>
            <th>Failing Areas</th>
            <th>Pass Rate</th>
          </tr>
          <tr>
            <td>Has Maintainer</td>
            <td class="center">{stats['total'] - stats['no_maintainer']}</td>
            <td class="center" style="color:var(--danger)">{stats['no_maintainer']}</td>
            <td class="center">{100*(stats['total']-stats['no_maintainer'])//stats['total']}%</td>
          </tr>
          <tr>
            <td>Has 2+ Collaborators</td>
            <td class="center">{stats['two_plus_collab']}</td>
            <td class="center" style="color:var(--danger)">{stats['total'] - stats['two_plus_collab']}</td>
            <td class="center">{100*stats['two_plus_collab']//stats['total']}%</td>
          </tr>
          <tr>
            <td>Has Sample Paths</td>
            <td class="center">{stats['has_samples']}</td>
            <td class="center" style="color:var(--danger)">{stats['total'] - stats['has_samples']}</td>
            <td class="center">{100*stats['has_samples']//stats['total']}%</td>
          </tr>
          <tr>
            <td>Has Test Paths</td>
            <td class="center">{stats['has_tests']}</td>
            <td class="center" style="color:var(--danger)">{stats['total'] - stats['has_tests']}</td>
            <td class="center">{100*stats['has_tests']//stats['total']}%</td>
          </tr>
          <tr>
            <td>Has Doc Paths</td>
            <td class="center">{stats['has_docs']}</td>
            <td class="center" style="color:var(--danger)">{stats['total'] - stats['has_docs']}</td>
            <td class="center">{100*stats['has_docs']//stats['total']}%</td>
          </tr>
          <tr>
            <td>Has Test IDs (tests: key)</td>
            <td class="center">{stats['has_test_ids']}</td>
            <td class="center" style="color:var(--danger)">{stats['total'] - stats['has_test_ids']}</td>
            <td class="center">{100*stats['has_test_ids']//stats['total']}%</td>
          </tr>
        </table>
      </div>
    </div>
    <div>
      <h3>Areas Needing Most Improvement</h3>
      <div id="worst" style="scroll-margin-top:60px">
      <div class="table-wrapper">
        <table>
          <tr>
            <th>Area</th>
            <th>Type</th>
            <th>Status</th>
            <th>Health</th>
            <th>Missing</th>
          </tr>
          {"".join(worst_rows[:30])}
        </table>
      </div>
      </div>
    </div>
  </div>
</section>

<!-- ALL AREAS TABLE -->
<section id="areas">
  <h2>&#128196; All Areas ({stats['total']} total)</h2>

  <div class="controls">
    <label>Filter by type:</label>
    <select id="typeFilter" onchange="filterTable()">
      <option value="">All types</option>
      <option value="General">General</option>
      <option value="Driver">Driver</option>
      <option value="Platform">Platform</option>
      <option value="West Project">West Project</option>
    </select>
    <label>Filter by status:</label>
    <select id="statusFilter" onchange="filterTable()">
      <option value="">All</option>
      <option value="maintained">Maintained</option>
      <option value="odd fixes">Odd Fixes</option>
      <option value="obsolete">Obsolete</option>
    </select>
    <label>Min health:</label>
    <select id="healthFilter" onchange="filterTable()">
      <option value="0">Any</option>
      <option value="1">1+</option>
      <option value="2">2+</option>
      <option value="3">3+</option>
      <option value="4">4+</option>
      <option value="5">5+</option>
      <option value="6">Perfect (6)</option>
    </select>
    <label>Search:</label>
    <input type="text" id="searchFilter" oninput="filterTable()" placeholder="Area name...">
    <span id="rowCount" style="font-size:0.8rem; color:var(--text-muted); margin-left:auto;"></span>
  </div>

  <div class="table-wrapper">
    <table id="areasTable">
      <thead>
        <tr>
          <th onclick="sortTable(0)">Area &#8597;</th>
          <th onclick="sortTable(1)">Type &#8597;</th>
          <th onclick="sortTable(2)">Status &#8597;</th>
          <th>Maintainers</th>
          <th>Collaborators</th>
          <th>File Patterns</th>
          <th onclick="sortTable(6)">Files &#8597;</th>
          <th title="Has sample paths in file patterns">Samples</th>
          <th title="Has test paths in file patterns">Tests</th>
          <th title="Has doc paths in file patterns">Docs</th>
          <th title="Has tests: key with test identifiers">Test IDs</th>
          <th>Test Identifiers</th>
          <th onclick="sortTable(12)" style="min-width:150px;">Health &#8597;</th>
        </tr>
      </thead>
      <tbody id="areasBody">
        {"".join(area_rows)}
      </tbody>
    </table>
  </div>
</section>

<!-- PEOPLE -->
<section id="people">
  <h2>&#128101; People ({stats['unique_maintainers']} maintainers, {stats['unique_collaborators']} collaborators)</h2>

  <div class="two-col">
    <!-- Top Maintainers -->
    <div id="top-maintainers" style="scroll-margin-top:60px">
      <h3>&#127947; Top Maintainers (by area count)</h3>
      <div class="table-wrapper">
        <table>
          <tr><th>GitHub Handle</th><th>Areas as Maintainer</th></tr>
          {top_m_rows}
        </table>
      </div>
    </div>

    <!-- Top Collaborators -->
    <div id="top-collaborators" style="scroll-margin-top:60px">
      <h3>&#129309; Top Collaborators (by area count)</h3>
      <div class="table-wrapper">
        <table>
          <tr><th>GitHub Handle</th><th>Areas as Collaborator</th></tr>
          {top_c_rows}
        </table>
      </div>
    </div>
  </div>

  <br>
  <h3>All Maintainers &amp; Involvement</h3>
  {'<p><em>Activity data: last contribution in the zephyrproject-rtos/zephyr repo within ' + str(since_days) + ' days. ' + str(inactive_count) + ' contributor(s) marked inactive.</em></p>' if activity else ''}
  <div class="table-wrapper">
    <table>
      <thead>
        <tr>
          <th>GitHub Handle</th>
          <th>Areas as Maintainer</th>
          <th>Areas as Collaborator</th>
          <th>Total Involvement</th>
          {'<th>Activity</th>' if activity else ''}
          <th>Maintainer Areas</th>
        </tr>
      </thead>
      <tbody>
        {"".join(people_rows)}
      </tbody>
    </table>
  </div>
</section>

{trend_html}

</main>

<button id="scrolltop" onclick="window.scrollTo({{top:0,behavior:'smooth'}})">&#8679;</button>

<footer>
  Generated by maintainers_report.py &bull; Zephyr RTOS &bull; {now}
</footer>

<script>
function filterTable() {{
  const typeVal = document.getElementById('typeFilter').value;
  const status = document.getElementById('statusFilter').value.toLowerCase();
  const minHealth = parseInt(document.getElementById('healthFilter').value);
  const search = document.getElementById('searchFilter').value.toLowerCase();
  const rows = document.querySelectorAll('#areasBody tr');
  let visible = 0;
  rows.forEach(row => {{
    const rowStatus = row.dataset.status || '';
    const rowHealth = parseInt(row.dataset.health || '0');
    const rowType = row.dataset.type || '';
    const rowText = row.cells[0].textContent.toLowerCase();
    const show = (!typeVal || rowType === typeVal) &&
                 (!status || rowStatus === status) &&
                 rowHealth >= minHealth &&
                 (!search || rowText.includes(search));
    row.style.display = show ? '' : 'none';
    if (show) visible++;
  }});
  document.getElementById('rowCount').textContent = `Showing ${{visible}} of {stats['total']} areas`;
}}

function sortTable(colIndex) {{
  const tbody = document.getElementById('areasBody');
  const rows = Array.from(tbody.querySelectorAll('tr'));
  const dir = tbody.dataset.sortDir === 'asc' ? -1 : 1;
  tbody.dataset.sortDir = dir === 1 ? 'asc' : 'desc';
  rows.sort((a, b) => {{
    const aVal = a.cells[colIndex]?.textContent.trim() || '';
    const bVal = b.cells[colIndex]?.textContent.trim() || '';
    const aNum = parseFloat(aVal.replace(/[^0-9.]/g, ''));
    const bNum = parseFloat(bVal.replace(/[^0-9.]/g, ''));
    if (!isNaN(aNum) && !isNaN(bNum)) return (aNum - bNum) * dir;
    return aVal.localeCompare(bVal) * dir;
  }});
  rows.forEach(r => tbody.appendChild(r));
}}

// Scroll-to-top button
window.addEventListener('scroll', () => {{
  document.getElementById('scrolltop').style.display = window.scrollY > 300 ? 'block' : 'none';
}});

// Initial count
filterTable();
</script>

</body>
</html>"""

    return html


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
        print(f"WARNING: Could not load history file {path}: {exc}", flush=True)
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
        print(f"WARNING: Could not write history file {path}: {exc}", flush=True)


def _maintainers_delta_html(current, prev, lower_is_better=True):
    """Return a tiny coloured arrow span comparing *current* to *prev*."""
    if prev is None:
        return ""
    try:
        diff = current - prev
    except TypeError:
        return ""
    if diff == 0:
        return ' <span style="color:#6b7280;font-size:.7rem">—</span>'
    improving = (diff < 0) == lower_is_better
    color = "#16a34a" if improving else "#dc2626"
    arrow = "▼" if diff < 0 else "▲"
    if isinstance(current, float):
        label = f"{diff:+.1f}"
    else:
        label = f"{diff:+d}"
    return (f' <span style="color:{color};font-size:.7rem;font-weight:600">' +
            f'{arrow} {label}</span>')


def _maintainers_trend_table(history):
    """Render an HTML table summarising all historical snapshots (newest first)."""
    if len(history) < 2:
        return (
            '<p style="color:#64748b;font-size:.85rem;margin-top:8px;">' +
            'Trend data will appear here after two or more runs.</p>'
        )

    cols = [
        ("Date",              "generated",          None),
        ("Total Areas",       "total",               True),
        ("Maintained",        "maintained",          False),
        ("No Maintainer",     "no_maintainer",       True),
        ("Unique Maint.",     "unique_maintainers",  False),
        ("Unique Collab.",    "unique_collaborators",False),
        ("No Collab.",        "no_collab",            True),
        ("2+ Collab.",        "two_plus_collab",      False),
        ("Has Test IDs",      "has_test_ids",         False),
        ("Avg Health",        "avg_health",           False),
        ("Critical/Poor",     "critical_poor",        True),
        ("Repo Coverage %",   "coverage_pct",         False),
    ]

    th = "".join(
        f'<th style="background:#1e3a5f;color:#fff;padding:6px 10px;' +
        f'text-align:right;white-space:nowrap">{c[0]}</th>'
        for c in cols
    )
    th = th.replace('right', 'left', 1)  # first column left-aligned

    rows = []
    for i in range(len(history) - 1, -1, -1):
        snap = history[i]
        prev = history[i - 1] if i > 0 else None
        cells = []
        for _, key, lib in cols:
            val = snap.get(key, "")
            if lib is None:
                cells.append(f'<td style="padding:5px 10px;text-align:left;'
                             f'border-bottom:1px solid #e2e8f0;white-space:nowrap">' +
                             f'{val}</td>')
            else:
                dh = _maintainers_delta_html(val, prev.get(key) if prev else None,
                                             lower_is_better=lib)
                if isinstance(val, float):
                    cell_val = f"{val:.1f}"
                else:
                    cell_val = str(val)
                cells.append(f'<td style="padding:5px 10px;text-align:right;'
                             f'border-bottom:1px solid #e2e8f0;white-space:nowrap">' +
                             f'{cell_val}{dh}</td>')
        bg = "#f9fafb" if (len(history) - 1 - i) % 2 == 1 else "#ffffff"
        cells_html = "".join(cells)
        rows.append(f'<tr style="background:{bg}">{cells_html}</tr>')

    return (
        '<div style="overflow-x:auto">' +
        f'<table style="width:100%;border-collapse:collapse;font-size:.82rem">' +
        f'<thead><tr>{th}</tr></thead>' +
        f'<tbody>{"".join(rows)}</tbody>' +
        '</table></div>'
    )


# Chart series definitions: (label, snapshot_key, y_axis_id, border_color, bg_color)
_CHART_SERIES = [
    # --- Maintainership ---
    ("Total Areas",        "total",               "yLeft",  "#2563eb", "rgba(37,99,235,.15)"),
    ("Maintained",         "maintained",          "yLeft",  "#16a34a", "rgba(22,163,74,.15)"),
    ("No Maintainer",      "no_maintainer",       "yLeft",  "#dc2626", "rgba(220,38,38,.15)"),
    ("Unique Maintainers", "unique_maintainers",  "yLeft",  "#7c3aed", "rgba(124,58,237,.15)"),
    # --- Collaborators ---
    ("2+ Collaborators",   "two_plus_collab",     "yLeft",  "#0891b2", "rgba(8,145,178,.15)"),
    ("No Collaborators",   "no_collab",           "yLeft",  "#f59e0b", "rgba(245,158,11,.15)"),
    # --- Health ---
    ("Avg Health Score",   "avg_health",          "yRight", "#059669", "rgba(5,150,105,.15)"),
    ("Critical/Poor",      "critical_poor",       "yLeft",  "#be185d", "rgba(190,24,93,.15)"),
    # --- Coverage ---
    ("Repo Coverage %",    "coverage_pct",        "yRight", "#6366f1", "rgba(99,102,241,.15)"),
    # --- Test IDs ---
    ("Has Test IDs",       "has_test_ids",        "yLeft",  "#d97706", "rgba(217,119,6,.15)"),
]


def _maintainers_trend_charts(history):
    """Return an HTML block with Chart.js line charts built from *history*.

    Renders two charts side-by-side:
      * Left  -- absolute-count metrics (areas, maintainers, ...)
      * Right -- ratio/score metrics (avg health, coverage %)
    """
    if len(history) < 2:
        return ""

    labels = json.dumps([s.get("generated", s.get("timestamp", "")[:16])
                         for s in history])

    def _series_js(title, key, color, bg):
        vals = json.dumps([s.get(key) for s in history])
        return (
            '{' +
            f'label:{json.dumps(title)},data:{vals},' +
            f'borderColor:{json.dumps(color)},backgroundColor:{json.dumps(bg)},' +
            'borderWidth:2,pointRadius:3,pointHoverRadius:5,tension:0.35,fill:true'
            '}'
        )

    left_series  = [s for s in _CHART_SERIES if s[2] == "yLeft"]
    right_series = [s for s in _CHART_SERIES if s[2] == "yRight"]

    left_ds  = ",".join(_series_js(t, k, c, b) for t, k, _, c, b in left_series)
    right_ds = ",".join(_series_js(t, k, c, b) for t, k, _, c, b in right_series)

    chart_opts = (
        'responsive:true,'
        'interaction:{mode:"index",intersect:false},'
        'plugins:{legend:{position:"bottom",labels:{font:{size:11},boxWidth:12}}},'
        'scales:{x:{ticks:{maxRotation:45,font:{size:10}}},'
        'y:{beginAtZero:false,ticks:{font:{size:10}}}}'
    )

    return (
        '<div style="display:grid;grid-template-columns:1fr 1fr;gap:1.5rem;margin-top:1rem">\n'
        '  <div style="background:#fff;border:1px solid #e2e8f0;border-radius:8px;'
        'padding:1rem;box-shadow:0 1px 3px rgba(0,0,0,.06)">\n'
        '    <h3 style="font-size:.9rem;color:#1e3a5f;margin-bottom:.5rem">'
        'Area &amp; People Counts</h3>\n'
        '    <canvas id="chartLeft"></canvas>\n'
        '  </div>\n'
        '  <div style="background:#fff;border:1px solid #e2e8f0;border-radius:8px;'
        'padding:1rem;box-shadow:0 1px 3px rgba(0,0,0,.06)">\n'
        '    <h3 style="font-size:.9rem;color:#1e3a5f;margin-bottom:.5rem">'
        'Health &amp; Coverage</h3>\n'
        '    <canvas id="chartRight"></canvas>\n'
        '  </div>\n'
        '</div>\n'
        '<script src="https://cdn.jsdelivr.net/npm/chart.js@4/dist/chart.umd.min.js"></script>\n'
        '<script>\n'
        '(function(){\n'
        f'  var labels = {labels};\n'
        f'  new Chart(document.getElementById(\'chartLeft\'), {{\n'
        f'    type: \'line\',\n'
        f'    data: {{labels: labels, datasets: [{left_ds}]}},\n'
        f'    options: {{{chart_opts}}}\n'
        f'  }});\n'
        f'  new Chart(document.getElementById(\'chartRight\'), {{\n'
        f'    type: \'line\',\n'
        f'    data: {{labels: labels, datasets: [{right_ds}]}},\n'
        f'    options: {{{chart_opts}}}\n'
        f'  }});\n'
        '})();\n'
        '</script>\n'
    )


def main():
    parser = argparse.ArgumentParser(
        description="Analyze MAINTAINERS.yml and generate a browsable HTML report."
    )
    parser.add_argument("-o", "--output", default=str(OUTPUT_FILE),
                        help="Output HTML file path (default: %(default)s)")
    parser.add_argument(
        "--maintainers-file",
        default=None,
        metavar="FILE",
        help=(
            "Path to an alternative MAINTAINERS.yml file to analyse.  "
            "Defaults to MAINTAINERS.yml at the repository root."
        ),
    )
    activity_grp = parser.add_mutually_exclusive_group()
    activity_grp.add_argument(
        "--activity-maintainers", action="store_true",
        help="Query GitHub API for activity of maintainers only. "
             "Faster; skips collaborators. All checks are scoped to the zephyr repo.",
    )
    activity_grp.add_argument(
        "--activity-all", action="store_true",
        help="Query GitHub API for activity of both maintainers and collaborators. "
             "More complete but slower. All checks are scoped to the zephyr repo.",
    )
    parser.add_argument("--token", default=os.environ.get("GITHUB_TOKEN", ""),
                        metavar="TOKEN",
                        help="GitHub personal access token (or set GITHUB_TOKEN env var). "
                             "Without a token requests are limited to 60/hour.")
    parser.add_argument("--inactive-days", type=int, default=365, metavar="DAYS",
                        help="Number of days without activity before a contributor is "
                             "considered inactive (default: %(default)s).")
    parser.add_argument("--retry", action="store_true",
                        help="When the GitHub API rate limit is hit, automatically sleep "
                             "until the quota resets and resume, rather than stopping early.")
    parser.add_argument("--search-delay", type=float, default=1.0, metavar="SECS",
                        help="Minimum seconds between GitHub Search API calls to avoid "
                             "secondary rate limits (default: %(default)s). "
                             "Lower to 0 to disable.")
    parser.add_argument(
        "--history",
        default=None,
        metavar="FILE",
        help=(
            "Path to a JSON file used to persist run history.  If the file "
            "exists, previous snapshots are loaded and the report shows a "
            "trend table at the bottom comparing key metrics over time.  "
            "After rendering, the current run's statistics are appended to "
            "the file (created if it does not exist)."
        ),
    )
    args = parser.parse_args()

    output_file = Path(args.output)
    maintainers_file = Path(args.maintainers_file) if args.maintainers_file else MAINTAINERS_FILE
    if not maintainers_file.exists():
        print(f"ERROR: Maintainers file not found: {maintainers_file}")
        sys.exit(1)

    print(f"Loading {maintainers_file}...")
    data = load_maintainers(maintainers_file)
    if data is None:
        print(f"ERROR: Failed to parse {maintainers_file}")
        sys.exit(1)

    print(f"Parsed {len(data)} top-level entries")
    areas, all_maintainers, all_collaborators = analyze_areas(data)
    print(f"Found {len(areas)} areas, {len(all_maintainers)} unique maintainers, "
          f"{len(all_collaborators)} unique collaborators")

    areas, global_covered = count_actual_files(areas)

    print("Counting total repo files...")
    total_repo_files = count_total_repo_files(REPO_ROOT)
    coverage_pct = len(global_covered) / total_repo_files * 100 if total_repo_files else 0.0
    print(f"Repo files: {total_repo_files:,}  Covered by globs: {len(global_covered):,}  "
          f"({coverage_pct:.1f}%)")

    activity = {}
    if args.activity_maintainers or args.activity_all:
        if args.activity_maintainers:
            target_users = all_maintainers
            scope_label = "maintainers only"
        else:
            target_users = all_maintainers | all_collaborators
            scope_label = "maintainers + collaborators"
        print(f"\nFetching GitHub activity for {len(target_users)} users "
              f"({scope_label}, inactive threshold: {args.inactive_days} days)...")
        print(f"  Repo scope: {_GH_REPO}")
        if not args.token:
            print("  WARNING: No GitHub token provided. Unauthenticated requests are "
                  "limited to 60/hour and may be rate-limited quickly.")
        activity = fetch_activity(
            target_users, args.token, args.inactive_days,
            retry=args.retry,
            search_delay=args.search_delay,
        )
        inactive = sum(1 for info in activity.values() if info.get("is_inactive"))
        print(f"  Done. {inactive} inactive / {len(activity)} queried.")

    stats = compute_stats(areas, all_maintainers, all_collaborators,
                          global_covered=global_covered,
                          total_repo_files=total_repo_files,
                          activity=activity)
    # store since_days separately for generate_html
    stats["since_days"] = args.inactive_days

    print("\n=== SUMMARY ===")
    print(f"Total areas: {stats['total']}")
    print(f"  Maintained: {stats['maintained']}")
    print(f"  Odd fixes:  {stats['odd_fixes']}")
    print(f"  Obsolete:   {stats['obsolete']}")
    print(f"  Unknown:    {stats['unknown']}")
    print(f"Unique maintainers: {stats['unique_maintainers']}")
    print(f"Unique collaborators: {stats['unique_collaborators']}")
    print(f"Areas without maintainer: {stats['no_maintainer']}")
    print(f"Areas with 2+ collaborators: {stats['two_plus_collab']}")
    print(f"Areas with sample paths: {stats['has_samples']}")
    print(f"Areas with test paths: {stats['has_tests']}")
    print(f"Areas with doc paths: {stats['has_docs']}")
    print(f"Areas with test IDs: {stats['has_test_ids']}")

    print(f"\nGenerating HTML report: {output_file}")

    # ---- Build snapshot for history ----
    generated_ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    avg_health = (sum(a["health_score"] for a in areas) / len(areas)
                  if areas else 0.0)
    critical_poor = sum(
        1 for a in areas
        if health_level(a["health_score"], a["area_type"])[0] in ("Critical", "Poor")
    )
    inactive_count_snap = (
        sum(1 for info in activity.values() if info.get("is_inactive"))
        if activity else None
    )
    snapshot = {
        "timestamp": datetime.datetime.utcnow().isoformat(),
        "generated": generated_ts,
        "total": stats["total"],
        "maintained": stats["maintained"],
        "odd_fixes": stats["odd_fixes"],
        "no_maintainer": stats["no_maintainer"],
        "unique_maintainers": stats["unique_maintainers"],
        "unique_collaborators": stats["unique_collaborators"],
        "no_collab": stats["no_collab"],
        "two_plus_collab": stats["two_plus_collab"],
        "has_test_ids": stats["has_test_ids"],
        "avg_health": round(avg_health, 2),
        "critical_poor": critical_poor,
        "coverage_pct": round(stats.get("coverage_pct", 0.0), 1),
        "general_areas": stats["type_counts"].get("General", 0),
        "driver_areas": stats["type_counts"].get("Driver", 0),
        "platform_areas": stats["type_counts"].get("Platform", 0),
        "west_areas": stats["type_counts"].get("West Project", 0),
        "covered_files": stats.get("covered_files", 0),
        "total_repo_files": stats.get("total_repo_files", 0),
        "inactive_count": inactive_count_snap,
    }

    # ---- Load history and build trend HTML ----
    history = []
    if args.history:
        history = _load_history(args.history)
    all_runs = list(history) + [snapshot]
    if len(all_runs) >= 2:
        trend_html = (
            '<section id="trend">\n'
            '<h2>&#128200; MAINTAINERS.yml Trend History</h2>\n'
            '<p style="color:#64748b;font-size:.85rem;margin-bottom:12px;">'
            'Each row is one saved run. Arrows show change vs. the previous '
            'run; green\u202f=\u202fimproving, red\u202f=\u202fworsening.</p>\n'
            + _maintainers_trend_charts(all_runs)
            + '\n<h3 style="font-size:1rem;color:#1e3a5f;margin:1.5rem 0 .5rem">'
            'Full History Table</h3>\n'
            + _maintainers_trend_table(all_runs)
            + '\n</section>'
        )
    else:
        trend_html = ""

    prev_snapshot = history[-1] if history else None
    html = generate_html(areas, stats, trend_html=trend_html,
                         prev_snapshot=prev_snapshot)
    with open(output_file, "w") as f:
        f.write(html)
    print(f"Report written to: {output_file}")
    print(f"Open with: xdg-open {output_file}")

    # ---- Persist snapshot ----
    if args.history:
        _save_snapshot(args.history, snapshot, history)
        print(f"Snapshot appended to history file: {args.history}")


if __name__ == "__main__":
    main()
