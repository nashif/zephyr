#!/usr/bin/env python3
# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0
"""Generate an HTML summary report from one or more twister.json files."""

from __future__ import annotations

import argparse
import glob
import json
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path

# All known testsuite/testcase statuses (from TwisterStatus enum)
_SUITE_STATUSES = ['passed', 'failed', 'error', 'skipped', 'filtered', 'not run']
_CASE_STATUSES  = ['passed', 'failed', 'error', 'skipped', 'blocked', 'not run']

# Statuses that represent an actual execution attempt (not build-only)
_EXECUTED_STATUSES = {'passed', 'failed', 'error'}

_STATUS_COLOR = {
    'passed':   '#2da44e',
    'failed':   '#cf222e',
    'error':    '#cf222e',
    'skipped':  '#9a6700',
    'filtered': '#9a6700',
    'blocked':  '#9a6700',
    'not run':  '#6e7781',
}

_STATUS_BADGE = {
    'passed':   ('✔', '#2da44e', '#ffffff'),
    'failed':   ('✖', '#cf222e', '#ffffff'),
    'error':    ('⚠', '#cf222e', '#ffffff'),
    'skipped':  ('⊘', '#bf8700', '#ffffff'),
    'filtered': ('⊘', '#bf8700', '#ffffff'),
    'blocked':  ('⊘', '#bf8700', '#ffffff'),
    'not run':  ('–',  '#6e7781', '#ffffff'),
}

# Human-readable descriptions used in the legend
_STATUS_DESC = {
    'passed':   'Test built and executed successfully.',
    'failed':   'Test built and executed but produced unexpected results.',
    'error':    'Test encountered a runtime or infrastructure error during execution.',
    'skipped':  'Test was explicitly skipped (e.g. unmet precondition).',
    'filtered': 'Test was filtered out for this platform/configuration and not built.',
    'blocked':  'Test case could not run because a dependency or setup step failed.',
    'not run':  'Test was built successfully but not executed — '
                'no matching hardware or simulation environment available.',
}


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------

def load_files(patterns: list[str]) -> tuple[dict | None, list[dict]]:
    """Return (environment, testsuites) merged from all matching files."""
    input_files: list[str] = []
    for pattern in patterns:
        matched = sorted(glob.glob(pattern, recursive=True))
        input_files.extend(matched if matched else [pattern])

    if not input_files:
        print(f"No input files found for: {patterns}", file=sys.stderr)
        sys.exit(1)

    environment: dict | None = None
    testsuites: list[dict] = []

    for path in input_files:
        try:
            with open(path) as fh:
                data = json.load(fh)
        except (OSError, json.JSONDecodeError) as exc:
            print(f"Error reading {path}: {exc}", file=sys.stderr)
            sys.exit(1)
        if environment is None:
            environment = data.get('environment', {})
        testsuites.extend(data.get('testsuites', []))

    return environment, testsuites


# ---------------------------------------------------------------------------
# Statistics helpers
# ---------------------------------------------------------------------------

def status_counts(items: list[dict], all_statuses: list[str]) -> dict[str, int]:
    counts: dict[str, int] = defaultdict(int)
    for item in items:
        counts[item.get('status', 'not run')] += 1
    return {s: counts.get(s, 0) for s in all_statuses}


def pass_rate(counts: dict[str, int]) -> float:
    """Pass rate over *executed* suites only (passed / (passed+failed+error)).

    'not run', 'filtered', and 'skipped' are excluded from the denominator
    because they were never executed — 'not run' means built-only with no
    available hardware or simulation target.
    """
    executed = sum(counts.get(s, 0) for s in _EXECUTED_STATUSES)
    return counts.get('passed', 0) / executed * 100 if executed else 0.0


def run_notrun_counts(suites: list[dict]) -> tuple[int, int]:
    """Return (ran, not_run) counts for a list of testsuites."""
    ran     = sum(1 for ts in suites if ts.get('status') in _EXECUTED_STATUSES)
    not_run = sum(1 for ts in suites if ts.get('status') == 'not run')
    return ran, not_run


def board_stats(testsuites: list[dict]) -> list[dict]:
    """Return per-board aggregated stats, sorted by total suites desc."""
    by_board: dict[str, dict] = defaultdict(lambda: {
        'suites': [], 'testcases': []
    })
    for ts in testsuites:
        board = ts.get('platform', 'unknown')
        by_board[board]['suites'].append(ts)
        by_board[board]['testcases'].extend(ts.get('testcases', []))

    rows = []
    for board, data in by_board.items():
        sc = status_counts(data['suites'], _SUITE_STATUSES)
        tc = status_counts(data['testcases'], _CASE_STATUSES)
        ran, not_run = run_notrun_counts(data['suites'])
        rows.append({
            'board':         board,
            'arch':          data['suites'][0].get('arch', ''),
            'suite_counts':  sc,
            'suite_total':   sum(sc.values()),
            'suite_passed':  sc.get('passed', 0),
            'suite_ran':     ran,
            'suite_notrun':  not_run,
            'tc_counts':     tc,
            'tc_total':      sum(tc.values()),
            'tc_passed':     tc.get('passed', 0),
            'pass_rate':     pass_rate(sc),
            'build_time':    sum(float(s.get('build_time', 0) or 0) for s in data['suites']),
            'exec_time':     sum(float(s.get('execution_time', 0) or 0)
                                 for s in data['suites'] if s.get('runnable')),
        })

    rows.sort(key=lambda r: (-r['suite_total'], r['board']))
    return rows


def arch_stats(testsuites: list[dict]) -> list[dict]:
    by_arch: dict[str, list[dict]] = defaultdict(list)
    for ts in testsuites:
        by_arch[ts.get('arch', 'unknown')].append(ts)
    rows = []
    for arch, suites in sorted(by_arch.items()):
        sc = status_counts(suites, _SUITE_STATUSES)
        tc_all = [tc for ts in suites for tc in ts.get('testcases', [])]
        tc = status_counts(tc_all, _CASE_STATUSES)
        ran, not_run = run_notrun_counts(suites)
        rows.append({
            'arch':         arch,
            'suite_counts': sc,
            'suite_total':  sum(sc.values()),
            'suite_ran':    ran,
            'suite_notrun': not_run,
            'tc_total':     sum(tc.values()),
            'tc_passed':    tc.get('passed', 0),
            'pass_rate':    pass_rate(sc),
        })
    rows.sort(key=lambda r: -r['suite_total'])
    return rows


