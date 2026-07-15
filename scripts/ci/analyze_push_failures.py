#!/usr/bin/env python3
# Copyright (c) 2026 The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0
"""Analyze twister failures from a *push* run of the ``twister.yaml`` workflow.

The upstream ``Run tests with twister`` workflow shards the full test corpus
across ``PUSH_MATRIX_SIZE`` (25) build jobs on every push to ``main``.  Each job
uploads an artifact ``Unit Test Results (Subset N)`` containing
``twister-out/twister.json``.  This tool:

  1. Resolves a push run (``--run-id`` or the latest completed one on a branch).
  2. Downloads the per-subset ``twister.json`` artifacts.
  3. Extracts every ``failed`` / ``error`` testsuite.
  4. Computes the *batch range* the run tested (previous completed push head ..
     this run's head) and attributes each failure to a **culprit commit** and
     its **originating PR**.
  5. (``coverage`` subcommand) Re-runs ``scripts/ci/test_plan_v2.py`` over the
     culprit PR's commit range to determine *why* PR CI did not catch the
     regression, maps the changed files to their ``MAINTAINERS.yml`` areas to
     surface coverage blind spots, and proposes concrete fixes — either a
     ``tests:`` pattern in the maintainer file or an ``integration_platforms``
     entry in the failing test's ``tests.yaml``.

The analysis is data-driven (it reads ``MAINTAINERS.yml`` and the test
manifests), so it generalizes across subsystems rather than targeting one
incident.  It shells out to ``gh`` (authenticated) and ``git``.  Culprit
attribution is a heuristic ranking, not a rebuild-bisect: treat the top
candidate as a strong lead, not proof.

Examples
--------
    # Analyze the most recent completed push run on main
    ./scripts/ci/analyze_push_failures.py failures --latest

    # Analyze a specific run and emit a JSON report
    ./scripts/ci/analyze_push_failures.py failures --run-id 29366699680 \
        --out report.json

    # Diagnose why a culprit PR's own CI missed the failure and propose fixes
    ./scripts/ci/analyze_push_failures.py coverage --pr 111619 \
        --scenario sample.net.openthread.shell.xg24 \
        --platform xg24_rb4186c --path samples/net/openthread/shell
"""
from __future__ import annotations

import argparse
import fnmatch
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass, field
from pathlib import Path

try:
    import yaml
    from yaml import CSafeLoader as _YamlLoader
except ImportError:  # PyYAML not available - maintainer-based analysis degrades
    yaml = None
    _YamlLoader = None
except Exception:  # noqa: BLE001 - CSafeLoader missing, fall back to pure python
    _YamlLoader = yaml.SafeLoader

log = logging.getLogger("analyze_push_failures")

DEFAULT_REPO = "zephyrproject-rtos/zephyr"
WORKFLOW = "twister.yaml"
ARTIFACT_PREFIX = "Unit Test Results (Subset "
MAINTAINERS_FILE = "MAINTAINERS.yml"
TEST_MANIFESTS = ("testcase.yaml", "sample.yaml", "tests.yaml")


def _generic_source_hint(test_path: str) -> list[str]:
    """Map a test/sample directory to the source subtree it most likely
    exercises, with no hard-coded per-subsystem knowledge.

    ``tests/<subtree>/...`` and ``samples/<subtree>/...`` conventionally mirror
    the code they cover, so the first two path segments after the ``tests``/
    ``samples`` root are a reasonable source prefix (e.g. ``tests/subsys/shell``
    → ``subsys/shell``).  This is only a heuristic to widen the git-log culprit
    search; the authoritative mapping comes from :class:`MaintainerMap`.
    """
    parts = Path(test_path).parts
    if not parts or parts[0] not in ("tests", "samples"):
        return []
    body = parts[1:]
    if not body:
        return []
    # tests/subsys/foo -> subsys/foo ; tests/kernel/bar -> kernel/bar
    depth = min(len(body), 2)
    return ["/".join(body[:depth]) + "/"]


