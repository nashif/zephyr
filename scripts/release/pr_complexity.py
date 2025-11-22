#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
import math
from collections import defaultdict
from datetime import datetime, timedelta, timezone

import lizard
import requests
from pydriller import Repository

GITHUB_API = "https://api.github.com"


def run_cmd(cmd, cwd=None):
    """Run a shell command, raising on failure."""
    result = subprocess.run(
        cmd,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed: {' '.join(cmd)}\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
    return result.stdout.strip()


def ensure_git_repo(repo_path):
    """Ensure repo_path is a git repo."""
    try:
        out = run_cmd(["git", "rev-parse", "--is-inside-work-tree"], cwd=repo_path)
    except Exception as e:
        raise RuntimeError(f"{repo_path} is not a git repository (or git not installed): {e}")
    if out.strip() != "true":
        raise RuntimeError(f"{repo_path} is not inside a git work tree")


def get_remote_repo_name(repo_path, remote_name):
    """
    Try to infer owner/repo from a given remote (e.g. origin, upstream).
    Used only for sanity checks.
    """
    try:
        url = run_cmd(["git", "remote", "get-url", remote_name], cwd=repo_path)
    except Exception:
        return None

    url = url.strip()
    if url.endswith(".git"):
        url = url[:-4]

    if url.startswith("git@github.com:"):
        path = url[len("git@github.com:"):]
    elif url.startswith("https://github.com/"):
        path = url[len("https://github.com/"):]
    else:
        # Unknown format
        return None

    # path should now be "owner/repo"
    return path


def github_get(path, token=None, params=None):
    """Simple GitHub GET wrapper."""
    headers = {"Accept": "application/vnd.github+json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    url = f"{GITHUB_API}{path}"
    resp = requests.get(url, headers=headers, params=params)
    if resp.status_code != 200:
        raise RuntimeError(
            f"GitHub API request failed ({resp.status_code}): {url}\n{resp.text}"
        )
    return resp.json()


def get_pr_info(owner, repo, pr_number, token=None):
    """Fetch PR metadata and changed files via GitHub API."""
    pr = github_get(f"/repos/{owner}/{repo}/pulls/{pr_number}", token=token)
    base_sha = pr["base"]["sha"]
    base_ref = pr["base"]["ref"]  # e.g. 'main'
    head_sha = pr["head"]["sha"]

    print(f"[INFO] PR #{pr_number}: base_sha={base_sha}, base_ref={base_ref}, head_sha={head_sha}")

    # Fetch all files (paginated)
    files = []
    page = 1
    while True:
        page_files = github_get(
            f"/repos/{owner}/{repo}/pulls/{pr_number}/files",
            token=token,
            params={"per_page": 100, "page": page},
        )
        if not page_files:
            break
        files.extend(page_files)
        page += 1

    return base_sha, base_ref, head_sha, files


def get_pr_commits(owner, repo, pr_number, token=None):
    """Fetch list of commit SHAs that belong to the PR."""
    commits = []
    page = 1
    while True:
        page_commits = github_get(
            f"/repos/{owner}/{repo}/pulls/{pr_number}/commits",
            token=token,
            params={"per_page": 100, "page": page},
        )
        if not page_commits:
            break
        commits.extend(page_commits)
        page += 1

    shas = [c["sha"] for c in commits]
    return shas


def fetch_pr_refs(repo_path, remote_name, pr_number, base_ref):
    """
    Fetch just what we need from the specified remote:
    - Update base_ref from remote
    - Fetch PR head ref from remote using GitHub's pull/<n>/head syntax
    """
    print(f"[INFO] Fetching base branch '{base_ref}' from remote '{remote_name}'")
    run_cmd(["git", "fetch", remote_name, base_ref], cwd=repo_path)

    pr_head_ref = f"refs/remotes/{remote_name}/pr/{pr_number}/head"
    print(f"[INFO] Fetching PR head into {pr_head_ref}")
    run_cmd(
        [
            "git",
            "fetch",
            remote_name,
            f"pull/{pr_number}/head:{pr_head_ref}",
        ],
        cwd=repo_path,
    )

    # Return the refs we will checkout for analysis
    base_checkout_ref = f"{remote_name}/{base_ref}"
    head_checkout_ref = pr_head_ref
    return base_checkout_ref, head_checkout_ref


def checkout_ref(repo_path, ref):
    print(f"[INFO] Checking out {ref}")
    run_cmd(["git", "checkout", "--quiet", ref], cwd=repo_path)


def is_interesting_source_file(filename, exts=None):
    """Filter which files to analyze for complexity."""
    if exts is None:
        exts = {
            ".c", ".h", ".cpp", ".cc", ".cxx",
            ".py",
            ".java",
            ".js", ".ts",
        }
    _, ext = os.path.splitext(filename)
    return ext.lower() in exts


def calculate_file_complexity(file_path):
    """
    Compute total cyclomatic complexity of a single file using Lizard.

    Returns:
        total_cc (int), num_functions (int)
    """
    if not os.path.exists(file_path):
        return 0, 0

    analysis = lizard.analyze_file(file_path)
    total_cc = sum(f.cyclomatic_complexity for f in analysis.function_list)
    num_functions = len(analysis.function_list)
    return total_cc, num_functions


def compute_dmm_for_commits(repo_path, commit_shas):
    """
    Use PyDriller's OS-DMM implementation to compute DMM metrics
    for each commit in commit_shas.
    """
    if not commit_shas:
        return []

    dmm_results = []
    sha_set = set(commit_shas)

    for commit in Repository(path_to_repo=repo_path, only_commits=commit_shas).traverse_commits():
        if commit.hash not in sha_set:
            continue

        size = commit.dmm_unit_size
        complexity = commit.dmm_unit_complexity
        interfacing = commit.dmm_unit_interfacing
        vals = [v for v in (size, complexity, interfacing) if v is not None]
        overall = sum(vals) / len(vals) if vals else None

        dmm_results.append(
            {
                "hash": commit.hash,
                "message": commit.msg,
                "dmm_unit_size": size,
                "dmm_unit_complexity": complexity,
                "dmm_unit_interfacing": interfacing,
                "dmm_overall": overall,
            }
        )

    # Preserve PR commit ordering as given by GitHub
    order = {sha: i for i, sha in enumerate(commit_shas)}
    dmm_results.sort(key=lambda x: order.get(x["hash"], 1_000_000))
    return dmm_results


def summarize_dmm(dmm_results):
    """Compute simple aggregate stats over DMM results."""
    if not dmm_results:
        return {}

    overall_vals = [c["dmm_overall"] for c in dmm_results if c["dmm_overall"] is not None]
    if not overall_vals:
        return {}

    avg = sum(overall_vals) / len(overall_vals)
    minimum = min(overall_vals)
    maximum = max(overall_vals)

    def count_band(lo, hi):
        return sum(1 for v in overall_vals if lo <= v < hi)

    bands = {
        "high_risk_commits": count_band(0.0, 0.3),
        "moderate_risk_commits": count_band(0.3, 0.7),
        "low_risk_commits": count_band(0.7, 1.01),
        "total_commits_with_dmm": len(overall_vals),
    }

    return {
        "avg_dmm_overall": avg,
        "min_dmm_overall": minimum,
        "max_dmm_overall": maximum,
        **bands,
    }


# ------------ Extra risk component calculators ------------

def is_public_header(filename):
    return filename.startswith("include/") and filename.endswith((".h", ".hpp"))


def compute_interface_risk(file_churn):
    """
    Heuristic: treat churn in public headers under include/ as interface risk.
    Higher header churn and header removals => higher risk.
    """
    header_churn = 0
    removed_headers = 0

    for fname, info in file_churn.items():
        if not is_public_header(fname):
            continue
        header_churn += info["additions"] + info["deletions"]
        if info["status"] == "removed":
            removed_headers += 1

    if header_churn == 0 and removed_headers == 0:
        return 0.0

    # soft caps
    churn_scale = 500.0
    removed_scale = 3.0

    risk_churn = max(0.0, min(1.0, header_churn / churn_scale))
    risk_removed = max(0.0, min(1.0, removed_headers / removed_scale))

    # Take the worst of the two signals
    return max(risk_churn, risk_removed)


def classify_path(filename):
    """Rough category for path-based risk."""
    core_prefixes = (
        "kernel/", "arch/", "drivers/", "include/zephyr/", "soc/",
    )
    test_prefixes = (
        "tests/", "samples/", "test/", "testing/",
    )
    doc_prefixes = (
        "doc/", "docs/", "documentation/",
    )

    for p in core_prefixes:
        if filename.startswith(p):
            return "core"
    for p in test_prefixes:
        if filename.startswith(p):
            return "tests"
    for p in doc_prefixes:
        if filename.startswith(p):
            return "docs"
    return "other"


def compute_path_risk(file_churn):
    """
    Risk based on where churn happens:
      - Core churn increases risk.
      - Test churn mitigates risk a bit.
    """
    churn_core = churn_tests = churn_docs = churn_other = 0

    for fname, info in file_churn.items():
        churn = info["additions"] + info["deletions"]
        if churn == 0:
            continue
        cat = classify_path(fname)
        if cat == "core":
            churn_core += churn
        elif cat == "tests":
            churn_tests += churn
        elif cat == "docs":
            churn_docs += churn
        else:
            churn_other += churn

    total_churn = churn_core + churn_tests + churn_docs + churn_other
    if total_churn == 0:
        return 0.0

    core_ratio = churn_core / total_churn
    test_ratio = churn_tests / total_churn

    # risk: more core changes, less test changes => higher risk
    test_mitigation = min(test_ratio * 2.0, 1.0)  # tests can halve the risk
    risk = core_ratio * (1.0 - test_mitigation)
    return max(0.0, min(1.0, risk))


def compute_entropy_risk(file_churn):
    """
    Shannon entropy of churn distribution across files.
    Dispersion in [0,1], higher = more scattered = more risk.
    """
    changes = []
    for info in file_churn.values():
        c = info["additions"] + info["deletions"]
        if c > 0:
            changes.append(c)

    n = len(changes)
    if n <= 1:
        return 0.0

    total = float(sum(changes))
    probs = [c / total for c in changes]
    H = -sum(p * math.log2(p) for p in probs if p > 0)
    H_max = math.log2(n)
    if H_max == 0:
        return 0.0

    return max(0.0, min(1.0, H / H_max))


def compute_fragility_risk(repo_path, changed_files, days=180):
    """
    Hotspot / fragility risk: how often have these files changed recently?
    Uses PyDriller over the last `days` days.
    """
    if not changed_files:
        return 0.0

    since = datetime.now(timezone.utc) - timedelta(days=days)
    counts = {f: 0 for f in changed_files}

    for commit in Repository(path_to_repo=repo_path, since=since).traverse_commits():
        for m in commit.modified_files:
            paths = set(filter(None, [m.new_path, m.old_path]))
            for p in paths:
                if p in counts:
                    counts[p] += 1

    if not counts:
        return 0.0

    # Normalize: >=20 touches in window => considered fully "hot"
    vals = [min(1.0, c / 20.0) for c in counts.values()]
    if not vals:
        return 0.0

    return sum(vals) / len(vals)


def compute_semantic_risk(dmm_commits, dmm_overall):
    """
    Risk based on semantic type inferred from commit messages.
    Very rough heuristic:
      - Refactor/cleanup dominated, with good DMM -> lower risk (~0.3)
      - Mostly fixes -> moderate (~0.6)
      - Mostly feature additions -> neutral (~0.5)
    """
    if not dmm_commits:
        return 0.5

    msgs = [c["message"].lower() for c in dmm_commits]
    total = len(msgs)

    def count_tokens(tokens):
        return sum(1 for m in msgs if any(t in m for t in tokens))

    ref_tokens = ["refactor", "cleanup", "clean up", "tidy", "format"]
    fix_tokens = ["fix", "bug", "regression", "issue", "hotfix"]
    feat_tokens = ["add ", "adds ", "implement", "introduce", "support", "feature"]

    n_ref = count_tokens(ref_tokens)
    n_fix = count_tokens(fix_tokens)
    n_feat = count_tokens(feat_tokens)

    if total == 0:
        return 0.5

    frac_ref = n_ref / total
    frac_fix = n_fix / total
    frac_feat = n_feat / total

    avg_dmm = None
    if dmm_overall and "avg_dmm_overall" in dmm_overall:
        avg_dmm = dmm_overall["avg_dmm_overall"]

    # default neutral
    risk = 0.5

    if frac_ref > 0.5 and (avg_dmm is None or avg_dmm >= 0.6):
        risk = 0.3
    elif frac_fix > 0.5:
        risk = 0.6
    elif frac_feat > 0.5:
        risk = 0.5

    return max(0.0, min(1.0, risk))


# ------------ Final PR risk score aggregation ------------

def compute_pr_risk_score(overall, dmm_overall, extras=None):
    """
    Combine base components (DMM, ΔCC, churn) and optional extras into a
    single risk score in [0, 1]. LOWER score = LESS risk.
    """
    if extras is None:
        extras = {}

    risks = {}
    weights = {}

    # ---- DMM-based risk ----
    if dmm_overall and "avg_dmm_overall" in dmm_overall:
        avg_dmm = max(0.0, min(1.0, dmm_overall["avg_dmm_overall"]))
        risk_dmm = 1.0 - avg_dmm
        risks["dmm"] = risk_dmm
        weights["dmm"] = 0.5
    else:
        # If no DMM, drop it; base will be CC+churn (+extras)
        pass

    # ---- ΔCC-based risk ----
    delta_cc = overall.get("total_delta_cc", 0)
    delta_cc_pos = max(0, delta_cc)
    cc_scale = 200.0
    risk_cc = max(0.0, min(1.0, delta_cc_pos / cc_scale))
    risks["cc"] = risk_cc
    weights["cc"] = 0.3

    # ---- Churn-based risk ----
    churn = overall.get("total_additions", 0) + overall.get("total_deletions", 0)
    churn_scale = 2000.0
    risk_churn = max(0.0, min(1.0, churn / churn_scale))
    risks["churn"] = risk_churn
    weights["churn"] = 0.2

    # If DMM is missing entirely, boost CC+churn
    if "dmm" not in risks:
        weights["cc"] = 0.6
        weights["churn"] = 0.4

    # ---- Extras (optional) ----
    extra_weights_default = {
        "iface": 0.2,
        "path": 0.15,
        "entropy": 0.1,
        "fragility": 0.2,
        "semantic": 0.1,
    }

    for name, value in extras.items():
        if value is None:
            continue
        value = max(0.0, min(1.0, float(value)))
        risks[name] = value
        weights[name] = extra_weights_default.get(name, 0.1)

    # ---- Final weighted average ----
    total_w = sum(w for k, w in weights.items() if k in risks and w > 0.0)
    if total_w == 0:
        score = 0.5  # fallback
    else:
        score = sum(risks[k] * weights[k] for k in risks) / total_w
        score = max(0.0, min(1.0, score))

    components = {
        "risks": risks,
        "weights": weights,
    }
    return score, components


# ------------ Main PR analysis ------------

def analyze_pr_complexity(
    owner,
    repo,
    pr_number,
    repo_path,
    remote_name,
    token=None,
    enable_interface=False,
    enable_path=False,
    enable_entropy=False,
    enable_fragility=False,
    enable_semantic=False,
):
    # Basic sanity check
    ensure_git_repo(repo_path)
    remote_path = get_remote_repo_name(repo_path, remote_name)
    if remote_path and remote_path.lower() != f"{owner}/{repo}".lower():
        print(
            f"[WARN] Remote '{remote_name}' appears to be '{remote_path}', "
            f"but you passed '{owner}/{repo}'. Continuing anyway..."
        )

    # Gather PR info via GitHub
    base_sha, base_ref, head_sha, pr_files = get_pr_info(owner, repo, pr_number, token=token)
    pr_commit_shas = get_pr_commits(owner, repo, pr_number, token=token)

    # Fetch/update the necessary refs locally (no clone)
    base_checkout_ref, head_checkout_ref = fetch_pr_refs(
        repo_path, remote_name, pr_number, base_ref
    )

    # Map filename -> churn info from GitHub
    file_churn = {}
    for f in pr_files:
        filename = f["filename"]
        file_churn[filename] = {
            "status": f["status"],
            "additions": f["additions"],
            "deletions": f["deletions"],
            "changes": f["changes"],
        }

    # Results: filename -> side -> metrics
    metrics = defaultdict(lambda: {"base": None, "head": None})

    # Analyze base revision
    checkout_ref(repo_path, base_checkout_ref)
    for filename in file_churn.keys():
        if not is_interesting_source_file(filename):
            continue
        full_path = os.path.join(repo_path, filename)
        cc, funcs = calculate_file_complexity(full_path)
        metrics[filename]["base"] = {"cc": cc, "functions": funcs}

    # Analyze head revision (PR head)
    checkout_ref(repo_path, head_checkout_ref)
    for filename in file_churn.keys():
        if not is_interesting_source_file(filename):
            continue
        full_path = os.path.join(repo_path, filename)
        cc, funcs = calculate_file_complexity(full_path)
        metrics[filename]["head"] = {"cc": cc, "functions": funcs}

    # Compute per-file and overall stats
    summary = []
    total_base_cc = total_head_cc = 0
    total_additions = total_deletions = 0

    for filename, churn in file_churn.items():
        if not is_interesting_source_file(filename):
            total_additions += churn["additions"]
            total_deletions += churn["deletions"]
            continue

        base_info = metrics[filename]["base"]
        head_info = metrics[filename]["head"]

        base_cc = base_info["cc"] if base_info else 0
        head_cc = head_info["cc"] if head_info else 0
        delta_cc = head_cc - base_cc

        total_base_cc += base_cc
        total_head_cc += head_cc
        total_additions += churn["additions"]
        total_deletions += churn["deletions"]

        summary.append(
            {
                "file": filename,
                "status": churn["status"],
                "base_cc": base_cc,
                "head_cc": head_cc,
                "delta_cc": delta_cc,
                "functions_base": base_info["functions"] if base_info else 0,
                "functions_head": head_info["functions"] if head_info else 0,
                "additions": churn["additions"],
                "deletions": churn["deletions"],
            }
        )

    overall = {
        "total_base_cc": total_base_cc,
        "total_head_cc": total_head_cc,
        "total_delta_cc": total_head_cc - total_base_cc,
        "total_additions": total_additions,
        "total_deletions": total_deletions,
        "base_sha": base_sha,
        "head_sha": head_sha,
        "remote": remote_name,
    }

    # ---- Extra risks that only depend on file_churn / repo ----
    extras = {}
    if enable_interface:
        extras["iface"] = compute_interface_risk(file_churn)
    if enable_path:
        extras["path"] = compute_path_risk(file_churn)
    if enable_entropy:
        extras["entropy"] = compute_entropy_risk(file_churn)
    if enable_fragility:
        extras["fragility"] = compute_fragility_risk(repo_path, list(file_churn.keys()))

    # ---- DMM (OS-DMM) analysis using PyDriller ----
    dmm_commits = compute_dmm_for_commits(repo_path, pr_commit_shas)
    dmm_overall = summarize_dmm(dmm_commits)

    # semantic risk needs commits + maybe DMM
    if enable_semantic:
        extras["semantic"] = compute_semantic_risk(dmm_commits, dmm_overall)

    # ---- Final risk score ----
    pr_risk_score, risk_components = compute_pr_risk_score(overall, dmm_overall, extras)
    overall["pr_risk_score"] = pr_risk_score
    overall["pr_risk_components"] = risk_components

    return summary, overall, dmm_commits, dmm_overall


def print_human_readable(summary, overall, dmm_commits, dmm_overall):
    # --- Per-file complexity ---
    print("\n=== Per-file complexity (Lizard cyclomatic) ===")
    if not summary:
        print("No interesting source files in this PR.")
    else:
        header = (
            f"{'FILE':60}  {'ST':3}  {'BASE_CC':8}  {'HEAD_CC':8}  "
            f"{'ΔCC':6}  {'ADD':5}  {'DEL':5}"
        )
        print(header)
        print("-" * len(header))

        for entry in summary:
            print(
                f"{entry['file'][:60]:60}  "
                f"{entry['status'][:3]:3}  "
                f"{entry['base_cc']:8d}  "
                f"{entry['head_cc']:8d}  "
                f"{entry['delta_cc']:6d}  "
                f"{entry['additions']:5d}  "
                f"{entry['deletions']:5d}"
            )

    # --- Overall CC / churn ---
    print("\n=== Overall PR complexity summary ===")
    print(f"Remote:           {overall['remote']}")
    print(f"Base SHA:         {overall['base_sha']}")
    print(f"Head SHA:         {overall['head_sha']}")
    print(f"Total base CC:    {overall['total_base_cc']}")
    print(f"Total head CC:    {overall['total_head_cc']}")
    print(f"Total Δ CC:       {overall['total_delta_cc']}")
    print(f"Total additions:  {overall['total_additions']}")
    print(f"Total deletions:  {overall['total_deletions']}")
    print("NOTE: CC = sum of cyclomatic complexity over all functions in analyzed files.")

    # --- DMM (per-commit + summary) ---
    print("\n=== Delta Maintainability (OS-DMM via PyDriller) ===")
    if not dmm_commits or not dmm_overall:
        print("DMM metrics not available for these commits (unsupported languages or no data).")
    else:
        print("\nPer-commit DMM (0.0 = high risk change, 1.0 = low risk change):")
        dh = (
            f"{'HASH':10}  {'DMM_OVR':8}  "
            f"{'SIZE':8}  {'CMPLX':8}  {'IFACE':8}  MESSAGE"
        )
        print(dh)
        print("-" * len(dh))

        for c in dmm_commits:
            def fmt(v):
                return f"{v:.3f}" if isinstance(v, (int, float)) else "   N/A "
            print(
                f"{c['hash'][:10]:10}  "
                f"{fmt(c['dmm_overall']):8}  "
                f"{fmt(c['dmm_unit_size']):8}  "
                f"{fmt(c['dmm_unit_complexity']):8}  "
                f"{fmt(c['dmm_unit_interfacing']):8}  "
                f"{c['message'].splitlines()[0][:60]}"
            )

        print("\nDMM summary for this PR:")
        print(f"Avg DMM overall:       {dmm_overall['avg_dmm_overall']:.3f}")
        print(f"Min / Max DMM overall: {dmm_overall['min_dmm_overall']:.3f} "
              f"/ {dmm_overall['max_dmm_overall']:.3f}")
        print(f"Commits analyzed:      {dmm_overall['total_commits_with_dmm']}")
        print(f"High-risk (<=0.3):     {dmm_overall['high_risk_commits']}")
        print(f"Moderate (0.3–0.7):    {dmm_overall['moderate_risk_commits']}")
        print(f"Low-risk (>=0.7):      {dmm_overall['low_risk_commits']}")
        print(
            "\nInterpretation: Higher DMM values reflect changes that improve or "
            "preserve maintainability (smaller, less complex units with lean interfaces). "
            "Lower values highlight riskier changes."
        )

    # --- Final risk score at the bottom ---
    score = overall.get("pr_risk_score", None)
    components = overall.get("pr_risk_components", {})
    if score is not None:
        print("\n=== Final PR risk score (combined) ===")
        print(f"PR risk score (0=low risk, 1=high risk): {score:.3f}")

        risks = components.get("risks", {})
        weights = components.get("weights", {})

        def fmt(v):
            return f"{v:.3f}" if isinstance(v, (int, float)) else "N/A"

        for name, risk_val in risks.items():
            w = weights.get(name, 0.0)
            print(f"  {name:10s} risk: {fmt(risk_val)} (weight {w:.2f})")

        if score < 0.25:
            qualitative = "very low risk / maintainability-friendly change"
        elif score < 0.5:
            qualitative = "low-to-moderate risk"
        elif score < 0.75:
            qualitative = "moderate-to-high risk, review carefully"
        else:
            qualitative = "high risk, likely to hurt maintainability"

        print(f"  Interpretation:   {qualitative}")


def main():
    parser = argparse.ArgumentParser(
        description="Analyze complexity and delta maintainability of changes introduced by a GitHub pull request."
    )
    parser.add_argument(
        "repo",
        help='Repository in the form "owner/repo", e.g. "zephyrproject-rtos/zephyr".',
    )
    parser.add_argument("pr", type=int, help="Pull request number.")
    parser.add_argument(
        "--repo-path",
        help="Local path to the repository (default: current directory).",
        default=".",
    )
    parser.add_argument(
        "--remote",
        help="Git remote name to fetch from (default: origin).",
        default="origin",
    )
    parser.add_argument(
        "--json-output",
        help="Optional path to write JSON summary.",
    )

    # Extra risk toggles
    parser.add_argument(
        "--enable-interface-risk",
        action="store_true",
        help="Include interface/public-header churn in risk score.",
    )
    parser.add_argument(
        "--enable-path-risk",
        action="store_true",
        help="Include path-based risk (core vs tests/docs).",
    )
    parser.add_argument(
        "--enable-entropy-risk",
        action="store_true",
        help="Include entropy/dispersion of changes across files.",
    )
    parser.add_argument(
        "--enable-fragility-risk",
        action="store_true",
        help="Include hotspot/fragility risk from recent history (heavier).",
    )
    parser.add_argument(
        "--enable-semantic-risk",
        action="store_true",
        help="Include semantic risk from commit messages (refactor vs fix vs feature).",
    )
    parser.add_argument(
        "--enable-all-extras",
        action="store_true",
        help="Enable all extra risk dimensions (interface, path, entropy, fragility, semantic).",
    )

    args = parser.parse_args()

    # Handle --enable-all-extras
    if args.enable_all_extras:
        args.enable_interface_risk = True
        args.enable_path_risk = True
        args.enable_entropy_risk = True
        args.enable_fragility_risk = True
        args.enable_semantic_risk = True

    owner, repo = args.repo.split("/", 1)
    repo_path = os.path.abspath(args.repo_path)
    token = os.environ.get("GITHUB_TOKEN")

    try:
        summary, overall, dmm_commits, dmm_overall = analyze_pr_complexity(
            owner,
            repo,
            args.pr,
            repo_path,
            args.remote,
            token=token,
            enable_interface=args.enable_interface_risk,
            enable_path=args.enable_path_risk,
            enable_entropy=args.enable_entropy_risk,
            enable_fragility=args.enable_fragility_risk,
            enable_semantic=args.enable_semantic_risk,
        )
        print_human_readable(summary, overall, dmm_commits, dmm_overall)

        if args.json_output:
            out = {
                "files": summary,
                "overall": overall,
                "dmm_commits": dmm_commits,
                "dmm_overall": dmm_overall,
            }
            with open(args.json_output, "w", encoding="utf-8") as f:
                json.dump(out, f, indent=2)
            print(f"\n[INFO] JSON summary written to {args.json_output}")

    except Exception as e:
        print(f"[ERROR] {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