def toolchain_stats(testsuites: list[dict]) -> list[dict]:
    by_tc: dict[str, list[dict]] = defaultdict(list)
    for ts in testsuites:
        by_tc[ts.get('toolchain', 'unknown')].append(ts)
    rows = []
    for tc_name, suites in sorted(by_tc.items()):
        sc = status_counts(suites, _SUITE_STATUSES)
        ran, not_run = run_notrun_counts(suites)
        rows.append({
            'toolchain':    tc_name,
            'suite_counts': sc,
            'suite_total':  sum(sc.values()),
            'suite_ran':    ran,
            'suite_notrun': not_run,
            'pass_rate':    pass_rate(sc),
        })
    rows.sort(key=lambda r: -r['suite_total'])
    return rows


def failed_suites(testsuites: list[dict]) -> list[dict]:
    return [
        ts for ts in testsuites
        if ts.get('status') in ('failed', 'error')
    ]


# ---------------------------------------------------------------------------
# HTML generation helpers
# ---------------------------------------------------------------------------

def _badge(status: str, count: int) -> str:
    if count == 0:
        return ''
    icon, bg, fg = _STATUS_BADGE.get(status, ('?', '#6e7781', '#fff'))
    return (
        f'<span class="badge" style="background:{bg};color:{fg}" '
        f'title="{status}">{icon} {count}</span>'
    )


def _pct_bar(pct: float) -> str:
    color = '#2da44e' if pct >= 90 else '#bf8700' if pct >= 50 else '#cf222e'
    return (
        f'<div class="bar-wrap" title="{pct:.1f}%">'
        f'<div class="bar" style="width:{pct:.1f}%;background:{color}"></div>'
        f'<span class="bar-label">{pct:.1f}%</span>'
        f'</div>'
    )


CSS = """
:root{--bg:#f6f8fa;--border:#d0d7de;--text:#24292f;--head:#eaeef2}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif;
     font-size:14px;color:var(--text);background:var(--bg);padding:24px}
h1{font-size:1.6em;margin-bottom:4px}
h2{font-size:1.15em;margin:28px 0 10px;border-bottom:1px solid var(--border);padding-bottom:6px}
.meta{color:#57606a;font-size:0.85em;margin-bottom:20px}
.cards{display:flex;flex-wrap:wrap;gap:14px;margin-bottom:28px}
.card{background:#fff;border:1px solid var(--border);border-radius:8px;
      padding:16px 22px;min-width:160px;flex:1 1 160px;max-width:220px}
.card-value{font-size:2em;font-weight:700;line-height:1.1}
.card-label{color:#57606a;font-size:0.82em;margin-top:4px}
table{width:100%;border-collapse:collapse;background:#fff;
      border:1px solid var(--border);border-radius:8px;overflow:hidden;margin-bottom:28px}
thead tr{background:var(--head)}
th,td{padding:8px 12px;text-align:left;border-bottom:1px solid var(--border);white-space:nowrap}
tbody tr:last-child td{border-bottom:none}
tbody tr:hover{background:#f0f3f6}
.badge{display:inline-flex;align-items:center;gap:3px;
       padding:2px 8px;border-radius:12px;font-size:0.8em;font-weight:600;margin:1px}
.bar-wrap{display:flex;align-items:center;gap:8px;min-width:120px}
.bar{height:10px;border-radius:4px;min-width:2px;transition:width .3s}
.bar-label{font-size:0.8em;color:#57606a;white-space:nowrap}
.fail-name{font-weight:600}
.subtle{color:#57606a;font-size:0.85em}
.legend{display:flex;flex-wrap:wrap;gap:10px;margin-bottom:28px;
         background:#fff;border:1px solid var(--border);border-radius:8px;padding:16px}
/* collapsible name tree */
.tree{background:#fff;border:1px solid var(--border);border-radius:8px;
      padding:10px 14px;margin-bottom:28px;overflow-x:auto}
.tree details{margin:0}
.tree details>details,.tree details>.leaf-table{margin-left:20px}
.tree summary{cursor:pointer;padding:4px 6px;border-radius:4px;
               list-style:none;display:flex;align-items:center;gap:6px}
.tree summary::-webkit-details-marker{display:none}
.tree summary::before{content:'\25B6';font-size:0.7em;color:#57606a;
                       display:inline-block;transition:transform 0.15s}
.tree details[open]>summary::before{transform:rotate(90deg)}
.tree summary:hover{background:var(--head)}
.tree .node-label{font-weight:600;font-size:0.92em}
.tree .node-stats{font-size:0.78em;color:#57606a;margin-left:4px}
.leaf-table{margin:4px 0 6px;border-collapse:collapse;font-size:0.82em;
             background:#fff;border:1px solid var(--border);
             border-radius:6px;overflow:hidden;width:max-content}
.leaf-table th,.leaf-table td{padding:4px 10px;text-align:left;
                               border-bottom:1px solid var(--border)}
.leaf-table thead tr{background:var(--head)}
.leaf-table tbody tr:last-child td{border-bottom:none}
.leaf-table tbody tr:hover{background:#f0f3f6}
/* insights */
.insight-section{background:#fff;border:1px solid var(--border);border-radius:8px;
                  padding:14px 18px;margin-bottom:18px}
.insight-section h3{font-size:1em;margin-bottom:8px;color:var(--text)}
.insight-ok{color:#2da44e;font-size:0.88em}
.insight-warn{color:#bf8700;font-size:0.88em}
.insight-crit{color:#cf222e;font-size:0.88em}
.insight-label{display:inline-block;padding:1px 7px;border-radius:10px;
                font-size:0.75em;font-weight:700;margin-right:6px}
.tag-badfilter{background:#cf222e;color:#fff}
.tag-buildonly{background:#6e7781;color:#fff}
.tag-noexec{background:#9a6700;color:#fff}
.tag-flaky{background:#8250df;color:#fff}
.tag-slow{background:#0969da;color:#fff}
.tag-lowcov{background:#57606a;color:#fff}
.legend-item{display:flex;align-items:flex-start;gap:8px;min-width:280px;flex:1 1 280px}
.legend-dot{width:12px;height:12px;border-radius:50%;flex-shrink:0;margin-top:2px}
.legend-text strong{display:block;font-size:0.85em}
.legend-text span{color:#57606a;font-size:0.8em;line-height:1.4}
.run-stat{font-size:0.85em;color:#57606a}
/* sticky top nav */
.topnav{position:sticky;top:0;z-index:100;background:#24292f;padding:0 16px;
         display:flex;align-items:center;gap:0;flex-wrap:wrap;
         border-bottom:2px solid #444c56;margin:-24px -24px 24px}
.topnav a{color:#cdd9e5;text-decoration:none;font-size:0.82em;font-weight:500;
           padding:10px 12px;white-space:nowrap;border-bottom:2px solid transparent;
           transition:border-color .15s,color .15s}
.topnav a:hover{color:#fff;border-bottom-color:#58a6ff}
.topnav .nav-brand{color:#fff;font-weight:700;font-size:0.9em;
                    padding:10px 14px 10px 0;margin-right:4px;
                    border-right:1px solid #444c56}
/* dropdown */
.nav-dropdown{position:relative}
.nav-dropdown>a::after{content:' ▾'}
.nav-dropdown .dropdown-menu{display:none;position:absolute;top:100%;left:0;
    background:#2d333b;border:1px solid #444c56;border-radius:0 0 6px 6px;
    min-width:200px;z-index:200;padding:4px 0}
.nav-dropdown:hover .dropdown-menu,.nav-dropdown:focus-within .dropdown-menu{display:block}
.dropdown-menu a{display:flex;align-items:center;gap:8px;padding:7px 14px;
    border-bottom:none;font-size:0.8em;white-space:nowrap}
.dropdown-menu a:hover{background:#373e47;border-bottom:none}
.dm-tag{display:inline-block;padding:1px 6px;border-radius:8px;
         font-size:0.72em;font-weight:700;flex-shrink:0}
.dm-ok{color:#2da44e}.dm-warn{color:#bf8700}.dm-crit{color:#cf222e}
@media(max-width:700px){.topnav{flex-wrap:wrap;gap:0}.topnav a{padding:7px 9px}}
@media(max-width:700px){.cards{flex-direction:column}.card{max-width:100%}}
"""