# ---------------------------------------------------------------------------
# Small shell helpers
# ---------------------------------------------------------------------------
def _run(cmd: list[str], cwd: str | None = None, check: bool = True) -> str:
    log.debug("exec: %s", " ".join(cmd))
    res = subprocess.run(
        cmd, cwd=cwd, check=check, text=True, capture_output=True
    )
    return res.stdout


def _gh_json(args: list[str]) -> object:
    return json.loads(_run(["gh", *args]))


def _git(args: list[str], cwd: str | None = None, check: bool = True) -> str:
    return _run(["git", *args], cwd=cwd, check=check)


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------
@dataclass
class Failure:
    subset: str
    platform: str
    scenario: str          # testsuite "name", e.g. sample.net.openthread.shell.xg24
    status: str            # failed | error
    reason: str
    path: str              # test source dir from twister.json
    failing_testcases: list[str] = field(default_factory=list)
    undefined_symbols: list[str] = field(default_factory=list)


@dataclass
class Culprit:
    sha: str
    subject: str
    author: str
    pr_number: int | None
    pr_title: str | None
    confidence: str        # high | medium | low
    rationale: str


# ---------------------------------------------------------------------------
# Run resolution & artifact download
# ---------------------------------------------------------------------------
def resolve_run(repo: str, run_id: str | None, branch: str, latest: bool) -> dict:
    if run_id:
        return _gh_json(
            ["run", "view", run_id, "--repo", repo, "--json",
             "databaseId,headSha,conclusion,createdAt,displayTitle"]
        )
    runs = _gh_json(
        ["run", "list", "--repo", repo, "--workflow", WORKFLOW,
         "--event", "push", "--branch", branch, "--limit", "40", "--json",
         "databaseId,headSha,conclusion,createdAt,displayTitle"]
    )
    completed = [r for r in runs if r["conclusion"] in ("success", "failure")]
    if not completed:
        sys.exit("No completed push runs found.")
    if latest:
        return completed[0]
    # default: the most recent *failed* run
    failed = [r for r in completed if r["conclusion"] == "failure"]
    return (failed or completed)[0]


def previous_push_run(repo: str, branch: str, before_iso: str) -> dict | None:
    """Closest completed push run *older* than ``before_iso`` (head + createdAt)."""
    runs = _gh_json(
        ["run", "list", "--repo", repo, "--workflow", WORKFLOW,
         "--event", "push", "--branch", branch, "--limit", "60", "--json",
         "headSha,conclusion,createdAt"]
    )
    older = [
        r for r in runs
        if r["conclusion"] in ("success", "failure") and r["createdAt"] < before_iso
    ]
    older.sort(key=lambda r: r["createdAt"], reverse=True)
    return older[0] if older else None


def failed_subsets(repo: str, run_id: str) -> list[str]:
    data = _gh_json(["run", "view", str(run_id), "--repo", repo, "--json", "jobs"])
    subsets = []
    for job in data.get("jobs", []):
        m = re.match(r"twister-build \((\d+)\)", job["name"])
        if m and job["conclusion"] == "failure":
            subsets.append(m.group(1))
    return subsets


def download_artifacts(repo: str, run_id: str, subsets: list[str], dest: Path) -> list[Path]:
    dest.mkdir(parents=True, exist_ok=True)
    cmd = ["gh", "run", "download", str(run_id), "--repo", repo, "--dir", str(dest)]
    for s in subsets:
        cmd += ["-p", f"{ARTIFACT_PREFIX}{s})"]
    _run(cmd, check=False)  # gh exits non-zero if some patterns match nothing
    return sorted(dest.glob("**/twister.json"))


# ---------------------------------------------------------------------------
# Failure extraction
# ---------------------------------------------------------------------------
_UNDEF_RE = re.compile(r"undefined (?:reference to|symbol:?) [`']?([A-Za-z_][A-Za-z0-9_]*)")


