#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0
"""
Remove legacy type/priority labels from GitHub issues that have native fields set.

For each issue that carries a native GitHub issue type (Bug, Enhancement, RFC,
Feature), the corresponding label that was previously used to signal the same
intent is removed:

    Issue type   | Labels removed
    -------------|------------------------------------
    Bug          | bug
    Enhancement  | Enhancement
    RFC          | RFC
    Feature      | Feature, Feature Request

When --priority-labels is given, the script migrates legacy priority labels to
the native GitHub Priority field.  For each issue that carries a "priority: X"
label:

* If the native Priority field is already set, only the label is removed.
* If the native Priority field is not yet set, the field is set from the label
  value (e.g. "priority: low" → Priority = "Low") and the label is removed.

The Priority field ID is discovered automatically via the GitHub GraphQL API.
If auto-discovery fails (the feature may not be enabled for the repo), pass
--priority-field-id with the GraphQL node ID of the field.

Usage:
    python3 scripts/utils/remove_type_labels.py --token <TOKEN> [options]

Options:
    --token TOKEN           GitHub personal access token (repo scope required).
                            Falls back to the GITHUB_TOKEN environment variable.
    --repo OWNER/REPO       Target repository (default: zephyrproject-rtos/zephyr).
    --state STATE           Issue state to scan: open, closed, or all
                            (default: open).
    --priority-labels       Migrate legacy priority labels to the native Priority
                            field: set the field when absent, then remove the
                            label.
    --priority-prefix PFX   Label prefix for old priority labels
                            (default: "priority: ").
    --priority-field-id ID  GraphQL node ID of the Priority issue field.
                            Auto-discovered when omitted.
    --dry-run               Print what would be done without making any changes.
    --verbose               Print every issue examined, not just modified ones.
    --rate-limit-pause SEC  Seconds to sleep between API write calls (default: 0.5).
"""

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request


# Mapping from native issue type name (case-insensitive) to labels to remove.
TYPE_LABEL_MAP = {
    "bug": ["bug"],
    "enhancement": ["Enhancement"],
    "rfc": ["RFC"],
    "feature": ["Feature", "Feature Request"],
}

# Default prefix for legacy priority labels (e.g. "priority: low").
DEFAULT_PRIORITY_PREFIX = "priority: "

GRAPHQL_URL = "https://api.github.com/graphql"

# Introspect the IssueFields union to discover its concrete member type names.
_INTROSPECT_QUERY = """
{
  __type(name: "IssueFields") {
    possibleTypes { name }
  }
}
"""


def _api_request(url, token, method="GET", data=None):
    """Perform a GitHub REST API request and return the parsed JSON body."""
    req = urllib.request.Request(url)
    req.add_header("Authorization", f"token {token}")
    req.add_header("Accept", "application/vnd.github+json")
    req.add_header("X-GitHub-Api-Version", "2022-11-28")
    req.method = method

    body = json.dumps(data).encode() if data is not None else None

    try:
        with urllib.request.urlopen(req, body) as resp:
            raw = resp.read()
            return json.loads(raw) if raw else None
    except urllib.error.HTTPError as exc:
        raw = exc.read()
        msg = json.loads(raw).get("message", raw.decode()) if raw else str(exc)
        raise RuntimeError(f"HTTP {exc.code} for {method} {url}: {msg}") from exc


def _graphql(query, variables, token):
    """Execute a GraphQL query and return the ``data`` dict."""
    payload = json.dumps({"query": query, "variables": variables}).encode()
    req = urllib.request.Request(GRAPHQL_URL, data=payload, method="POST")
    req.add_header("Authorization", f"bearer {token}")
    req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req) as resp:
            result = json.loads(resp.read())
    except urllib.error.HTTPError as exc:
        raw = exc.read()
        raise RuntimeError(f"GraphQL HTTP {exc.code}: {raw.decode()}") from exc
    if "errors" in result:
        raise RuntimeError(
            "; ".join(e.get("message", "") for e in result["errors"])
        )
    return result.get("data", {})