def _html_top(title: str, env: dict | None, total_suites: int,
              suite_counts: dict, tc_total: int, tc_passed: int,
              suites_ran: int, suites_notrun: int,
              insight_items: list[dict] | None = None) -> str:
    ts_passed  = suite_counts.get('passed', 0)
    ts_failed  = suite_counts.get('failed', 0) + suite_counts.get('error', 0)
    ts_skipped = suite_counts.get('skipped', 0) + suite_counts.get('filtered', 0)
    pct = pass_rate(suite_counts)

    env = env or {}
    zver   = env.get('zephyr_version', 'N/A')
    tchain = env.get('toolchain', 'N/A')
    run_dt = env.get('run_date', '')
    try:
        run_dt = datetime.fromisoformat(run_dt).strftime('%Y-%m-%d %H:%M UTC')
    except (ValueError, TypeError):
        pass

    cards = [
        (total_suites,   'Total Suites Built'),
        (suites_ran,     'Suites Executed'),
        (suites_notrun,  'Suites Not Run\u00a0(build-only)'),
        (tc_total,       'Test Cases'),
        (ts_passed,      'Suites Passed'),
        (ts_failed,      'Suites Failed / Error'),
        (ts_skipped,     'Filtered / Skipped'),
        (f'{pct:.1f}%',  'Pass Rate\u00a0(executed only)'),
    ]
    card_html = ''.join(
        f'<div class="card"><div class="card-value">{v}</div>'
        f'<div class="card-label">{l}</div></div>'
        for v, l in cards
    )

    # Build insights dropdown items
    insight_items = insight_items or []
    if insight_items:
        dropdown_links = ''.join(
            f'<a href="#{it["slug"]}">'
            f'<span class="dm-tag" style="background:{it["bg"]};color:#fff">{it["tag"]}</span>'
            f'<span class="dm-{it["sev"]}">{it["short"]}</span>'
            f'</a>'
            for it in insight_items
        )
        insights_nav = (
            f'<span class="nav-dropdown">'
            f'<a href="#insights">Insights</a>'
            f'<div class="dropdown-menu">{dropdown_links}</div>'
            f'</span>'
        )
    else:
        insights_nav = '<a href="#insights">Insights</a>'

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{title}</title>
<style>{CSS}</style>
</head>
<body>
<nav class="topnav">
  <span class="nav-brand">Twister</span>
  <a href="#summary">Summary</a>
  <a href="#legend">Legend</a>
  {insights_nav}
  <a href="#tree">By Test Name</a>
  <a href="#arch">By Arch</a>
  <a href="#toolchain">By Toolchain</a>
  <a href="#boards">By Board</a>
  <a href="#failures">Failures</a>
</nav>
<h1 id="summary">{title}</h1>
<p class="meta">
  Zephyr&nbsp;{zver} &nbsp;|&nbsp; Toolchain:&nbsp;{tchain}
  {"&nbsp;|&nbsp; Run date: " + run_dt if run_dt else ""}