def parse_failures(json_path: Path) -> list[Failure]:
    subset = "?"
    m = re.search(r"Subset (\d+)", str(json_path))
    if m:
        subset = m.group(1)
    with open(json_path) as fh:
        data = json.load(fh)
    out: list[Failure] = []
    for ts in data.get("testsuites", []):
        if ts.get("status") not in ("failed", "error"):
            continue
        reason = ts.get("reason") or "Unknown reason"
        syms = _UNDEF_RE.findall(reason)
        log_txt = ts.get("log") or ""
        syms += _UNDEF_RE.findall(log_txt)
        failing_tc = [
            tc.get("identifier")
            for tc in ts.get("testcases", [])
            if tc.get("status") in ("failed", "error")
        ]
        out.append(Failure(
            subset=subset,
            platform=ts.get("platform") or "?",
            scenario=ts.get("name") or "?",
            status=ts.get("status"),
            reason=reason,
            path=ts.get("path") or "",
            failing_testcases=failing_tc,
            undefined_symbols=sorted(set(syms)),
        ))
    return out


# ---------------------------------------------------------------------------
# Culprit attribution
# ---------------------------------------------------------------------------
def _resolve_pr(repo: str, sha: str) -> tuple[int | None, str | None]:
    try:
        pulls = _gh_json(["api", f"repos/{repo}/commits/{sha}/pulls"])
    except subprocess.CalledProcessError:
        return None, None
    if pulls:
        return pulls[0]["number"], pulls[0]["title"]
    return None, None


def _commit_info(sha: str, cwd: str) -> tuple[str, str]:
    out = _git(["show", "-s", "--format=%s%x1f%an", sha], cwd=cwd).strip()
    subj, _, author = out.partition("\x1f")
    return subj, author


# ---------------------------------------------------------------------------
# MAINTAINERS.yml model — the authoritative file↔area↔tests mapping that
# test_plan_v2's MaintainerArea strategy consumes.  Everything here is derived
# from that data, so the analysis stays generic across subsystems.
# ---------------------------------------------------------------------------
class MaintainerMap:
    """Read ``MAINTAINERS.yml`` and answer file/area/tests questions.

    A test_plan_v2 ``tests:`` entry ``E`` selects any scenario matching
    ``^E(\\.|$)`` (the same regex the strategy builds), so scenario
    reachability here mirrors real selection behavior.
    """

    def __init__(self, zephyr_base: str):
        self.areas: dict[str, dict] = {}
        path = Path(zephyr_base) / MAINTAINERS_FILE
        if yaml is None:
            log.warning("PyYAML unavailable — maintainer-based analysis disabled.")
            return
        if not path.exists():
            log.warning("%s not found — maintainer-based analysis disabled.", path)
            return
        with open(path, encoding="utf-8") as fh:
            data = yaml.load(fh, Loader=_YamlLoader)
        self.areas = data if isinstance(data, dict) else {}

    @staticmethod
    def _file_matches(patterns, filepath: str) -> bool:
        for pat in patterns or []:
            if pat.endswith("/"):
                if filepath.startswith(pat):
                    return True
            elif "*" in pat or "?" in pat:
                if fnmatch.fnmatch(filepath, pat):
                    return True
            elif filepath == pat or filepath.startswith(pat + "/"):
                return True
        return False

    def areas_for_file(self, filepath: str) -> list[str]:
        out = []
        for name, area in self.areas.items():
            if not isinstance(area, dict):
                continue
            if self._file_matches(area.get("files"), filepath):
                out.append(name)
                continue
            for rx in area.get("files-regex", []) or []:
                if re.search(rx, filepath):
                    out.append(name)
                    break
        return out

    @staticmethod
    def _tests_match(entry: str, scenario: str) -> bool:
        return bool(re.match(rf"^{re.escape(entry)}(\.|$)", scenario))

    def tests_patterns(self, area_name: str) -> list[str]:
        area = self.areas.get(area_name, {})
        return list(area.get("tests", []) or []) if isinstance(area, dict) else []

    def area_reaches_scenario(self, area_name: str, scenario: str) -> bool:
        return any(self._tests_match(e, scenario) for e in self.tests_patterns(area_name))

    def source_files(self, area_name: str) -> list[str]:
        area = self.areas.get(area_name, {})
        return list(area.get("files", []) or []) if isinstance(area, dict) else []