def _discover_priority_field_id(repo, token):
    """Return the GraphQL node ID of the Priority issue field, or None.

    Uses a two-step approach to handle the IssueFields union type:
    1. Introspect the union to discover its concrete member type names.
    2. Build and run the actual field-listing query with correct inline
       fragments for each member type.
    """
    owner, repo_name = repo.split("/", 1)

    # Step 1 — discover which concrete types make up the IssueFields union.
    try:
        intro = _graphql(_INTROSPECT_QUERY, {}, token)
    except RuntimeError as exc:
        print(f"  WARNING: GraphQL introspection failed: {exc}", file=sys.stderr)
        return None

    possible = (intro.get("__type") or {}).get("possibleTypes", [])
    if not possible:
        print(
            "  WARNING: IssueFields union has no known types; "
            "use --priority-field-id to specify the field ID manually.",
            file=sys.stderr,
        )
        return None

    # Step 2 — build the field-listing query with one inline fragment per type.
    fragments = " ".join(f"... on {t['name']} {{ id name }}" for t in possible)
    fields_query = f"""
query($owner: String!, $repo: String!) {{
  organization(login: $owner) {{
    issueFields(first: 50) {{ nodes {{ {fragments} }} }}
  }}
  repository(owner: $owner, name: $repo) {{
    issueFields(first: 50) {{ nodes {{ {fragments} }} }}
  }}
}}
"""
    try:
        data = _graphql(fields_query, {"owner": owner, "repo": repo_name}, token)
    except RuntimeError as exc:
        print(f"  WARNING: GraphQL field discovery failed: {exc}", file=sys.stderr)
        return None

    for scope in ("organization", "repository"):
        nodes = (data.get(scope) or {}).get("issueFields", {}).get("nodes", [])
        for node in nodes:
            if node and (node.get("name") or "").lower() == "priority":
                return node["id"]
    return None


def _set_priority_field(repo, issue_number, field_id, value, token):
    """Set the native Priority field on an issue via the REST PATCH endpoint."""
    url = f"https://api.github.com/repos/{repo}/issues/{issue_number}"
    _api_request(
        url,
        token,
        method="PATCH",
        data={"issue_field_values": [{"field_id": field_id, "value": value}]},
    )


def _paginate_issues(repo, token, state):
    """Yield every issue (not pull request) for *repo* in the given *state*."""
    page = 1
    per_page = 100
    while True:
        url = (
            f"https://api.github.com/repos/{repo}/issues"
            f"?state={state}&per_page={per_page}&page={page}"
        )
        batch = _api_request(url, token)
        if not batch:
            break
        for issue in batch:
            # The /issues endpoint also returns pull requests; skip them.
            if "pull_request" not in issue:
                yield issue
        if len(batch) < per_page:
            break
        page += 1


def _labels_to_remove(issue):
    """Return the subset of old-style type labels still present on *issue*."""
    issue_type = issue.get("type")
    if not issue_type:
        return []

    type_name = issue_type.get("name", "").lower()
    target_labels = TYPE_LABEL_MAP.get(type_name, [])
    if not target_labels:
        return []

    present = {lbl["name"] for lbl in issue.get("labels", [])}
    return [lbl for lbl in target_labels if lbl in present]


def _priority_value(issue):
    """Return the native Priority field value string, or None if not set.

    GitHub exposes custom issue fields in the ``issue_field_values`` list.
    Each entry may take one of these shapes depending on API version:

    * ``{"field": {"name": "Priority"}, "value": "High"}``
    * ``{"name": "Priority", "value": "High"}``
    * ``{"field_name": "Priority", "value": "High"}``
    """
    for fv in issue.get("issue_field_values") or []:
        # Normalise the entry to extract field name and value.
        field_name = (
            (fv.get("field") or {}).get("name")
            or fv.get("name")
            or fv.get("field_name")
            or ""
        )
        if field_name.lower() == "priority":
            return fv.get("value") or ""
    return None


def _priority_label_to_remove(issue, prefix):
    """Return the legacy priority label to remove, or None.

    If the issue has a native Priority field set, returns the label name
    ``<prefix><priority_value_lowercase>`` when that label is present.
    """
    value = _priority_value(issue)
    if not value:
        return None
    candidate = prefix + value.lower()
    present = {lbl["name"] for lbl in issue.get("labels", [])}
    return candidate if candidate in present else None


def _get_priority_label(issue, prefix):
    """Return the priority label present on *issue* (regardless of native field), or None."""
    prefix_lower = prefix.lower()
    for lbl in issue.get("labels", []):
        if lbl["name"].lower().startswith(prefix_lower):
            return lbl["name"]
    return None


def _fetch_issue(repo, issue_number, token):
    """Fetch a single issue by number and return it, or raise RuntimeError."""
    url = f"https://api.github.com/repos/{repo}/issues/{issue_number}"
    return _api_request(url, token)