</p>
<div class="cards">{card_html}</div>
"""


def _arch_table(rows: list[dict]) -> str:
    thead = ('<tr><th>Arch</th><th>Built</th><th>Ran</th><th>Not Run</th>'
             '<th>Test Cases</th><th>Status breakdown</th>'
             '<th>Pass rate (executed)</th></tr>')
    body = ''
    for r in rows:
        badges = ' '.join(_badge(s, r['suite_counts'].get(s, 0))
                          for s in _SUITE_STATUSES)
        body += (
            f'<tr><td><strong>{r["arch"]}</strong></td>'
            f'<td>{r["suite_total"]}</td>'
            f'<td>{r["suite_ran"]}</td>'
            f'<td class="subtle">{r["suite_notrun"]}</td>'
            f'<td>{r["tc_passed"]}&nbsp;/&nbsp;{r["tc_total"]}</td>'
            f'<td>{badges}</td>'
            f'<td>{_pct_bar(r["pass_rate"])}</td></tr>'
        )
    return f'<h2 id="arch">Results by Architecture</h2><table><thead>{thead}</thead><tbody>{body}</tbody></table>'


def _toolchain_table(rows: list[dict]) -> str:
    thead = ('<tr><th>Toolchain</th><th>Built</th><th>Ran</th><th>Not Run</th>'
             '<th>Status breakdown</th><th>Pass rate (executed)</th></tr>')
    body = ''
    for r in rows:
        badges = ' '.join(_badge(s, r['suite_counts'].get(s, 0))
                          for s in _SUITE_STATUSES)
        body += (
            f'<tr><td><strong>{r["toolchain"]}</strong></td>'
            f'<td>{r["suite_total"]}</td>'
            f'<td>{r["suite_ran"]}</td>'
            f'<td class="subtle">{r["suite_notrun"]}</td>'
            f'<td>{badges}</td>'
            f'<td>{_pct_bar(r["pass_rate"])}</td></tr>'
        )
    return f'<h2 id="toolchain">Results by Toolchain</h2><table><thead>{thead}</thead><tbody>{body}</tbody></table>'


def _board_table(rows: list[dict]) -> str:
    thead = (
        '<tr><th>Board / Platform</th><th>Arch</th>'
        '<th>Built</th><th>Ran</th><th>Not Run</th>'
        '<th>Test Cases</th><th>Suite status</th>'
        '<th>Pass rate (executed)</th>'
        '<th>Build time (s)</th><th>Exec time (s)</th></tr>'
    )
    body = ''
    for r in rows:
        badges = ' '.join(_badge(s, r['suite_counts'].get(s, 0))
                          for s in _SUITE_STATUSES)
        notrun_cell = (
            f'<span title="Build-only: no hardware/simulator available"'
            f' class="subtle">{r["suite_notrun"]}</span>'
            if r['suite_notrun'] else '<span class="subtle">0</span>'
        )
        body += (
            f'<tr>'
            f'<td><code>{r["board"]}</code></td>'
            f'<td>{r["arch"]}</td>'
            f'<td>{r["suite_total"]}</td>'
            f'<td>{r["suite_ran"]}</td>'
            f'<td>{notrun_cell}</td>'
            f'<td>{r["tc_passed"]}&nbsp;/&nbsp;{r["tc_total"]}</td>'
            f'<td>{badges}</td>'
            f'<td>{_pct_bar(r["pass_rate"])}</td>'
            f'<td class="subtle">{r["build_time"]:.1f}</td>'
            f'<td class="subtle">{r["exec_time"]:.1f}</td>'
            f'</tr>'
        )
    return f'<h2 id="boards">Results by Board / Platform ({len(rows)} boards)</h2><table><thead>{thead}</thead><tbody>{body}</tbody></table>'


# ---------------------------------------------------------------------------
# Name tree
# ---------------------------------------------------------------------------

def build_name_tree(testsuites: list[dict]) -> dict:
    """Build a nested dict tree from dot-separated test suite names.

    Each internal node is a dict whose values are either nested dicts
    (sub-nodes) or a special '_suites' key holding a list of testsuite
    records that share that exact name path.
    """
    root: dict = {}
    for ts in testsuites:
        parts = ts.get('name', 'unknown').split('.')
        node = root
        for part in parts:
            node = node.setdefault(part, {})
        node.setdefault('_suites', []).append(ts)
    return root


def _tree_node_stats(node: dict) -> tuple[int, int, int]:
    """Return (total, ran, passed) counts for all suites under a node."""
    total = ran = passed = 0
    stack = [node]
    while stack:
        n = stack.pop()
        for k, v in n.items():
            if k == '_suites':
                for ts in v:
                    total += 1
                    if ts.get('status') in _EXECUTED_STATUSES:
                        ran += 1
                    if ts.get('status') == 'passed':
                        passed += 1
            elif isinstance(v, dict):
                stack.append(v)
    return total, ran, passed


def _render_tree_node(name: str, node: dict, depth: int = 0) -> str:
    """Recursively render a tree node as a <details> element."""
    child_keys = [k for k in node if k != '_suites']
    suites     = node.get('_suites', [])
    total, ran, passed = _tree_node_stats(node)

    pct_str = ''
    if ran > 0:
        pct = passed / ran * 100
        color = '#2da44e' if pct >= 90 else '#bf8700' if pct >= 50 else '#cf222e'
        pct_str = f'<span style="color:{color};font-weight:600">{pct:.0f}%</span> '

    stats = (
        f'<span class="node-stats">'
        f'{pct_str}'
        f'{total} suite{"s" if total != 1 else ""}, '
        f'{ran} ran'
        f'{", " + str(total - ran) + " build-only" if total - ran else ""}'
        f'</span>'
    )

    summary = (
        f'<summary>'
        f'<span class="node-label">{name}</span>'
        f'{stats}'
        f'</summary>'
    )

    inner = ''

    # Render platform table at leaf (or mixed) nodes that carry suites
    if suites:
        inner += _render_leaf_table(suites)

    # Recurse into child nodes
    for child_key in sorted(child_keys):
        inner += _render_tree_node(child_key, node[child_key], depth + 1)

    open_attr = ' open' if depth == 0 else ''
    return f'<details{open_attr}>{summary}{inner}</details>\n'


def _render_leaf_table(suites: list[dict]) -> str:
    """Render a compact per-platform table for suites sharing a name."""
    thead = ('<tr><th>Platform</th><th>Arch</th><th>Toolchain</th>'
             '<th>Status</th><th>Build (s)</th><th>Exec (s)</th></tr>')
    rows = ''
    for ts in sorted(suites, key=lambda s: s.get('platform', '')):
        status = ts.get('status', '')
        color  = _STATUS_COLOR.get(status, '#6e7781')
        icon   = _STATUS_BADGE.get(status, ('?', color, '#fff'))[0]
        sbadge = (
            f'<span class="badge" style="background:{color};color:#fff">'
            f'{icon} {status}</span>'
        )
        exec_t = (
            f'{float(ts["execution_time"]):.2f}'
            if ts.get('runnable') and ts.get('execution_time') else '–'
        )
        rows += (
            f'<tr>'
            f'<td><code>{ts.get("platform","")}</code></td>'
            f'<td>{ts.get("arch","")}</td>'
            f'<td>{ts.get("toolchain","")}</td>'
            f'<td>{sbadge}</td>'
            f'<td class="subtle">{float(ts.get("build_time") or 0):.2f}</td>'
            f'<td class="subtle">{exec_t}</td>'
            f'</tr>'
        )
    return f'<table class="leaf-table"><thead>{thead}</thead><tbody>{rows}</tbody></table>\n'


def _name_tree_html(testsuites: list[dict]) -> str:
    """Render the full collapsible test-name tree section."""
    tree = build_name_tree(testsuites)
    inner = ''.join(
        _render_tree_node(key, tree[key], depth=0)
        for key in sorted(tree)
    )
    return f'<h2 id="tree">Results by Test Name</h2><div class="tree">{inner}</div>'


# ---------------------------------------------------------------------------
# Insights & anomaly detection
# ---------------------------------------------------------------------------

def _compute_insights(testsuites: list[dict]) -> dict:
    """Analyse testsuites and return a dict of categorised findings."""
    import statistics as _stats

    # --- Group by test name across all platforms ---
    by_name: dict[str, list[dict]] = defaultdict(list)
    for ts in testsuites:
        by_name[ts.get('name', 'unknown')].append(ts)

    # 1. Zero-execution tests: a test name where no instance ever ran
    #    Sub-classes:
    #      a) ALL instances filtered -> possible bad filter configuration
    #      b) ALL instances "not run" -> always build-only (no exec env)
    #      c) Mix of filtered + not-run -> combined no-exec
    all_filtered_names:  list[dict] = []
    all_notrun_names:    list[dict] = []
    mixed_noexec_names:  list[dict] = []

    for name, suites in by_name.items():
        statuses = {ts['status'] for ts in suites}
        exec_count = sum(1 for ts in suites if ts['status'] in _EXECUTED_STATUSES)
        if exec_count > 0:
            continue  # at least one execution happened — not anomalous here
        filtered_count = sum(1 for ts in suites if ts['status'] == 'filtered')
        notrun_count   = sum(1 for ts in suites if ts['status'] == 'not run')
        skipped_count  = sum(1 for ts in suites if ts['status'] == 'skipped')
        total = len(suites)
        entry = {
            'name':     name,
            'total':    total,
            'filtered': filtered_count,
            'not_run':  notrun_count,
            'skipped':  skipped_count,
            'path':     suites[0].get('path', ''),
        }
        if filtered_count == total:
            all_filtered_names.append(entry)
        elif notrun_count == total:
            all_notrun_names.append(entry)
        else:
            mixed_noexec_names.append(entry)

    # 2. Flaky tests: retried at least once
    flaky: list[dict] = [
        {
            'name':     ts.get('name', ''),
            'platform': ts.get('platform', ''),
            'status':   ts.get('status', ''),
            'retries':  ts.get('retries', 0),
        }
        for ts in testsuites if ts.get('retries', 0) > 0
    ]
    flaky.sort(key=lambda x: -x['retries'])

    # 3. Build-time outliers: > mean + 2*stdev
    build_times = [(float(ts.get('build_time', 0) or 0), ts) for ts in testsuites]
    bt_vals = [b for b, _ in build_times]
    if len(bt_vals) >= 3:
        bt_mean, bt_sd = _stats.mean(bt_vals), _stats.stdev(bt_vals)
        bt_threshold = bt_mean + 2 * bt_sd
    else:
        bt_mean = bt_sd = 0.0
        bt_threshold = float('inf')
    slow_build: list[dict] = sorted(
        [{'name': ts.get('name',''), 'platform': ts.get('platform',''),
          'build_time': b, 'z': (b - bt_mean) / bt_sd if bt_sd else 0}
         for b, ts in build_times if b > bt_threshold],
        key=lambda x: -x['build_time']
    )

    # 4. Execution-time outliers: > mean + 2*stdev (runnable suites only)
    exec_pairs = [(float(ts.get('execution_time', 0) or 0), ts)
                  for ts in testsuites if ts.get('runnable')]
    et_vals = [e for e, _ in exec_pairs]
    if len(et_vals) >= 3:
        et_mean, et_sd = _stats.mean(et_vals), _stats.stdev(et_vals)
        et_threshold = et_mean + 2 * et_sd
    else:
        et_mean = et_sd = 0.0
        et_threshold = float('inf')
    slow_exec: list[dict] = sorted(
        [{'name': ts.get('name',''), 'platform': ts.get('platform',''),
          'exec_time': e, 'z': (e - et_mean) / et_sd if et_sd else 0}
         for e, ts in exec_pairs if e > et_threshold],
        key=lambda x: -x['exec_time']
    )

    # 5. Tests failing on multiple platforms
    failures: dict[str, list[str]] = defaultdict(list)
    for ts in testsuites:
        if ts['status'] in ('failed', 'error'):
            failures[ts['name']].append(ts['platform'])
    multi_fail = sorted(
        [{'name': n, 'platforms': plats, 'count': len(plats)}
         for n, plats in failures.items() if len(plats) >= 1],
        key=lambda x: -x['count']
    )

    # 6. Platforms with the most failures
    plat_failures: dict[str, int] = defaultdict(int)
    for ts in testsuites:
        if ts['status'] in ('failed', 'error'):
            plat_failures[ts['platform']] += 1
    platform_fail_hot = sorted(
        [{'platform': p, 'count': c} for p, c in plat_failures.items()],
        key=lambda x: -x['count']
    )

    # 7. Very low platform coverage: test name with only 1 platform instance
    #    (and that instance is executed, not just filtered)
    low_coverage = [
        {'name': name, 'platform': suites[0].get('platform',''),
         'status': suites[0].get('status','')}
        for name, suites in by_name.items()
        if len(suites) == 1 and suites[0].get('status') in _EXECUTED_STATUSES
    ]

    # 8. Suites with empty testcase list that did execute
    empty_tc = [
        {'name': ts.get('name',''), 'platform': ts.get('platform',''),
         'status': ts.get('status','')}
        for ts in testsuites
        if ts.get('status') in _EXECUTED_STATUSES and not ts.get('testcases')
    ]

    return {
        'all_filtered':       all_filtered_names,
        'all_notrun':         all_notrun_names,
        'mixed_noexec':       mixed_noexec_names,
        'flaky':              flaky,
        'slow_build':         slow_build,
        'slow_exec':          slow_exec,
        'multi_fail':         multi_fail,
        'platform_fail_hot':  platform_fail_hot,
        'low_coverage':       low_coverage,
        'empty_tc':           empty_tc,
        'bt_mean':            bt_mean,
        'bt_sd':              bt_sd,
        'et_mean':            et_mean,
        'et_sd':              et_sd,
    }


def _insight_block(title: str, tag_html: str, description: str, body_html: str,
                   severity: str = 'warn', slug: str = '') -> str:
    """Render a single insight block with an optional anchor id."""
    sev_class = {'ok': 'insight-ok', 'warn': 'insight-warn',
                 'crit': 'insight-crit'}.get(severity, 'insight-warn')
    id_attr = f' id="{slug}"' if slug else ''
    return (
        f'<div class="insight-section"{id_attr}>'
        f'<h3>{tag_html}<span class="{sev_class}">{title}</span></h3>'
        f'<p class="subtle" style="margin-bottom:8px">{description}</p>'
        f'{body_html}'
        f'</div>'
    )


def _ok_block(title: str) -> str:
    return (
        f'<div class="insight-section">'
        f'<h3><span class="insight-ok">✔ {title}</span></h3>'
        f'</div>'
    )


def _zero_exec_table(entries: list[dict], extra_col: str = '') -> str:
    if not entries:
        return ''
    extra_th = f'<th>{extra_col}</th>' if extra_col else ''
    thead = (f'<tr><th>Test name</th><th>Path</th><th>Platforms</th>'
             f'<th>Filtered</th><th>Not run</th><th>Skipped</th>'
             f'{extra_th}'
             f'</tr>')
    body = ''.join(
        f'<tr><td><code>{e["name"]}</code></td>'
        f'<td class="subtle">{e["path"]}</td>'
        f'<td>{e["total"]}</td>'
        f'<td>{e["filtered"] or "-"}</td>'
        f'<td>{e["not_run"] or "-"}</td>'
        f'<td>{e["skipped"] or "-"}</td>'
        f'</tr>'
        for e in entries
    )
    return f'<table><thead>{thead}</thead><tbody>{body}</tbody></table>'


def _insights_html(testsuites: list[dict]) -> tuple[str, list[dict]]:
    """Return (html, nav_items) where nav_items drives the dropdown menu."""
    ins = _compute_insights(testsuites)
    sections = ['<h2 id="insights">Insights &amp; Anomalies</h2>']
    nav_items: list[dict] = []   # {slug, tag, short, bg, sev}

    # colour/tag lookup matching _STATUS_BADGE palette
    _TAG_STYLES = {
        'tag-badfilter': ('#cf222e', 'crit'),
        'tag-noexec':    ('#9a6700', 'warn'),
        'tag-buildonly': ('#6e7781', 'warn'),
        'tag-flaky':     ('#8250df', 'warn'),
        'tag-slow':      ('#0969da', 'warn'),
        'tag-lowcov':    ('#57606a', 'warn'),
    }

    def _add(block_html: str, slug: str, tag_cls: str, tag_text: str,
             short: str, is_ok: bool = False) -> None:
        sections.append(block_html)
        bg, sev = ('#2da44e', 'ok') if is_ok else _TAG_STYLES.get(tag_cls, ('#6e7781', 'warn'))
        nav_items.append({'slug': slug, 'tag': tag_text, 'short': short,
                          'bg': bg, 'sev': sev})

    # --- 1. All-filtered: no execution by any target ---
    if ins['all_filtered']:
        n = len(ins['all_filtered'])
        tag = '<span class="insight-label tag-badfilter">BAD FILTER</span>'
        body = _zero_exec_table(ins['all_filtered'])
        _add(_insight_block(
            f'{n} test suite{"s" if n!=1 else ""} filtered on every platform —'
            f' never built or run',
            tag,
            'These tests have filter rules that exclude <strong>all</strong> '
            'available platforms. They are never built or executed, which may '
            'indicate an overly broad or misconfigured filter expression. '
            'Review the <code>filter:</code> and <code>platform_allow:</code>/<code>platform_exclude:</code> '
            'entries in the test YAML.',
            body, severity='crit', slug='ins-badfilter'
        ), 'ins-badfilter', 'tag-badfilter', 'BAD FILTER',
           f'{n} always-filtered')
    else:
        _add(_ok_block('No tests filtered out on every platform'),
             'ins-badfilter', '', '✔', 'No bad filters', is_ok=True)

    # --- 2. Mixed no-exec (filtered + not-run, zero execution) ---
    combined = ins['mixed_noexec']
    if combined:
        n = len(combined)
        tag = '<span class="insight-label tag-noexec">NO EXEC</span>'
        body = _zero_exec_table(combined)
        _add(_insight_block(
            f'{n} test suite{"s" if n!=1 else ""} never executed (mix of filtered and not-run)',
            tag,
            'These tests are partly filtered and partly built-only but have '
            'zero executed instances across the run. They contribute build '
            'time but produce no pass/fail signal.',
            body, severity='warn', slug='ins-noexec'
        ), 'ins-noexec', 'tag-noexec', 'NO EXEC', f'{n} never executed')
    else:
        pass  # omit from nav when absent

    # --- 3. All-not-run: always build-only ---
    if ins['all_notrun']:
        n = len(ins['all_notrun'])
        tag = '<span class="insight-label tag-buildonly">BUILD-ONLY</span>'
        body = _zero_exec_table(ins['all_notrun'])
        _add(_insight_block(
            f'{n} test suite{"s" if n!=1 else ""} built everywhere but never executed',
            tag,
            'Every platform that includes this test has status <em>not run</em>. '
            'This is expected when no hardware or simulation target is available, '
            'but worth reviewing if execution environments could be added.',
            body, severity='warn', slug='ins-buildonly'
        ), 'ins-buildonly', 'tag-buildonly', 'BUILD-ONLY',
           f'{n} build-only')
    else:
        pass  # omit from nav when absent

    # --- 4. Flaky / retried tests ---
    if ins['flaky']:
        n = len(ins['flaky'])
        tag = '<span class="insight-label tag-flaky">FLAKY</span>'
        thead = '<tr><th>Test name</th><th>Platform</th><th>Retries</th><th>Final status</th></tr>'
        body_rows = ''.join(
            f'<tr><td><code>{f["name"]}</code></td>'
            f'<td><code>{f["platform"]}</code></td>'
            f'<td>{f["retries"]}</td>'
            f'<td>{_badge(f["status"], 1)}</td></tr>'
            for f in ins['flaky']
        )
        body = f'<table><thead>{thead}</thead><tbody>{body_rows}</tbody></table>'
        _add(_insight_block(
            f'{n} flaky test suite{"s" if n!=1 else ""} (needed retries to pass)',
            tag,
            'These suites passed only after one or more retries, indicating '
            'intermittent failures. Investigate for timing sensitivity, '
            'resource contention, or test isolation issues.',
            body, severity='warn', slug='ins-flaky'
        ), 'ins-flaky', 'tag-flaky', 'FLAKY', f'{n} flaky')
    else:
        _add(_ok_block('No flaky tests (zero retries)'),
             'ins-flaky', '', '✔', 'No flaky tests', is_ok=True)

    # --- 5. Failing tests (multi-platform) ---
    if ins['multi_fail']:
        tag = '<span class="insight-label tag-badfilter">FAILING</span>'
        thead = ('<tr><th>Test name</th><th>Failing platforms</th>'
                 '<th>Count</th></tr>')
        body_rows = ''.join(
            f'<tr><td><code>{f["name"]}</code></td>'
            '<td class="subtle">'
            + ('<br>'.join(f'<code>{p}</code>' for p in f['platforms'][:10])
               + (f'<br><em>+{len(f["platforms"])-10} more</em>'
                  if len(f['platforms']) > 10 else ''))
            + f'</td><td>{f["count"]}</td></tr>'
            for f in ins['multi_fail']
        )
        body = f'<table><thead>{thead}</thead><tbody>{body_rows}</tbody></table>'
        _add(_insight_block(
            f'{len(ins["multi_fail"])} test name{"s" if len(ins["multi_fail"])!=1 else ""} with failures',
            tag,
            'Tests failing across one or more platforms. Failures on many '
            'platforms suggest a code-level issue; failures on a single '
            'platform may indicate a platform-specific regression.',
            body, severity='crit', slug='ins-failing'
        ), 'ins-failing', 'tag-badfilter', 'FAILING',
           f'{len(ins["multi_fail"])} failing')
    else:
        _add(_ok_block('No test failures'),
             'ins-failing', '', '✔', 'No failures', is_ok=True)

    # --- 6. Hot platforms (most failures) ---
    if ins['platform_fail_hot']:
        tag = '<span class="insight-label tag-badfilter">PLATFORM</span>'
        thead = '<tr><th>Platform</th><th>Failing suites</th></tr>'
        body_rows = ''.join(
            f'<tr><td><code>{p["platform"]}</code></td><td>{p["count"]}</td></tr>'
            for p in ins['platform_fail_hot'][:20]
        )
        body = f'<table><thead>{thead}</thead><tbody>{body_rows}</tbody></table>'
        _add(_insight_block(
            'Platforms with most failing suites',
            tag,
            'Platforms where a disproportionate number of suites fail may '
            'indicate a toolchain regression, a driver issue, or a board '
            'support problem.',
            body, severity='crit', slug='ins-platform'
        ), 'ins-platform', 'tag-badfilter', 'PLATFORM',
           f'{len(ins["platform_fail_hot"])} hot board(s)')

    # --- 7. Build-time outliers ---
    if ins['slow_build']:
        n = len(ins['slow_build'])
        tag = '<span class="insight-label tag-slow">SLOW BUILD</span>'
        thresh = ins['bt_mean'] + 2 * ins['bt_sd']
        thead = '<tr><th>Test name</th><th>Platform</th><th>Build time (s)</th><th>σ above mean</th></tr>'
        body_rows = ''.join(
            f'<tr><td><code>{s["name"]}</code></td>'
            f'<td><code>{s["platform"]}</code></td>'
            f'<td>{s["build_time"]:.1f}</td>'
            f'<td class="insight-warn">+{s["z"]:.1f}σ</td></tr>'
            for s in ins['slow_build'][:20]
        )
        body = f'<table><thead>{thead}</thead><tbody>{body_rows}</tbody></table>'
        _add(_insight_block(
            f'{n} build-time outlier{"s" if n!=1 else ""} ('
            f'threshold\u00a0\u2265\u00a0{thresh:.0f}\u00a0s\u00a0=\u00a0mean\u00a0+\u00a02σ)',
            tag,
            f'Build times more than 2σ above the mean '
            f'({ins["bt_mean"]:.0f}\u00a0s avg). '
            'These may slow CI unnecessarily; consider whether dependency '
            'changes or compiler options can reduce build time.',
            body, severity='warn', slug='ins-slowbuild'
        ), 'ins-slowbuild', 'tag-slow', 'SLOW BUILD', f'{n} outlier(s)')
    else:
        _add(_ok_block('No build-time outliers'),
             'ins-slowbuild', '', '✔', 'Build times OK', is_ok=True)

    # --- 8. Execution-time outliers ---
    if ins['slow_exec']:
        n = len(ins['slow_exec'])
        tag = '<span class="insight-label tag-slow">SLOW EXEC</span>'
        thresh = ins['et_mean'] + 2 * ins['et_sd']
        thead = '<tr><th>Test name</th><th>Platform</th><th>Exec time (s)</th><th>σ above mean</th></tr>'
        body_rows = ''.join(
            f'<tr><td><code>{s["name"]}</code></td>'
            f'<td><code>{s["platform"]}</code></td>'
            f'<td>{s["exec_time"]:.1f}</td>'
            f'<td class="insight-warn">+{s["z"]:.1f}σ</td></tr>'
            for s in ins['slow_exec'][:20]
        )
        body = f'<table><thead>{thead}</thead><tbody>{body_rows}</tbody></table>'
        _add(_insight_block(
            f'{n} execution-time outlier{"s" if n!=1 else ""} ('
            f'threshold\u00a0\u2265\u00a0{thresh:.0f}\u00a0s\u00a0=\u00a0mean\u00a0+\u00a02σ)',
            tag,
            f'Execution times more than 2σ above the mean '
            f'({ins["et_mean"]:.1f}\u00a0s avg). '
            'Long-running tests can mask failures and delay CI feedback; '
            'consider adding a tighter timeout or splitting the test.',
            body, severity='warn', slug='ins-slowexec'
        ), 'ins-slowexec', 'tag-slow', 'SLOW EXEC', f'{n} outlier(s)')
    else:
        _add(_ok_block('No execution-time outliers'),
             'ins-slowexec', '', '✔', 'Exec times OK', is_ok=True)

    # --- 9. Low-coverage tests (only one platform) ---
    if ins['low_coverage']:
        n = len(ins['low_coverage'])
        tag = '<span class="insight-label tag-lowcov">LOW COV</span>'
        thead = '<tr><th>Test name</th><th>Only platform</th><th>Status</th></tr>'
        body_rows = ''.join(
            f'<tr><td><code>{lc["name"]}</code></td>'
            f'<td><code>{lc["platform"]}</code></td>'
            f'<td>{_badge(lc["status"], 1)}</td></tr>'
            for lc in ins['low_coverage']
        )
        body = f'<table><thead>{thead}</thead><tbody>{body_rows}</tbody></table>'
        _add(_insight_block(
            f'{n} executed test suite{"s" if n!=1 else ""} with only one platform',
            tag,
            'These tests ran on exactly one platform. If the intent is broader '
            'coverage, consider adding <code>platform_allow</code> entries or '
            'relaxing filter conditions.',
            body, severity='warn', slug='ins-lowcov'
        ), 'ins-lowcov', 'tag-lowcov', 'LOW COV', f'{n} single-platform')
    else:
        _add(_ok_block('All executed tests cover more than one platform'),
             'ins-lowcov', '', '✔', 'Coverage OK', is_ok=True)

    # --- 10. Empty testcase lists ---
    if ins['empty_tc']:
        n = len(ins['empty_tc'])
        tag = '<span class="insight-label tag-badfilter">EMPTY TC</span>'
        thead = '<tr><th>Test name</th><th>Platform</th><th>Status</th></tr>'
        body_rows = ''.join(
            f'<tr><td><code>{e["name"]}</code></td>'
            f'<td><code>{e["platform"]}</code></td>'
            f'<td>{_badge(e["status"], 1)}</td></tr>'
            for e in ins['empty_tc']
        )
        body = f'<table><thead>{thead}</thead><tbody>{body_rows}</tbody></table>'
        _add(_insight_block(
            f'{n} executed suite{"s" if n!=1 else ""} reported no test cases',
            tag,
            'A suite that ran but produced zero test-case results may indicate '
            'a broken test harness, an incorrect Ztest registration, or a '
            'crash before test execution begins.',
            body, severity='crit', slug='ins-emptytc'
        ), 'ins-emptytc', 'tag-badfilter', 'EMPTY TC',
           f'{n} empty result(s)')
    else:
        _add(_ok_block('All executed suites reported at least one test case'),
             'ins-emptytc', '', '✔', 'No empty suites', is_ok=True)

    return '\n'.join(sections) + '\n', nav_items


def _legend() -> str:
    items = ''
    for status, desc in _STATUS_DESC.items():
        color = _STATUS_COLOR.get(status, '#6e7781')
        icon, bg, fg = _STATUS_BADGE.get(status, ('?', color, '#fff'))
        badge = (f'<span class="badge" style="background:{bg};color:{fg}"'
                 f' title="{status}">{icon} {status}</span>')
        items += (
            f'<div class="legend-item">'
            f'{badge}'
            f'<div class="legend-text"><span>{desc}</span></div>'
            f'</div>'
        )
    note = (
        '<p class="subtle" style="margin-top:10px;width:100%">'
        '<strong>Pass rate</strong> is calculated over <em>executed</em> suites only '
        '(passed / (passed + failed + error)).  '
        '\u201cNot run\u201d suites are excluded from the pass-rate denominator '
        'because they were never executed.</p>'
    )
    return f'<h2 id="legend">Status Legend</h2><div class="legend">{items}{note}</div>'


def _failed_table(suites: list[dict]) -> str:
    if not suites:
        return '<h2 id="failures">Failed / Error Suites</h2><p class="subtle">None ✔</p>'
    thead = '<tr><th>Suite</th><th>Platform</th><th>Status</th><th>Failed test cases</th></tr>'
    body = ''
    for ts in suites:
        failed_tcs = [tc['identifier'] for tc in ts.get('testcases', [])
                      if tc.get('status') in ('failed', 'error', 'blocked')]
        tc_html = ('<ul style="padding-left:16px">' +
                   ''.join(f'<li><code>{t}</code></li>' for t in failed_tcs[:20]) +
                   (f'<li class="subtle">… and {len(failed_tcs)-20} more</li>'
                    if len(failed_tcs) > 20 else '') +
                   '</ul>') if failed_tcs else '<span class="subtle">–</span>'
        color = _STATUS_COLOR.get(ts.get('status', ''), '#6e7781')
        status_badge = (f'<span class="badge" style="background:{color};color:#fff">'
                        f'{ts.get("status","")}</span>')
        body += (
            f'<tr>'
            f'<td class="fail-name"><code>{ts.get("name","")}</code></td>'
            f'<td><code>{ts.get("platform","")}</code></td>'
            f'<td>{status_badge}</td>'
            f'<td>{tc_html}</td>'
            f'</tr>'
        )
    return (f'<h2 id="failures">Failed / Error Suites ({len(suites)})</h2>'
            f'<table><thead>{thead}</thead><tbody>{body}</tbody></table>')


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description='Generate an HTML summary report from twister.json files',
        allow_abbrev=False,
    )
    parser.add_argument(
        'inputs',
        nargs='+',
        help='twister.json files or glob patterns',
    )
    parser.add_argument(
        '-o', '--output',
        default='twister_summary.html',
        help='Output HTML file (default: twister_summary.html)',
    )
    parser.add_argument(
        '--title',
        default='Twister Test Run Summary',
        help='Report title',
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    env, testsuites = load_files(args.inputs)

    all_tc = [tc for ts in testsuites for tc in ts.get('testcases', [])]
    suite_counts = status_counts(testsuites, _SUITE_STATUSES)
    tc_counts    = status_counts(all_tc, _CASE_STATUSES)
    suites_ran, suites_notrun = run_notrun_counts(testsuites)

    # Compute insights first so the nav dropdown can be built
    insights_html, insight_items = _insights_html(testsuites)

    html = _html_top(
        args.title, env,
        total_suites=len(testsuites),
        suite_counts=suite_counts,
        tc_total=sum(tc_counts.values()),
        tc_passed=tc_counts.get('passed', 0),
        suites_ran=suites_ran,
        suites_notrun=suites_notrun,
        insight_items=insight_items,
    )
    html += _legend()
    html += insights_html
    html += _name_tree_html(testsuites)
    html += _arch_table(arch_stats(testsuites))
    html += _toolchain_table(toolchain_stats(testsuites))
    html += _board_table(board_stats(testsuites))
    html += _failed_table(failed_suites(testsuites))
    html += '\n</body>\n</html>\n'

    Path(args.output).write_text(html, encoding='utf-8')
    pct = pass_rate(suite_counts)
    print(f"Summary written to {args.output}  "
          f"({len(testsuites)} suites built, {suites_ran} ran, "
          f"{suites_notrun} not run, "
          f"{sum(tc_counts.values())} test cases, "
          f"{pct:.1f}% pass rate over executed)")
    return 0


if __name__ == '__main__':
    sys.exit(main())