def _source_hints(path: str, mmap: MaintainerMap | None = None,
                  scenario: str = "") -> list[str]:
    """Candidate source paths for the git-log culprit search.

    Prefer the authoritative maintainer mapping (the ``files:`` of any area
    whose ``tests:`` reach the failing scenario); fall back to the generic
    tests→source path convention.
    """
    hints: list[str] = []
    if mmap and scenario:
        for name, area in mmap.areas.items():
            if isinstance(area, dict) and mmap.area_reaches_scenario(name, scenario):
                hints += mmap.source_files(name)
    hints += _generic_source_hint(path)
    # de-dup, keep order
    seen, out = set(), []
    for h in hints:
        if h not in seen:
            seen.add(h)
            out.append(h)
    return out


def attribute_culprit(repo: str, failure: Failure, commit_range: str,
                      cwd: str, mmap: MaintainerMap | None = None) -> list[Culprit]:
    """Rank candidate culprit commits within ``commit_range`` for one failure."""
    candidates: dict[str, Culprit] = {}

    def add(sha: str, confidence: str, rationale: str):
        if sha in candidates:
            return
        subj, author = _commit_info(sha, cwd)
        pr, title = _resolve_pr(repo, sha)
        candidates[sha] = Culprit(sha[:12], subj, author, pr, title, confidence, rationale)

    # 1. Strongest signal: a build failure naming an undefined symbol.  The
    #    commit that *introduced the reference* is found with `git log -S`.
    for sym in failure.undefined_symbols:
        try:
            shas = _git(
                ["log", "-S", sym, "--format=%H", commit_range],
                cwd=cwd, check=False,
            ).split()
        except subprocess.CalledProcessError:
            shas = []
        for sha in shas:
            add(sha, "high", f"introduces reference to undefined symbol '{sym}' (git log -S)")

    # 2. Path-based: commits in range touching the test dir or the code it
    #    tests (source areas resolved from MAINTAINERS.yml when available).
    search_paths = [failure.path] if failure.path else []
    search_paths += _source_hints(failure.path, mmap, failure.scenario)
    if search_paths:
        shas = _git(
            ["log", "--format=%H", commit_range, "--", *search_paths],
            cwd=cwd, check=False,
        ).split()
        for sha in shas:
            add(sha, "medium", f"touches source area related to {failure.path or failure.scenario}")

    order = {"high": 0, "medium": 1, "low": 2}
    return sorted(candidates.values(), key=lambda c: order[c.confidence])