def _remove_label(repo, issue_number, label, token):
    """Delete *label* from the given issue via the GitHub API."""
    encoded = urllib.parse.quote(label, safe="")
    url = (
        f"https://api.github.com/repos/{repo}/issues"
        f"/{issue_number}/labels/{encoded}"
    )
    _api_request(url, token, method="DELETE")


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--token", default=os.environ.get("GITHUB_TOKEN", ""))
    parser.add_argument("--repo", default="zephyrproject-rtos/zephyr")
    parser.add_argument(
        "--issue",
        type=int,
        metavar="NUMBER",
        help="Process only this issue number instead of scanning all issues.",
    )
    parser.add_argument(
        "--state",
        choices=["open", "closed", "all"],
        default="open",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be removed without making changes.",
    )
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument(
        "--priority-labels",
        action="store_true",
        help="Migrate legacy priority labels to the native Priority field.",
    )
    parser.add_argument(
        "--priority-prefix",
        default=DEFAULT_PRIORITY_PREFIX,
        metavar="PFX",
        help=(
            f'Label prefix for old priority labels (default: "{DEFAULT_PRIORITY_PREFIX}").'
        ),
    )
    parser.add_argument(
        "--priority-field-id",
        metavar="ID",
        help="GraphQL node ID of the Priority field (auto-discovered when omitted).",
    )
    parser.add_argument(
        "--rate-limit-pause",
        type=float,
        default=0.5,
        metavar="SEC",
    )
    args = parser.parse_args()

    if not args.token:
        parser.error(
            "A GitHub token is required. "
            "Pass --token or set the GITHUB_TOKEN environment variable."
        )

    total_examined = 0
    total_removed = 0
    total_set = 0
    errors = []

    # State for lazy Priority field-ID discovery (done at most once per run).
    _prio_field_id = args.priority_field_id  # may be None
    _prio_field_id_checked = _prio_field_id is not None

    mode_notes = []
    if args.dry_run:
        mode_notes.append("dry run")
    if args.priority_labels:
        mode_notes.append(f'priority prefix="{args.priority_prefix}"')
    suffix = f" ({', '.join(mode_notes)})" if mode_notes else ""

    if args.issue:
        print(f"Processing issue #{args.issue} in {args.repo}{suffix}")
        try:
            issues = [_fetch_issue(args.repo, args.issue, args.token)]
        except RuntimeError as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            sys.exit(1)
        if "pull_request" in issues[0]:
            print(f"#{args.issue} is a pull request, not an issue.", file=sys.stderr)
            sys.exit(1)
    else:
        print(f"Scanning {args.state} issues in {args.repo}{suffix}")
        issues = _paginate_issues(args.repo, args.token, args.state)

    for issue in issues:
        total_examined += 1
        number = issue["number"]
        title = issue["title"]
        issue_type = issue.get("type")
        type_name = issue_type["name"] if issue_type else "(none)"

        to_remove = _labels_to_remove(issue)

        if args.priority_labels:
            prio_label = _get_priority_label(issue, args.priority_prefix)
            if prio_label:
                native_prio = _priority_value(issue)
                if native_prio:
                    # Native field already set: just drop the legacy label.
                    to_remove.append(prio_label)
                else:
                    # Native field absent: discover its ID once, set it, then
                    # queue the label for removal.
                    if not _prio_field_id_checked:
                        _prio_field_id = _discover_priority_field_id(
                            args.repo, args.token
                        )
                        _prio_field_id_checked = True
                        if _prio_field_id is None:
                            print(
                                "  WARNING: Priority field not found; "
                                "use --priority-field-id to specify it manually. "
                                "Labels on issues without the native field set "
                                "will not be migrated.",
                                file=sys.stderr,
                            )
                    if _prio_field_id:
                        stripped = prio_label[len(args.priority_prefix):].strip()
                        field_value = stripped.title()
                        if args.dry_run:
                            total_set += 1
                        else:
                            try:
                                _set_priority_field(
                                    args.repo,
                                    number,
                                    _prio_field_id,
                                    field_value,
                                    args.token,
                                )
                                total_set += 1
                                time.sleep(args.rate_limit_pause)
                            except RuntimeError as exc:
                                errors.append(f"#{number} set-priority: {exc}")
                                print(f"    ERROR: {exc}", file=sys.stderr)
                                prio_label = None  # skip label removal on error
                        if prio_label:
                            to_remove.append(prio_label)

        if args.verbose or to_remove:
            prio_val = _priority_value(issue) if args.priority_labels else None
            prio_str = f"  prio={prio_val}" if prio_val else ""
            print(
                f"  #{number:>7}  type={type_name:<12}{prio_str}"
                f"  remove={to_remove or []!r:40}  {title[:60]}"
            )

        for label in to_remove:
            if args.dry_run:
                total_removed += 1
                continue
            try:
                _remove_label(args.repo, number, label, args.token)
                total_removed += 1
                time.sleep(args.rate_limit_pause)
            except RuntimeError as exc:
                errors.append(f"#{number}: {exc}")
                print(f"    ERROR: {exc}", file=sys.stderr)

    verb = "would" if args.dry_run else "did"
    parts = [f"examined {total_examined} issues"]
    if total_set:
        parts.append(f"{verb} set Priority field on {total_set}")
    parts.append(f"{verb} remove {total_removed} label(s)")
    print("\nDone. " + "; ".join(parts) + ".")
    if errors:
        print(f"\n{len(errors)} error(s):", file=sys.stderr)
        for err in errors:
            print(f"  {err}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