# ---------------------------------------------------------------------------
# `failures` subcommand
# ---------------------------------------------------------------------------
def cmd_failures(args) -> int:
    repo = args.repo
    zbase = args.zephyr_base
    run = resolve_run(repo, args.run_id, args.branch, args.latest)
    run_id = run.get("databaseId", args.run_id)
    head = run["headSha"]
    log.info("Run %s  head=%s  conclusion=%s  %s",
             run_id, head[:12], run.get("conclusion"), run.get("displayTitle", ""))

    # Ensure the head commit is available locally for git attribution.
    _git(["fetch", args.remote, head], cwd=zbase, check=False)

    prev_run = previous_push_run(repo, args.branch, run["createdAt"])
    prev_batch_range = None
    if prev_run:
        prev = prev_run["headSha"]
        _git(["fetch", args.remote, prev], cwd=zbase, check=False)
        commit_range = f"{prev}..{head}"
        # One batch further back, used as a fallback when a failure predates
        # this batch (a latent break or a flaky test surfacing now).
        prev2_run = previous_push_run(repo, args.branch, prev_run["createdAt"])
        if prev2_run:
            _git(["fetch", args.remote, prev2_run["headSha"]], cwd=zbase, check=False)
            prev_batch_range = f"{prev2_run['headSha']}..{prev}"
    else:
        commit_range = f"{head}~30..{head}"
    log.info("Batch range: %s (%s commits)", commit_range,
             len(_git(["log", "--oneline", commit_range], cwd=zbase, check=False).split("\n")))
    if prev_batch_range:
        log.info("Previous batch range (fallback): %s", prev_batch_range)

    subsets = args.subset or (failed_subsets(repo, run_id) if not args.all_subsets else
                              [str(i) for i in range(1, 26)])
    if not subsets:
        log.warning("No failed twister-build subsets reported; downloading all 25.")
        subsets = [str(i) for i in range(1, 26)]

    mmap = MaintainerMap(zbase)

    with tempfile.TemporaryDirectory(prefix="push_artifacts_") as tmp:
        jsons = download_artifacts(repo, run_id, subsets, Path(tmp))
        failures: list[Failure] = []
        for jp in jsons:
            failures += parse_failures(jp)

        report = []
        print(f"\n=== {len(failures)} failing testsuite(s) in run {run_id} ===\n")
        for f in failures:
            culprits = attribute_culprit(repo, f, commit_range, zbase, mmap)
            print(f"[{f.status}] {f.platform} :: {f.scenario}")
            print(f"    reason : {f.reason}")
            if f.failing_testcases:
                print(f"    tests  : {', '.join(f.failing_testcases)}")

            prev_culprits: list[Culprit] = []
            if not culprits:
                print("    culprit: <none found in this batch>")
                if prev_batch_range:
                    prev_culprits = attribute_culprit(repo, f, prev_batch_range, zbase, mmap)
                    if prev_culprits:
                        prev_base = prev_batch_range.split("..")[0][:12]
                        print(f"    ↳ found in the PREVIOUS batch ({prev_base}..) — likely"
                              " pre-existing (latent break or flaky test surfacing now):")
                        for c in prev_culprits[:2]:
                            pr = (f"PR #{c.pr_number} ({c.pr_title})"
                                  if c.pr_number else "PR: unknown")
                            print(f"        prev-batch[{c.confidence}]: "
                                  f"{c.sha} {c.subject}  {pr}")
                    else:
                        print("    ↳ also not in the previous batch. Re-run further back:"
                              "\n        analyze_push_failures.py failures"
                              " --run-id <older-run-id>")
                else:
                    print("    ↳ no previous batch available to check; try an older --run-id.")

            for c in culprits[:3]:
                pr = f"PR #{c.pr_number} ({c.pr_title})" if c.pr_number else "PR: unknown"
                print(f"    culprit[{c.confidence}]: {c.sha} {c.subject}")
                print(f"             {pr}")
                print(f"             ↳ {c.rationale}")
            print()

            report.append({"failure": asdict(f),
                           "culprits": [asdict(c) for c in culprits],
                           "previous_batch_culprits": [asdict(c) for c in prev_culprits]})

    if args.out:
        Path(args.out).write_text(json.dumps(
            {"run_id": run_id, "head": head, "range": commit_range, "failures": report},
            indent=2))
        log.info("Wrote %s", args.out)
    return 1 if failures else 0


# ---------------------------------------------------------------------------
# `coverage` subcommand — why did the PR's own CI miss it?
# ---------------------------------------------------------------------------
def _pr_merged_range(repo: str, pr: int, zbase: str, remote: str) -> tuple[str, list[str]]:
    """Return (commit_range, changed_files) for a merged PR, mapped onto the
    target branch.  Uses the PR's changed file set to locate the contiguous
    merged commit block on the base branch."""
    info = _gh_json(["pr", "view", str(pr), "--repo", repo, "--json",
                     "files,mergeCommit,baseRefName,title"])
    files = [f["path"] for f in info["files"]]
    merge = info.get("mergeCommit") or {}
    # Zephyr uses rebase-merge (linear history): the PR's commits land as a
    # contiguous block ending at the last merged commit.  Find them by matching
    # the changed-file set in recent history.
    tip = merge.get("oid")
    if not tip:
        sys.exit(f"Could not determine merged commit for PR #{pr}")
    _git(["fetch", remote, tip], cwd=zbase, check=False)
    # Walk back until the union of touched files stops matching the PR fileset.
    log_lines = _git(["log", "--format=%H", f"{tip}~40..{tip}"], cwd=zbase).split()
    fileset = set(files)
    block: list[str] = []
    for sha in log_lines:
        touched = set(_git(["show", "--name-only", "--format=", sha], cwd=zbase).split())
        if touched & fileset:
            block.append(sha)
    if not block:
        sys.exit("Could not locate PR commits on base branch.")
    base = f"{block[-1]}~1"
    return f"{base}..{tip}", files


def _board_of(platform: str) -> str:
    """Board name without the /variant suffix (tests.yaml uses board names)."""
    return platform.split("/")[0]


def _load_manifest(root: Path, test_path: str) -> tuple[str | None, dict]:
    """Load the tests/sample manifest for a test directory, if present.

    Returns the *repo-relative* manifest path (for display) and its parsed
    contents.
    """
    if not test_path or yaml is None:
        return None, {}
    for name in TEST_MANIFESTS:
        mf = root / test_path / name
        if mf.exists():
            rel = f"{test_path}/{name}"
            try:
                data = yaml.load(mf.read_text(encoding="utf-8"), Loader=_YamlLoader)
            except Exception as err:  # noqa: BLE001
                log.warning("Could not parse %s: %s", rel, err)
                return rel, {}
            return rel, (data if isinstance(data, dict) else {})
    return None, {}


def _scenario_entry(manifest: dict, scenario: str) -> dict:
    """Merged (common + per-test) config for a scenario key, if present."""
    tests = manifest.get("tests", {}) or {}
    entry = tests.get(scenario)
    if entry is None:
        return {}
    merged = dict(manifest.get("common", {}) or {})
    for k, v in entry.items():
        if k in ("platform_allow", "integration_platforms", "platform_exclude",
                 "tags", "depends_on") and isinstance(v, list):
            merged[k] = list(merged.get(k, [])) + list(v)
        else:
            merged[k] = v
    return merged


def _propose_maintainer_fix(mmap: MaintainerMap, files: list[str],
                            scenario: str) -> list[str]:
    """If no owning maintainer area's ``tests:`` reaches the scenario, suggest
    a concrete ``tests:`` pattern and the area(s) to add it to."""
    owning = sorted({a for f in files for a in mmap.areas_for_file(f)})
    if not owning:
        return [f"No MAINTAINERS.yml area claims the changed files "
                f"({', '.join(files)}). Add a section that lists these paths "
                f"under 'files:' and the relevant tests under 'tests:'."]
    if any(mmap.area_reaches_scenario(a, scenario) for a in owning):
        return []  # already reachable
    # Suggest a family prefix (drop the trailing board/variant qualifier).
    parts = scenario.split(".")
    suggestion = ".".join(parts[:3]) if len(parts) > 3 else scenario
    lines = [
        f"The failing scenario '{scenario}' is NOT reachable from the "
        f"'tests:' list of any area owning the changed files.",
        f"Add a pattern to MAINTAINERS.yml so a change to these files selects "
        f"this test. Candidate area(s): {', '.join(owning)}.",
        "",
        "    tests:",
        f"      - {suggestion}",
    ]
    return lines


def _propose_test_fix(entry: dict, manifest: str | None, scenario: str,
                      platform: str) -> list[str]:
    """If the failing board is allowed but not an integration platform, suggest
    adding it so PR CI (which runs with --integration) exercises it."""
    if not entry:
        return []
    board = _board_of(platform)
    allow = [_board_of(p) for p in entry.get("platform_allow", []) or []]
    integ = [_board_of(p) for p in entry.get("integration_platforms", []) or []]
    where = f" in {manifest}" if manifest else ""
    if board in integ:
        return []
    if allow and board not in allow:
        return [f"Board '{board}' is not in 'platform_allow' for '{scenario}'"
                f"{where}; the push build reaches it via default platforms but "
                f"a targeted PR run may not. Consider widening coverage."]
    if not integ:
        return [
            f"Scenario '{scenario}' defines no 'integration_platforms'{where}, "
            f"so in --integration (PR) mode it contributes no build for board "
            f"'{board}'. Add:",
            "",
            "    integration_platforms:",
            f"      - {board}",
        ]
    return [
        f"Board '{board}' builds '{scenario}' only outside integration mode. "
        f"Add it to 'integration_platforms'{where} so PR CI covers it:",
        "",
        "    integration_platforms:",
        *[f"      - {b}" for b in integ + [board]],
    ]


def _print_coverage_map(mmap: MaintainerMap, files: list[str],
                        selected_names: set[str]) -> None:
    """Generic 'what does CI cover for these files' view — surfaces blind
    spots (files whose owning area has no tests, or no selected test)."""
    print("\n--- coverage map (changed file → area → tests) ---")
    for f in files:
        areas = mmap.areas_for_file(f)
        if not areas:
            print(f"  {f}\n      ⚠ no MAINTAINERS.yml area owns this file")
            continue
        for a in areas:
            pats = mmap.tests_patterns(a)
            if not pats:
                note = "⚠ area has NO 'tests:' — blind spot"
            else:
                covered = any(
                    any(mmap._tests_match(e, n) for e in pats) for n in selected_names
                )
                note = "selected in plan" if covered else "has tests, none selected"
            print(f"  {f}\n      area '{a}': {note}")


def cmd_coverage(args) -> int:
    repo = args.repo
    zbase = args.zephyr_base
    commit_range, files = _pr_merged_range(repo, args.pr, zbase, args.remote)
    log.info("PR #%s range: %s", args.pr, commit_range)
    log.info("Changed files: %s", ", ".join(files))

    mmap = MaintainerMap(zbase)
    tip = commit_range.split("..")[-1]
    wt = Path(tempfile.mkdtemp(prefix="wt_cov_"))
    testplan = wt / "testplan.json"
    try:
        _git(["worktree", "add", "-f", "--detach", str(wt), tip], cwd=zbase)
        env = dict(os.environ, ZEPHYR_BASE=str(wt))
        cmd = [sys.executable, "scripts/ci/test_plan_v2.py",
               "-c", commit_range, "--disable-strategy", "BoilerplateFilter"]
        log.info("Running: %s", " ".join(cmd))
        proc = subprocess.run(cmd, cwd=str(wt), env=env, text=True,
                              capture_output=True)
        combined = proc.stdout + proc.stderr

        # A hardened test_plan_v2 fails closed (non-zero exit) when twister
        # crashes; older/fail-open versions swallowed it as "0 suites" and
        # still exited 0.  Detect both.  Optional-strategy import warnings
        # (lizard/pydriller) are expected and must not count as a crash.
        crashed = proc.returncode != 0 or bool(re.search(
            r"twister crashed|twister exited with code|"
            r"twister did not produce output|Test plan generation FAILED",
            combined))
        selected: list[dict] = []
        if testplan.exists():
            plan = json.loads(testplan.read_text())
            selected = plan.get("testsuites", [])
        selected_names = {t["name"] for t in selected}

        print(f"\n=== Coverage diagnosis for PR #{args.pr} ===")
        print(f"changed files : {len(files)}")
        print(f"test_plan_v2  : {'CRASHED (fail-closed exit)' if crashed else 'ran'}")
        print(f"suites planned: {len(selected)}")

        _print_coverage_map(mmap, files, selected_names)

        fixes: list[str] = []
        if args.scenario:
            hits = [t for t in selected if t["name"] == args.scenario
                    and (not args.platform or _board_of(args.platform)
                         in _board_of(t["platform"]) or args.platform in t["platform"])]
            plan_path = args.path or (selected and next(
                (t.get("path") for t in selected if t["name"] == args.scenario), None))
            print(f"\ntarget        : {args.platform or '*'} :: {args.scenario}")
            print(f"in plan       : {'YES' if hits else 'NO'}")

            print("\n--- verdict ---")
            if crashed:
                print("TOOLING (fail-open at PR time): the selector crashed and, on the "
                      "unpatched tool, was treated as '0 tests' — the PR could merge with "
                      "zero coverage. Fix the twister environment AND ensure the selector "
                      "fails closed (see test_plan_v2 TwisterExecutionError).")
            elif hits:
                print("SELECTED: the scenario/platform IS selectable from the changed "
                      "files. A PR-time miss then points to a stale base or a tooling "
                      "crash at PR time — compare with the PR's twister-build-prep log.")
            else:
                print("SELECTION GAP: enumeration succeeded but the failing scenario was "
                      "not selected for the changed files. Coverage fix needed below.")

            # --- concrete fix proposals -----------------------------------
            fixes += _propose_maintainer_fix(mmap, files, args.scenario)
            _mf, manifest = _load_manifest(wt, plan_path) if plan_path else (None, {})
            entry = _scenario_entry(manifest, args.scenario)
            if args.platform:
                fixes += _propose_test_fix(entry, _mf, args.scenario, args.platform)
            if not plan_path:
                fixes.append("Pass --path <test source dir> to enable test-manifest "
                             "(tests.yaml / integration_platforms) fix proposals.")
        else:
            print("\n(pass --scenario/--platform from a `failures` result for a targeted "
                  "diagnosis and concrete coverage-fix proposals.)")

        if fixes:
            print("\n=== proposed coverage fixes ===")
            for line in fixes:
                print(line if line == "" else f"  {line}")
        return 0
    finally:
        subprocess.run(["git", "worktree", "remove", "--force", str(wt)],
                       cwd=zbase, check=False)
        shutil.rmtree(wt, ignore_errors=True)


# ---------------------------------------------------------------------------
def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--repo", default=DEFAULT_REPO)
    p.add_argument("--zephyr-base", default=os.environ.get("ZEPHYR_BASE", os.getcwd()))
    p.add_argument("--remote", default="upstream",
                   help="git remote tracking the upstream repo (default: upstream)")
    p.add_argument("-v", "--verbose", action="store_true")
    sub = p.add_subparsers(dest="cmd", required=True)

    f = sub.add_parser("failures", help="find and attribute failures in a push run")
    f.add_argument("--run-id")
    f.add_argument("--branch", default="main")
    f.add_argument("--latest", action="store_true",
                   help="use latest completed run (default: latest failed)")
    f.add_argument("--all-subsets", action="store_true",
                   help="download all 25 subsets, not just failed jobs")
    f.add_argument("--subset", action="append", help="specific subset number(s)")
    f.add_argument("--out", help="write JSON report to this path")
    f.set_defaults(func=cmd_failures)

    c = sub.add_parser("coverage", help="diagnose why a PR's CI missed a failure")
    c.add_argument("--pr", type=int, required=True)
    c.add_argument("--scenario", help="failing testsuite name to check for in the plan")
    c.add_argument("--platform", help="failing platform to check for in the plan")
    c.add_argument("--path", help="test source dir of the failing scenario "
                                  "(e.g. samples/net/openthread/shell); enables "
                                  "tests.yaml / integration_platforms fix proposals")
    c.set_defaults(func=cmd_coverage)

    args = p.parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)-7s %(message)s",
    )
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
