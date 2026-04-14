#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The Zephyr Project
"""
Analyze board-specific overlays (DTS .overlay and Kconfig .conf) found in
boards/ subdirectories under samples/ and tests/.  Produces a self-contained
HTML report containing:
  - Rankings of boards by overlay count
  - Duplicate / near-duplicate detection
  - Pattern identification in DTS and Kconfig overlays
  - Actionable recommendations to reduce boilerplate
"""

import argparse
import collections
import hashlib
import html
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# Data collection
# ---------------------------------------------------------------------------

OverlayFile = collections.namedtuple(
    "OverlayFile", ["path", "board", "kind", "content", "lines", "sha256"]
)


def strip_comments(text: str, kind: str) -> str:
    """Remove comment lines so content comparison is semantic, not syntactic."""
    if kind == "overlay":
        # C-style /* … */ and // comments
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
        text = re.sub(r"//[^\n]*", "", text)
    else:  # .conf
        text = re.sub(r"#[^\n]*", "", text)
    return text


def canonical(text: str) -> str:
    """Normalise whitespace for comparison."""
    return re.sub(r"\s+", " ", text).strip()


def collect_tree_stats(roots: List[Path], zephyr_root: Path) -> dict:
    """
    Count every file under the given roots (samples/ and tests/) so we can
    compute the overlay share of those subtrees.
    Also counts:
    - the number of distinct sample/test directories
    - directories with at least one overlay
    - total files in the entire Zephyr repository (excluding .git and build/)
    """
    total_files = 0
    testcase_dirs: set = set()
    dirs_with_overlay: set = set()

    for root in roots:
        for dirpath, dirnames, filenames in os.walk(root):
            total_files += len(filenames)
            dp = Path(dirpath)
            # A directory is a "test/sample" if it owns a manifest or CMakeLists
            if any(f in filenames for f in ("testcase.yaml", "sample.yaml", "CMakeLists.txt")):
                testcase_dirs.add(dp)
            # Track which parent dirs have a boards/ subdir with overlays
            if dp.name == "boards" and any(
                f.endswith(".overlay") or f.endswith(".conf") for f in filenames
            ):
                dirs_with_overlay.add(dp.parent)

    # Count every file in the whole repo, skipping .git and common build dirs
    _SKIP = {".git", "build", ".west", "__pycache__"}
    whole_tree_files = 0
    for dirpath, dirnames, filenames in os.walk(zephyr_root):
        dirnames[:] = [d for d in dirnames if d not in _SKIP]
        whole_tree_files += len(filenames)

    return {
        "total_files": total_files,
        "testcase_dirs": len(testcase_dirs),
        "dirs_with_overlay": len(dirs_with_overlay),
        "whole_tree_files": whole_tree_files,
    }


def collect_overlays(roots: List[Path]) -> List[OverlayFile]:
    overlays: List[OverlayFile] = []
    for root in roots:
        for dirpath, dirnames, filenames in os.walk(root):
            dp = Path(dirpath)
            # Only files that live directly inside a 'boards' directory
            if dp.name != "boards":
                continue
            for fname in filenames:
                if fname.endswith(".overlay"):
                    kind = "overlay"
                elif fname.endswith(".conf"):
                    kind = "conf"
                else:
                    continue
                fpath = dp / fname
                try:
                    raw = fpath.read_text(errors="replace")
                except OSError:
                    continue
                board = fpath.stem
                # Strip qualifiers like _ns, _s, @X after board name for grouping
                sha = hashlib.sha256(canonical(strip_comments(raw, kind)).encode()).hexdigest()[:12]
                overlays.append(
                    OverlayFile(
                        path=fpath,
                        board=board,
                        kind=kind,
                        content=raw,
                        lines=len(raw.splitlines()),
                        sha256=sha,
                    )
                )
    return overlays


# ---------------------------------------------------------------------------
# Analysis helpers
# ---------------------------------------------------------------------------


def board_base(board: str) -> str:
    """Return coarse board family:  strip trailing _ns, _s, @… suffixes."""
    b = re.sub(r"@[^_]*$", "", board)
    b = re.sub(r"_(ns|s)$", "", b)
    return b


def extract_dts_patterns(overlays: List[OverlayFile]) -> Dict[str, List[OverlayFile]]:
    """Classify DTS overlays by their dominant pattern."""
    patterns: Dict[str, List[OverlayFile]] = collections.defaultdict(list)
    for ov in overlays:
        if ov.kind != "overlay":
            continue
        c = ov.content
        tags = set()
        if re.search(r"partitions\s*\{", c):
            tags.add("flash_partitions")
        if re.search(r"zephyr,flash-controller|zephyr,flash\b|zephyr,sram\b|zephyr,code-partition", c):
            tags.add("memory_layout")
        if re.search(r"status\s*=\s*[\"']disabled[\"']", c):
            tags.add("peripheral_disable")
        if re.search(r"status\s*=\s*[\"']okay[\"']", c):
            tags.add("peripheral_enable")
        if re.search(r"/delete-node/|/delete-property/", c):
            tags.add("delete_node_property")
        if re.search(r"pinctrl|&pinctrl", c):
            tags.add("pinctrl")
        if re.search(r"chosen\s*\{", c):
            tags.add("chosen_node")
        if re.search(r"aliases\s*\{", c):
            tags.add("aliases_node")
        if re.search(r"zephyr,memory-attr|zephyr,memory-region", c):
            tags.add("memory_attr")
        if re.search(r"sram\d*_[ns]s|slot\d+_[ns]s", c):
            tags.add("tfm_sram_split")
        if re.search(r"uart\d+|serial@|usart\d+", c, re.IGNORECASE):
            tags.add("uart_config")
        if re.search(r"i2c\d+|spi\d+|can\d+|pwm\d+", c, re.IGNORECASE):
            tags.add("bus_config")
        if not tags:
            tags.add("other")
        for tag in tags:
            patterns[tag].append(ov)
    return patterns


def extract_kconfig_patterns(overlays: List[OverlayFile]) -> Dict[str, List[OverlayFile]]:
    patterns: Dict[str, List[OverlayFile]] = collections.defaultdict(list)
    for ov in overlays:
        if ov.kind != "conf":
            continue
        c = ov.content
        tags = set()
        for line in c.splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            m = re.match(r"(CONFIG_[A-Z0-9_]+)\s*=\s*(.+)", line)
            if not m:
                continue
            key = m.group(1)
            val = m.group(2).strip()
            # Classify by subsystem
            if any(k in key for k in ["FLASH", "MEMORY", "SRAM", "RAM"]):
                tags.add("memory_storage")
            if any(k in key for k in ["TFM", "TRUSTED"]):
                tags.add("tfm_config")
            if any(k in key for k in ["UART", "SERIAL", "CONSOLE"]):
                tags.add("uart_console")
            if any(k in key for k in ["NET", "ETH", "WIFI", "BT", "BLE", "IEEE802154"]):
                tags.add("networking")
            if any(k in key for k in ["GPIO", "I2C", "SPI", "CAN", "PWM", "ADC", "DAC"]):
                tags.add("peripheral_drivers")
            if any(k in key for k in ["FPU", "CPU", "ARCH", "CORE"]):
                tags.add("cpu_arch")
            if any(k in key for k in ["LOG", "DEBUG", "ASSERT"]):
                tags.add("debug_logging")
            if any(k in key for k in ["HEAP", "STACK", "THREAD", "SCHED"]):
                tags.add("rtos_tuning")
            if "DYNAMIC_OBJECTS" in key or "USERSPACE" in key or "MPU" in key or "MMU" in key:
                tags.add("userspace_mpu")
        if not tags:
            tags.add("other")
        for tag in tags:
            patterns[tag].append(ov)
    return patterns


def find_duplicates(overlays: List[OverlayFile]) -> Dict[str, List[OverlayFile]]:
    """Group overlays that have identical canonical content."""
    groups: Dict[str, List[OverlayFile]] = collections.defaultdict(list)
    for ov in overlays:
        groups[f"{ov.kind}:{ov.sha256}"].append(ov)
    return {k: v for k, v in groups.items() if len(v) > 1}


def find_near_duplicates(
    overlays: List[OverlayFile], threshold: float = 0.85
) -> List[Tuple[OverlayFile, OverlayFile, float]]:
    """
    Detect highly similar (but not identical) overlay pairs using a simple
    token-overlap Jaccard similarity.  Limited to same-kind pairs to keep
    runtime reasonable.
    """

    def tokenise(text: str, kind: str) -> frozenset:
        clean = canonical(strip_comments(text, kind))
        return frozenset(re.split(r"\s+|[{};=,]", clean))

    by_kind: Dict[str, List[OverlayFile]] = collections.defaultdict(list)
    for ov in overlays:
        by_kind[ov.kind].append(ov)

    results = []
    for kind, group in by_kind.items():
        tokens = [(ov, tokenise(ov.content, kind)) for ov in group]
        for i, (ov_a, tok_a) in enumerate(tokens):
            for ov_b, tok_b in tokens[i + 1 :]:
                if ov_a.sha256 == ov_b.sha256:
                    continue  # exact duplicate already handled
                union = tok_a | tok_b
                if not union:
                    continue
                j = len(tok_a & tok_b) / len(union)
                if j >= threshold:
                    results.append((ov_a, ov_b, j))
    # Sort by similarity descending, keep top-200 to avoid huge tables
    results.sort(key=lambda x: -x[2])
    return results[:200]


# ---------------------------------------------------------------------------
# Recommendation engine
# ---------------------------------------------------------------------------

RECOMMENDATIONS = [
    {
        "title": "Use SoC-level or SoC-family overlays for shared silicon",
        "trigger_pattern": "flash_partitions",
        "trigger_kind": "overlay",
        "description": (
            "Many boards sharing the same SoC (e.g., all nRF52840 boards) carry "
            "identical flash-partition tables.  These should be moved to a SoC-level "
            "<code>soc/&lt;vendor&gt;/&lt;family&gt;/boards/</code> overlay or a "
            "<em>shield</em>-style Kconfig fragment that is conditionally applied based "
            "on <code>CONFIG_SOC</code>.  Zephyr's hardware model already supports "
            "<code>soc.yml</code> inheritance; a shared <code>.overlay</code> placed "
            "under the SoC directory is automatically included for all boards using "
            "that SoC."
        ),
    },
    {
        "title": "Replace peripheral-disable overlays with SOC-/board-level DTS",
        "trigger_pattern": "peripheral_disable",
        "trigger_kind": "overlay",
        "description": (
            "Overlays that only set <code>status = \"disabled\"</code> on a peripheral "
            "duplicate information that should live in the board's base DTS file.  "
            "If a peripheral is never usable in a given context (e.g., UART1 always "
            "conflicts with TF-M), the board DTS should reflect that truth directly "
            "instead of requiring every sample/test to carry the same workaround "
            "overlay.  Alternatively, a <em>Zephyr board revision</em> or a "
            "<em>board qualifier</em> (the <code>/ns</code> suffix) can encode the "
            "disabled state."
        ),
    },
    {
        "title": "Consolidate TF-M SRAM partitioning overlays per board",
        "trigger_pattern": "tfm_sram_split",
        "trigger_kind": "overlay",
        "description": (
            "Boards that support TF-M carry near-identical SRAM-split overlays across "
            "multiple samples.  A single canonical overlay in "
            "<code>boards/&lt;vendor&gt;/&lt;board&gt;/</code> named "
            "<code>&lt;board&gt;_ns.overlay</code> (already the convention for the "
            "<code>_ns</code> qualifier) should own this definition.  Individual "
            "samples should include it via CMake "
            "<code>board_overlay()</code> or the west build "
            "<code>--overlay</code> mechanism rather than duplicating the content."
        ),
    },
    {
        "title": "Deduplicate identical .conf fragments into shared Kconfig snippets",
        "trigger_pattern": "tfm_config",
        "trigger_kind": "conf",
        "description": (
            "Many <code>.conf</code> overlays enable or disable the same small set of "
            "TF-M / IPC Kconfig symbols.  Zephyr <em>snippets</em> "
            "(<code>snippets/</code> top-level directory) are purpose-built for this: "
            "a named snippet (e.g., <code>tfm-sfn-level1</code>) can encapsulate those "
            "settings and be referenced in testcase YAML with "
            "<code>extra_args: SNIPPET=tfm-sfn-level1</code>, eliminating per-board "
            "duplicates entirely."
        ),
    },
    {
        "title": "Replace single-symbol .conf overlays with testcase.yaml extra_configs",
        "trigger_pattern": "other",
        "trigger_kind": "conf",
        "description": (
            "Kconfig overlay files that set only one or two symbols are prime "
            "candidates for inline specification.  In <code>testcase.yaml</code> or "
            "<code>sample.yaml</code>, the <code>extra_configs</code> key (or "
            "<code>extra_args: CONFIG_FOO=y</code> in west build) can supply the "
            "symbol without creating a standalone file, reducing file-system clutter "
            "and making the requirement visible directly in the test/sample manifest."
        ),
    },
    {
        "title": "Use pinctrl default states in board DTS instead of overlay pinctrl",
        "trigger_pattern": "pinctrl",
        "trigger_kind": "overlay",
        "description": (
            "Overlays that only configure <code>pinctrl</code> state or add a "
            "<code>&amp;pinctrl</code> node usually encode a hardware truth (which pins "
            "a peripheral uses on that board).  This belongs in the board's own DTS "
            "or DTSI files.  If the peripheral is optional/not present by default, a "
            "<em>board shield</em> or <em>board extension</em> DTS (introduced in "
            "Zephyr 3.5) is the appropriate vehicle."
        ),
    },
    {
        "title": "Leverage chosen / alias nodes at board level for generic drivers",
        "trigger_pattern": "chosen_node",
        "trigger_kind": "overlay",
        "description": (
            "Repeated overlays that only set <code>chosen</code> or "
            "<code>aliases</code> nodes (e.g., directing a sample to use a specific "
            "UART instance as console) should be expressed in the board DTS directly "
            "or through a board-level snippet.  Samples should rely on the "
            "standardised chosen/alias mechanism rather than hard-coding node "
            "references, making them portable without per-board overlays."
        ),
    },
    {
        "title": "Consolidate memory-attribute overlays via Kconfig memory regions",
        "trigger_pattern": "memory_attr",
        "trigger_kind": "overlay",
        "description": (
            "Overlays that patch <code>zephyr,memory-attr</code> or "
            "<code>zephyr,memory-region</code> properties are typically working around "
            "a gap in the board's base DTS.  Addressing the root cause in the board "
            "DTS (or filing a PR to do so) removes the need for per-sample workarounds.  "
            "Where the attribute is truly test-specific, a shared DTSI included by "
            "multiple samples avoids the N-copies problem."
        ),
    },
    {
        "title": "Use CONFIG_SERIAL_INIT_PRIORITY / board-level defaults to avoid UART .conf overlays",
        "trigger_pattern": "uart_console",
        "trigger_kind": "conf",
        "description": (
            "UART / console Kconfig overlays often select a specific UART instance or "
            "baud rate that differs from a board default.  If multiple samples need "
            "the same override, the board's <code>board.cmake</code> or a board-level "
            "<code>Kconfig.defconfig</code> is a better home.  When the need is test- "
            "or sample-specific, a shared snippet covers multiple test cases at once."
        ),
    },
]


def applicable_recommendations(
    dts_patterns: Dict[str, List[OverlayFile]],
    conf_patterns: Dict[str, List[OverlayFile]],
    min_files: int = 5,
) -> List[dict]:
    applicable = []
    for rec in RECOMMENDATIONS:
        src = dts_patterns if rec["trigger_kind"] == "overlay" else conf_patterns
        count = len(src.get(rec["trigger_pattern"], []))
        if count >= min_files:
            applicable.append({**rec, "count": count})
    return applicable


# ---------------------------------------------------------------------------
# HTML generation
# ---------------------------------------------------------------------------

CSS = """
:root {
  --bg: #0d1117; --fg: #c9d1d9; --border: #30363d;
  --accent: #58a6ff; --accent2: #3fb950;
  --warn: #f0883e; --danger: #ff7b72;
  --card: #161b22; --code-bg: #1f2428;
  --bar: #1f6feb;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: var(--bg); color: var(--fg); font: 15px/1.6 "Segoe UI", system-ui, sans-serif; padding: 0 0 40px; }
a { color: var(--accent); text-decoration: none; }
a:hover { text-decoration: underline; }
h1 { font-size: 1.8rem; padding: 32px 40px 8px; color: #fff; }
h2 { font-size: 1.2rem; padding: 28px 40px 10px; border-bottom: 1px solid var(--border); color: #e6edf3; }
h3 { font-size: 1rem; color: var(--accent); margin-bottom: 6px; }
.subtitle { padding: 0 40px 20px; color: #8b949e; font-size: 0.9rem; }
.container { max-width: 1400px; margin: 0 auto; padding: 0 40px; }
.grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-top: 20px; }
.grid-3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: 20px; margin-top: 20px; }
.card { background: var(--card); border: 1px solid var(--border); border-radius: 8px; padding: 20px; }
.stat-row { display: flex; flex-wrap: wrap; gap: 16px; padding: 20px 40px; }
.stat-box { background: var(--card); border: 1px solid var(--border); border-radius: 8px; padding: 16px 24px; flex: 1; min-width: 160px; }
.stat-box .num { font-size: 2rem; font-weight: 700; color: var(--accent); }
.stat-box .label { font-size: 0.82rem; color: #8b949e; }
table { width: 100%; border-collapse: collapse; font-size: 0.88rem; }
th { text-align: left; padding: 8px 12px; background: #1c2128; border-bottom: 2px solid var(--border); color: #8b949e; font-weight: 600; white-space: nowrap; }
td { padding: 7px 12px; border-bottom: 1px solid var(--border); vertical-align: top; }
tr:hover td { background: #1c2230; }
.badge { display: inline-block; padding: 2px 8px; border-radius: 12px; font-size: 0.78rem; font-weight: 600; }
.badge-overlay { background: #1d3461; color: #79c0ff; }
.badge-conf { background: #1e3a1e; color: #56d364; }
.bar-cell { padding: 8px 12px; }
.bar-wrap { background: #21262d; border-radius: 4px; height: 14px; overflow: hidden; }
.bar-inner { height: 100%; background: var(--bar); border-radius: 4px; transition: width .3s; }
.rec { background: var(--card); border: 1px solid var(--border); border-left: 4px solid var(--accent2); border-radius: 8px; padding: 18px 22px; margin-bottom: 14px; }
.rec h3 { color: var(--accent2); margin-bottom: 6px; }
.rec .count-badge { float: right; background: #1e3a1e; color: #56d364; padding: 2px 10px; border-radius: 12px; font-size: 0.8rem; }
.rec p { font-size: 0.88rem; color: var(--fg); line-height: 1.65; }
code { background: var(--code-bg); padding: 1px 5px; border-radius: 4px; font-size: 0.85em; }
.dup-hash { font-family: monospace; font-size: 0.8rem; color: #8b949e; }
.path-list { font-family: monospace; font-size: 0.78rem; color: #8b949e; }
.near-sim { font-size: 0.8rem; font-weight: 600; }
details summary { cursor: pointer; color: var(--accent); font-size: 0.88rem; margin-top: 8px; }
details[open] summary { color: var(--warn); }
nav { background: var(--card); border-bottom: 1px solid var(--border); padding: 12px 40px; display: flex; gap: 20px; flex-wrap: wrap; font-size: 0.88rem; position: sticky; top: 0; z-index: 10; }
.toc-num { color: #8b949e; font-size: 0.82rem; }
.rank-1 td:first-child { color: #ffd700; }
.rank-2 td:first-child { color: #c0c0c0; }
.rank-3 td:first-child { color: #cd7f32; }
.pie-wrap { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 14px; }
.pie-item { display: flex; align-items: center; gap: 8px; font-size: 0.84rem; }
.pie-dot { width: 12px; height: 12px; border-radius: 3px; flex-shrink: 0; }
"""

COLORS = [
    "#58a6ff", "#3fb950", "#f0883e", "#ff7b72", "#d2a8ff",
    "#ffa657", "#79c0ff", "#56d364", "#ff9a8b", "#a5d6ff",
    "#e3b341", "#8b949e", "#bc8cff", "#7ee787", "#ffa28b",
]


def pct_bar(val: int, max_val: int) -> str:
    pct = int(100 * val / max_val) if max_val else 0
    return f'<div class="bar-wrap"><div class="bar-inner" style="width:{pct}%"></div></div>'


def e(text: str) -> str:
    return html.escape(str(text))


def build_html(
    overlays: List[OverlayFile],
    dts_patterns: Dict[str, List[OverlayFile]],
    conf_patterns: Dict[str, List[OverlayFile]],
    duplicates: Dict[str, List[OverlayFile]],
    near_dups: List[Tuple[OverlayFile, OverlayFile, float]],
    recs: List[dict],
    zephyr_root: Path,
    tree_stats: dict,
) -> str:

    total = len(overlays)
    total_dts = sum(1 for o in overlays if o.kind == "overlay")
    total_conf = sum(1 for o in overlays if o.kind == "conf")

    # Board rankings
    board_counter: Dict[str, Dict[str, int]] = collections.defaultdict(
        lambda: {"overlay": 0, "conf": 0}
    )
    for ov in overlays:
        board_counter[ov.board][ov.kind] += 1
    board_ranked = sorted(board_counter.items(), key=lambda x: -(x[1]["overlay"] + x[1]["conf"]))

    # Top-level sample/test paths
    sample_counter: collections.Counter = collections.Counter()
    for ov in overlays:
        # Determine which sample/test this overlay belongs to
        parts = ov.path.parts
        for i, p in enumerate(parts):
            if p in ("samples", "tests") and i + 1 < len(parts):
                # Use up to 3 path components for the test/sample name
                sample_counter["/".join(parts[i : min(i + 4, len(parts) - 2)])] += 1
                break

    sample_ranked = sample_counter.most_common(40)

    # Dup stats
    total_dup_files = sum(len(v) for v in duplicates.values())
    dup_groups = len(duplicates)

    # Unique boards
    unique_boards = len(board_counter)

    # Tree-wide stats
    total_tree_files = tree_stats["total_files"]
    whole_tree_files = tree_stats["whole_tree_files"]
    total_testcase_dirs = tree_stats["testcase_dirs"]
    dirs_with_overlay = tree_stats["dirs_with_overlay"]
    overlay_share_pct = 100.0 * total / total_tree_files if total_tree_files else 0.0
    overlay_share_whole_pct = 100.0 * total / whole_tree_files if whole_tree_files else 0.0
    avg_overlays = total / dirs_with_overlay if dirs_with_overlay else 0.0
    avg_per_testcase = total / total_testcase_dirs if total_testcase_dirs else 0.0
    coverage_pct = 100.0 * dirs_with_overlay / total_testcase_dirs if total_testcase_dirs else 0.0

    parts_html: List[str] = []

    # ---- STATS HEADER
    parts_html.append(f"""
<div class="stat-row">
  <div class="stat-box"><div class="num">{total:,}</div><div class="label">Total overlay files</div></div>
  <div class="stat-box"><div class="num">{total_dts:,}</div><div class="label">DTS .overlay files</div></div>
  <div class="stat-box"><div class="num">{total_conf:,}</div><div class="label">Kconfig .conf files</div></div>
  <div class="stat-box"><div class="num">{unique_boards:,}</div><div class="label">Unique boards referenced</div></div>
  <div class="stat-box"><div class="num">{whole_tree_files:,}</div><div class="label">Total files in repo</div></div>
  <div class="stat-box"><div class="num">{total_tree_files:,}</div><div class="label">Total files in samples+tests</div></div>
  <div class="stat-box"><div class="num">{overlay_share_whole_pct:.1f}%</div><div class="label">Overlay share of whole repo</div></div>
  <div class="stat-box"><div class="num">{overlay_share_pct:.1f}%</div><div class="label">Overlay share of samples+tests</div></div>
  <div class="stat-box"><div class="num">{total_testcase_dirs:,}</div><div class="label">Sample/test directories</div></div>
  <div class="stat-box"><div class="num">{dirs_with_overlay:,}</div><div class="label">Have ≥1 board overlay</div></div>
  <div class="stat-box"><div class="num">{coverage_pct:.1f}%</div><div class="label">Coverage (dirs with overlays)</div></div>
  <div class="stat-box"><div class="num">{avg_overlays:.1f}</div><div class="label">Avg overlays / dir with overlays</div></div>
  <div class="stat-box"><div class="num">{avg_per_testcase:.2f}</div><div class="label">Avg overlays / all sample+test dirs</div></div>
  <div class="stat-box"><div class="num">{dup_groups:,}</div><div class="label">Exact-duplicate groups</div></div>
  <div class="stat-box"><div class="num">{total_dup_files:,}</div><div class="label">Files in duplicate groups</div></div>
  <div class="stat-box"><div class="num">{len(near_dups):,}</div><div class="label">Near-duplicate pairs (≥85%)</div></div>
</div>""")

    # ---- SECTION 0: Tree-wide statistics detail
    parts_html.append('<h2 id="s0"><span class="toc-num">0 · </span>Tree-Wide Statistics</h2>')
    parts_html.append('<div class="container">')

    # Distribution of overlays-per-dir (among dirs that have any)
    dir_overlay_counts: collections.Counter = collections.Counter()
    for ov in overlays:
        parts = ov.path.parts
        for i, p in enumerate(parts):
            if p in ("samples", "tests") and i + 1 < len(parts):
                key = "/".join(parts[: len(parts) - 2])  # parent of boards/
                dir_overlay_counts[key] += 1
                break
    count_dist: collections.Counter = collections.Counter(dir_overlay_counts.values())
    dist_rows = "".join(
        f"<tr><td>{n}</td><td>{count_dist[n]:,}</td>"
        f"<td>{100*count_dist[n]/dirs_with_overlay:.1f}%</td></tr>"
        for n in sorted(count_dist)
    )

    # Percentile stats
    vals = sorted(dir_overlay_counts.values())
    p50 = vals[len(vals) // 2] if vals else 0
    p90 = vals[int(len(vals) * 0.9)] if vals else 0
    p99 = vals[int(len(vals) * 0.99)] if vals else 0
    max_ov = vals[-1] if vals else 0
    max_dir = max(dir_overlay_counts, key=dir_overlay_counts.get) if dir_overlay_counts else ""

    parts_html.append(f"""
<div class="grid-2" style="margin-top:20px">
  <div class="card">
    <h3>Overlay file share of the tree</h3>
    <p style="margin-top:10px;font-size:0.9rem;color:#8b949e">
      The Zephyr repository contains
      <strong style="color:var(--fg)">{whole_tree_files:,}</strong> total files
      (excluding <code>.git/</code> and <code>build/</code>).
      Board-specific overlay files account for
      <strong style="color:var(--warn)">{total:,}</strong>
      — <strong style="color:var(--warn)">{overlay_share_whole_pct:.1f}%</strong>
      of the <em>entire repo</em>.<br><br>
      Within <code>samples/</code> and <code>tests/</code> alone
      ({total_tree_files:,} files), the share rises to
      <strong style="color:var(--warn)">{overlay_share_pct:.1f}%</strong>.<br><br>
      DTS overlays: <strong style="color:#79c0ff">{total_dts:,}</strong>&nbsp;
      ({100*total_dts/total:.1f}% of overlays) ·
      Kconfig overlays: <strong style="color:#56d364">{total_conf:,}</strong>&nbsp;
      ({100*total_conf/total:.1f}% of overlays).
    </p>
  </div>
  <div class="card">
    <h3>Samples / tests coverage</h3>
    <p style="margin-top:10px;font-size:0.9rem;color:#8b949e">
      <strong style="color:var(--fg)">{total_testcase_dirs:,}</strong> directories contain a
      <code>CMakeLists.txt</code>, <code>testcase.yaml</code>, or <code>sample.yaml</code>.
      Of these, <strong style="color:var(--warn)">{dirs_with_overlay:,}</strong>
      (<strong style="color:var(--warn)">{coverage_pct:.1f}%</strong>) have at least one
      board-specific overlay.<br><br>
      Average overlays among dirs <em>with</em> overlays: <strong style="color:var(--accent)">{avg_overlays:.1f}</strong><br>
      Average overlays across <em>all</em> dirs: <strong style="color:var(--accent)">{avg_per_testcase:.2f}</strong>
    </p>
  </div>
</div>
<div class="card" style="margin-top:20px">
  <h3>Overlay count per-directory — percentiles</h3>
  <p style="margin-top:8px;font-size:0.88rem;color:#8b949e">
    Median (p50): <strong style="color:var(--accent)">{p50}</strong> &nbsp;·&nbsp;
    p90: <strong style="color:var(--warn)">{p90}</strong> &nbsp;·&nbsp;
    p99: <strong style="color:var(--danger)">{p99}</strong> &nbsp;·&nbsp;
    Max: <strong style="color:var(--danger)">{max_ov}</strong>
    (<code style="font-size:0.8rem">{e(max_dir)}</code>)
  </p>
</div>
<details style="margin-top:18px">
  <summary>Distribution table — overlay count per directory</summary>
  <table style="margin-top:10px;max-width:500px">
    <thead><tr><th># overlays in dir</th><th>Directories</th><th>% of dirs with overlays</th></tr></thead>
    <tbody>{dist_rows}</tbody>
  </table>
</details>
""")
    parts_html.append("</div>")

    # ---- SECTION 1: Board ranking
    parts_html.append('<h2 id="s1"><span class="toc-num">1 · </span>Board Overlay Ranking</h2>')
    parts_html.append('<div class="container">')
    max_total = board_ranked[0][1]["overlay"] + board_ranked[0][1]["conf"] if board_ranked else 1
    rows = []
    for rank, (board, counts) in enumerate(board_ranked[:100], 1):
        t = counts["overlay"] + counts["conf"]
        row_cls = f"rank-{rank}" if rank <= 3 else ""
        rows.append(
            f'<tr class="{row_cls}">'
            f"<td>{rank}</td>"
            f"<td>{e(board)}</td>"
            f'<td><span class="badge badge-overlay">{counts["overlay"]}</span></td>'
            f'<td><span class="badge badge-conf">{counts["conf"]}</span></td>'
            f"<td>{t}</td>"
            f'<td class="bar-cell">{pct_bar(t, max_total)}</td>'
            f"</tr>"
        )
    parts_html.append(
        '<table><thead><tr>'
        "<th>#</th><th>Board</th><th>.overlay</th><th>.conf</th><th>Total</th><th style='width:200px'>Bar</th>"
        f"</tr></thead><tbody>{''.join(rows)}</tbody></table>"
    )
    if len(board_ranked) > 100:
        parts_html.append(
            f"<p style='margin-top:10px;color:#8b949e;font-size:.85rem'>Showing top 100 of {len(board_ranked)} boards.</p>"
        )
    parts_html.append("</div>")

    # ---- SECTION 2: Sample/test ranking
    parts_html.append('<h2 id="s2"><span class="toc-num">2 · </span>Samples & Tests by Overlay Count</h2>')
    parts_html.append('<div class="container">')
    max_s = sample_ranked[0][1] if sample_ranked else 1
    rows = []
    for rank, (sname, cnt) in enumerate(sample_ranked, 1):
        rows.append(
            f"<tr><td>{rank}</td><td><code>{e(sname)}</code></td>"
            f"<td>{cnt}</td>"
            f'<td class="bar-cell">{pct_bar(cnt, max_s)}</td></tr>'
        )
    parts_html.append(
        '<table><thead><tr>'
        "<th>#</th><th>Sample / Test path</th><th>Overlays</th><th style='width:200px'>Bar</th>"
        f"</tr></thead><tbody>{''.join(rows)}</tbody></table>"
    )
    parts_html.append("</div>")

    # ---- SECTION 3: DTS pattern breakdown
    parts_html.append('<h2 id="s3"><span class="toc-num">3 · </span>DTS Overlay Pattern Analysis</h2>')
    parts_html.append('<div class="container">')
    dts_sorted = sorted(dts_patterns.items(), key=lambda x: -len(x[1]))
    max_dp = len(dts_sorted[0][1]) if dts_sorted else 1
    rows = []
    for i, (pat, files) in enumerate(dts_sorted):
        color = COLORS[i % len(COLORS)]
        rows.append(
            f"<tr><td><span style='color:{color};font-weight:600'>{e(pat.replace('_',' '))}</span></td>"
            f"<td>{len(files)}</td>"
            f'<td class="bar-cell">{pct_bar(len(files), max_dp)}</td></tr>'
        )
    legend = "".join(
        f'<div class="pie-item"><div class="pie-dot" style="background:{COLORS[i % len(COLORS)]}"></div>'
        f'<span>{e(pat.replace("_"," "))} ({len(files)})</span></div>'
        for i, (pat, files) in enumerate(dts_sorted)
    )
    parts_html.append(f'<div class="pie-wrap">{legend}</div>')
    parts_html.append(
        '<table style="margin-top:14px"><thead><tr>'
        "<th>Pattern</th><th>File count</th><th style='width:200px'>Bar</th>"
        f"</tr></thead><tbody>{''.join(rows)}</tbody></table>"
    )
    parts_html.append(
        "<p style='margin-top:10px;color:#8b949e;font-size:.85rem'>"
        "Note: a single file may match multiple patterns; counts are non-exclusive.</p>"
    )
    parts_html.append("</div>")

    # ---- SECTION 4: Kconfig pattern breakdown
    parts_html.append('<h2 id="s4"><span class="toc-num">4 · </span>Kconfig Overlay Pattern Analysis</h2>')
    parts_html.append('<div class="container">')
    conf_sorted = sorted(conf_patterns.items(), key=lambda x: -len(x[1]))
    max_cp = len(conf_sorted[0][1]) if conf_sorted else 1
    rows = []
    for i, (pat, files) in enumerate(conf_sorted):
        color = COLORS[i % len(COLORS)]
        rows.append(
            f"<tr><td><span style='color:{color};font-weight:600'>{e(pat.replace('_',' '))}</span></td>"
            f"<td>{len(files)}</td>"
            f'<td class="bar-cell">{pct_bar(len(files), max_cp)}</td></tr>'
        )
    legend = "".join(
        f'<div class="pie-item"><div class="pie-dot" style="background:{COLORS[i % len(COLORS)]}"></div>'
        f'<span>{e(pat.replace("_"," "))} ({len(files)})</span></div>'
        for i, (pat, files) in enumerate(conf_sorted)
    )
    parts_html.append(f'<div class="pie-wrap">{legend}</div>')
    parts_html.append(
        '<table style="margin-top:14px"><thead><tr>'
        "<th>Pattern</th><th>File count</th><th style='width:200px'>Bar</th>"
        f"</tr></thead><tbody>{''.join(rows)}</tbody></table>"
    )
    parts_html.append("</div>")

    # ---- SECTION 5: Exact duplicates
    parts_html.append('<h2 id="s5"><span class="toc-num">5 · </span>Exact Duplicate Overlay Groups</h2>')
    parts_html.append('<div class="container">')
    dup_sorted = sorted(duplicates.items(), key=lambda x: -len(x[1]))
    rows = []
    for key, files in dup_sorted[:80]:
        kind_label, sha = key.split(":", 1)
        badge = f'<span class="badge badge-{"overlay" if kind_label=="overlay" else "conf"}">{kind_label}</span>'
        example = files[0]
        # Snippet of content (first non-comment, non-blank line)
        snippet = ""
        for line in example.content.splitlines():
            stripped = line.strip()
            if stripped and not stripped.startswith(("/", "#", "*")):
                snippet = stripped[:80]
                break
        path_list = "<br>".join(
            e(str(f.path.relative_to(zephyr_root))) for f in files[:8]
        )
        if len(files) > 8:
            path_list += f"<br><em>…and {len(files)-8} more</em>"
        rows.append(
            f"<tr><td>{badge}</td>"
            f'<td class="dup-hash">{sha}</td>'
            f"<td>{len(files)}</td>"
            f"<td><code>{e(snippet)}</code></td>"
            f'<td class="path-list">{path_list}</td></tr>'
        )
    parts_html.append(
        '<table><thead><tr>'
        "<th>Kind</th><th>Hash</th><th>Copies</th><th>Content preview</th><th>Files</th>"
        f"</tr></thead><tbody>{''.join(rows)}</tbody></table>"
    )
    if len(dup_sorted) > 80:
        parts_html.append(
            f"<p style='margin-top:10px;color:#8b949e;font-size:.85rem'>Showing 80 of {len(dup_sorted)} groups.</p>"
        )
    parts_html.append("</div>")

    # ---- SECTION 6: Near duplicates
    parts_html.append('<h2 id="s6"><span class="toc-num">6 · </span>Near-Duplicate Overlay Pairs (Jaccard ≥ 85%)</h2>')
    parts_html.append('<div class="container">')
    rows = []
    for ov_a, ov_b, sim in near_dups[:100]:
        badge = f'<span class="badge badge-{"overlay" if ov_a.kind=="overlay" else "conf"}">{ov_a.kind}</span>'
        rows.append(
            f"<tr><td>{badge}</td>"
            f'<td class="near-sim" style="color:{"var(--accent2)" if sim >= 0.95 else "var(--warn)"}">{sim:.0%}</td>'
            f'<td class="path-list">{e(str(ov_a.path.relative_to(zephyr_root)))}</td>'
            f'<td class="path-list">{e(str(ov_b.path.relative_to(zephyr_root)))}</td>'
            f"</tr>"
        )
    parts_html.append(
        '<table><thead><tr>'
        "<th>Kind</th><th>Similarity</th><th>File A</th><th>File B</th>"
        f"</tr></thead><tbody>{''.join(rows)}</tbody></table>"
    )
    if len(near_dups) > 100:
        parts_html.append(
            f"<p style='margin-top:10px;color:#8b949e;font-size:.85rem'>Showing top 100 of {len(near_dups)} pairs.</p>"
        )
    parts_html.append("</div>")

    # ---- SECTION 7: Recommendations
    parts_html.append('<h2 id="s7"><span class="toc-num">7 · </span>Recommendations to Reduce Board Overlays</h2>')
    parts_html.append('<div class="container">')
    for rec in recs:
        parts_html.append(
            f'<div class="rec">'
            f'<span class="count-badge">{rec["count"]} files affected</span>'
            f'<h3>{e(rec["title"])}</h3>'
            f'<p>{rec["description"]}</p>'
            f"</div>"
        )
    if not recs:
        parts_html.append("<p>No pattern met the minimum threshold for a recommendation.</p>")
    parts_html.append("</div>")

    # ---- SECTION 8: Top N single-option .conf files (just CONFIG_X=y)
    small_conf = [
        ov for ov in overlays
        if ov.kind == "conf"
        and sum(
            1 for l in ov.content.splitlines()
            if l.strip() and not l.strip().startswith("#")
        ) == 1
    ]
    parts_html.append('<h2 id="s8"><span class="toc-num">8 · </span>Single-Symbol Kconfig Overlays</h2>')
    parts_html.append('<div class="container">')
    parts_html.append(
        f"<p style='margin-bottom:14px;color:#8b949e'>There are <strong style='color:var(--warn)'>{len(small_conf)}</strong> "
        f".conf overlay files that contain exactly one active Kconfig line. These are "
        f"the lowest-hanging fruit for consolidation via <code>extra_configs</code> in "
        f"testcase/sample YAML, or a named snippet.</p>"
    )
    sym_counter: collections.Counter = collections.Counter()
    for ov in small_conf:
        for line in ov.content.splitlines():
            line = line.strip()
            if line and not line.startswith("#"):
                sym_counter[line] += 1
    rows = []
    for sym, cnt in sym_counter.most_common(50):
        rows.append(f"<tr><td><code>{e(sym)}</code></td><td>{cnt}</td></tr>")
    parts_html.append(
        '<table><thead><tr>'
        "<th>Symbol assignment</th><th>Occurrences in single-line .conf files</th>"
        f"</tr></thead><tbody>{''.join(rows)}</tbody></table>"
    )
    parts_html.append("</div>")

    # ---- SECTION 9: Community discussion & insights from GitHub issues
    # Quantify a few data points to tie into the discussion
    # Label-only DTS overlays (only adds aliases / chosen / nodelabels, no real HW)
    label_only = [
        ov for ov in overlays
        if ov.kind == "overlay"
        and not re.search(r"partitions\s*\{|status\s*=|reg\s*=|compatible\s*=|#address-cells|interrupts\s*=|pinctrl", ov.content)
        and re.search(r"aliases\s*\{|chosen\s*\{|:\s*\w+@|zephyr_\w+\s*=|&\w+\s*\{", ov.content)
    ]
    # Fake-hardware overlays: add gpio-keys or leds nodes with a free GPIO
    fake_hw = [
        ov for ov in overlays
        if ov.kind == "overlay"
        and re.search(r"gpio-keys|gpio_keys|pwm-leds|pwm_leds", ov.content)
        and re.search(r"label\s*=\s*[\"'](sw0|button0|led0)", ov.content)
    ]
    # Build-coverage overlays: tiny overlays that just enable a peripheral node
    build_cov = [
        ov for ov in overlays
        if ov.kind == "overlay"
        and ov.lines < 20
        and re.search(r"status\s*=\s*[\"']okay[\"']", ov.content)
        and not re.search(r"partitions|pinctrl|chosen|aliases", ov.content)
    ]
    # PM-related .conf overlays (enable power management unexpectedly)
    pm_conf = [
        ov for ov in overlays
        if ov.kind == "conf"
        and re.search(r"CONFIG_PM\b|CONFIG_POWER_MANAGEMENT|CONFIG_PM_DEVICE", ov.content)
    ]
    # Count unique soc prefixes in board names to estimate soc-level consolidation potential
    soc_prefix_count: collections.Counter = collections.Counter()
    for ov in overlays:
        # Rough SoC extraction: take first two '_'-delimited tokens
        parts_b = ov.board.split("_")
        prefix = "_".join(parts_b[:2]) if len(parts_b) >= 2 else parts_b[0]
        soc_prefix_count[prefix] += 1
    top_soc_prefixes = soc_prefix_count.most_common(20)
    soc_rows = "".join(
        f"<tr><td><code>{e(p)}</code></td><td>{c}</td></tr>"
        for p, c in top_soc_prefixes
    )

    parts_html.append('<h2 id="s9"><span class="toc-num">9 · </span>Community Discussion &amp; Insights</h2>')
    parts_html.append('<div class="container">')
    parts_html.append(f"""
<p style="margin-bottom:18px;color:#8b949e;font-size:0.9rem">
  The following section synthesises the discussions from three GitHub issues that
  directly address the overlay proliferation problem, cross-referenced with the
  quantitative data from this scan.
</p>

<!-- Issue cards -->
<div class="grid-3" style="margin-bottom:24px">
  <div class="card" style="border-left:4px solid #58a6ff">
    <h3 style="margin-bottom:6px">
      <a href="https://github.com/zephyrproject-rtos/zephyr/issues/90750" target="_blank">#90750</a>
      &nbsp;Scaling of board overlay mechanism
    </h3>
    <p style="font-size:0.83rem;color:#8b949e">
      Raised by Testing WG. Highlighted extreme cases: uart_async_api (142 board files),
      dma/chan_blen_transfer (173 board files).  Proposed hierarchical discovery model
      (4-tier shared placement), SoC/series/family overlays, and board-owned snippets.
      @nordicjm rejected pure hierarchy; @nashif noted overlays moved complexity from
      <code>#ifdef</code>s without solving the root cause.
    </p>
  </div>
  <div class="card" style="border-left:4px solid #3fb950">
    <h3 style="margin-bottom:6px">
      <a href="https://github.com/zephyrproject-rtos/zephyr/issues/81458" target="_blank">#81458</a>
      &nbsp;hwmv2: series/ and families/ folders
    </h3>
    <p style="font-size:0.83rem;color:#8b949e">
      Proposed by @erwango. Add <code>series/</code> and <code>families/</code>
      application folders (analogous to <code>socs/</code>) so one family overlay
      replaces N board overlays. @tejlmand raised open questions: file ordering,
      build-without-Kconfig-parsing requirement, and discoverability complexity.
      @mbolivar suggested nesting under <code>socs/&lt;vendor&gt;/</code> to avoid
      global naming clashes.  Architecture review requested; no PR yet.
    </p>
  </div>
  <div class="card" style="border-left:4px solid #f0883e">
    <h3 style="margin-bottom:6px">
      <a href="https://github.com/zephyrproject-rtos/zephyr/issues/102912" target="_blank">#102912</a>
      &nbsp;Reduce overlays — strict policy
    </h3>
    <p style="font-size:0.83rem;color:#8b949e">
      Proposed by @jfischer-no. Policy-first approach: ban trivial 1&ndash;3 line
      overlays, ban label-only DTS overlays, ban feature-faking.  Detailed decision
      tree and reviewer checklist contributed by @hakehuang.
      Discussed in Process WG 2026-02-04.  Broad agreement on the problem; @nashif
      flagged that documenting guidelines alone did not stop inflow in the past.
    </p>
  </div>
</div>

<!-- Key insights grid -->
<h3 style="margin:20px 0 10px">Key insights cross-referenced with scan data</h3>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:16px">

  <div class="card">
    <h3>Insight 1 · Single-line overlays are the biggest quick win</h3>
    <p style="font-size:0.87rem;color:#8b949e;margin-top:8px">
      Issue #102912 directly calls out trivial 1&ndash;3 line Kconfig overlays as
      "not allowed" — use <code>extra_configs</code> in YAML or fix upstream defaults
      instead.  Our scan found <strong style="color:var(--warn)">{len(small_conf):,}</strong>
      single-symbol <code>.conf</code> files.  These are zero-effort wins that can be
      converted today without any build-system changes.
    </p>
  </div>

  <div class="card">
    <h3>Insight 2 · Label-only DTS overlays should not exist</h3>
    <p style="font-size:0.87rem;color:#8b949e;margin-top:8px">
      Issue #102912 §2.3 prohibits overlays whose only purpose is to add a nodelabel,
      <code>aliases</code>, or <code>chosen</code> entry.  The fix is to standardise
      "well-known labels" (e.g. <code>zephyr_i2c</code>, <code>zephyr_udc0</code>)
      in board DTS files.  Our scan estimates
      <strong style="color:var(--warn)">{len(label_only):,}</strong> DTS overlays
      match this profile.  PR <a href="https://github.com/zephyrproject-rtos/zephyr/pull/102991" target="_blank">#102991</a>
      (mentioned in Process WG) shows a single alias in a board file can eliminate
      one overlay per sample.
    </p>
  </div>

  <div class="card">
    <h3>Insight 3 · Power-management .conf overlays indicate bad board defaults</h3>
    <p style="font-size:0.87rem;color:#8b949e;margin-top:8px">
      @nashif noted in the Process WG that overlays enabling
      <code>CONFIG_PM</code> / <code>CONFIG_PM_DEVICE</code> in samples are a red
      flag — they indicate the board default config is wrong, not the sample.
      Our scan found <strong style="color:var(--warn)">{len(pm_conf):,}</strong>
      board-specific <code>.conf</code> files that touch power-management symbols.
      Each should be investigated and the setting pushed to the board
      <code>Kconfig.defconfig</code>.
    </p>
  </div>

  <div class="card">
    <h3>Insight 4 · Feature-faking overlays should be banned</h3>
    <p style="font-size:0.87rem;color:#8b949e;margin-top:8px">
      Issue #102912 §2.4 explicitly forbids "faking" absent hardware — e.g. mapping
      a free GPIO as <code>led0</code> or <code>sw0</code> so a sample "works".
      Our scan found approximately
      <strong style="color:var(--warn)">{len(fake_hw):,}</strong> overlays that
      define <code>gpio-keys</code> / <code>pwm-leds</code> nodes labelled
      <code>sw0</code>/<code>led0</code>/<code>button0</code>.  The correct fix is
      <code>platform_allow</code> restriction or removal of the overlay.
    </p>
  </div>

  <div class="card">
    <h3>Insight 5 · Build-coverage overlays belong in tests/drivers/build_all/</h3>
    <p style="font-size:0.87rem;color:#8b949e;margin-top:8px">
      @nordicjm and @jfischer-no agree: overlays that exist only to force
      compilation of a driver not enabled by any board should use
      <code>tests/drivers/build_all/</code> instead of polluting the sample
      <code>boards/</code> directory.  Our scan found roughly
      <strong style="color:var(--warn)">{len(build_cov):,}</strong> small DTS overlays
      (under 20 lines) whose only effect is <code>status = "okay"</code> on a node —
      the classic build-coverage pattern.
    </p>
  </div>

  <div class="card">
    <h3>Insight 6 · SoC/family/series overlay grouping has community support but no PR yet</h3>
    <p style="font-size:0.87rem;color:#8b949e;margin-top:8px">
      Issue #81458 proposes <code>families/stm32.overlay</code> replacing 18 board
      files for the DAC sample alone.  @mbolivar, @erwango, and @ajf58 support it;
      @tejlmand requests resolution of ordering and discoverability questions first.
      The table below shows the top board-name prefixes (rough SoC/family groupings)
      in overlay files — these are the families where consolidation would have the
      largest immediate impact.
    </p>
  </div>

  <div class="card">
    <h3>Insight 7 · Exact duplicates are the clearest proof of the problem</h3>
    <p style="font-size:0.87rem;color:#8b949e;margin-top:8px">
      @johnlange2 (issue #90750) found 32% of overlay files are exact duplicates —
      consistent with our scan's
      <strong style="color:var(--warn)">{total_dup_files:,}</strong> files across
      <strong style="color:var(--warn)">{dup_groups:,}</strong> duplicate groups.
      @nordicjm argued git storage is not the issue; the real cost is maintenance
      divergence risk.  @tejlmand suggested tooling to detect drift among duplicates
      as a lower-risk alternative to hierarchical discovery.
    </p>
  </div>

  <div class="card">
    <h3>Insight 8 · Process and policy enforcement are prerequisites</h3>
    <p style="font-size:0.87rem;color:#8b949e;margin-top:8px">
      @nashif noted (issue #102912) that documenting guidelines didn't stop inflow.
      @jfischer-no and @hakehuang therefore proposed a <strong>CI lint gate</strong>:
      a presubmit check that fails when a new trivial / label-only overlay is added
      without a documented justification block.  Without enforcement, any
      consolidation effort will be undone by continued inflow.  The decision tree
      in issue #102912 provides a ready-to-use reviewer checklist.
    </p>
  </div>

</div>

<!-- SOC prefix rank table -->
<h3 style="margin:24px 0 10px">Top 20 board-name prefixes by overlay count (SoC/family consolidation potential)</h3>
<p style="font-size:0.85rem;color:#8b949e;margin-bottom:10px">
  Each prefix is a rough proxy for a SoC or board family.  All overlays under
  a prefix with the same content could be replaced with a single
  <code>families/</code> or <code>socs/</code> file if issue #81458 is implemented.
</p>
<table style="max-width:500px">
  <thead><tr><th>Board-name prefix (≈ SoC family)</th><th>Overlay files</th></tr></thead>
  <tbody>{soc_rows}</tbody>
</table>

<!-- Proposed approaches summary table -->
<h3 style="margin:24px 0 10px">Summary of proposed approaches from community discussions</h3>
<table>
  <thead><tr>
    <th>Approach</th><th>Issue(s)</th><th>Status</th><th>Data-backed impact</th><th>Concerns raised</th>
  </tr></thead>
  <tbody>
    <tr>
      <td><strong>Ban trivial 1–3 line .conf overlays → <code>extra_configs</code></strong></td>
      <td><a href="https://github.com/zephyrproject-rtos/zephyr/issues/102912" target="_blank">#102912</a></td>
      <td><span style="color:#3fb950">Policy proposed, no PR</span></td>
      <td>{len(small_conf):,} single-line .conf files immediately actionable</td>
      <td>Requires enforcement (CI lint) or guidelines will be ignored</td>
    </tr>
    <tr>
      <td><strong>Well-known nodelabels in board DTS (<code>zephyr_i2c</code>, etc.)</strong></td>
      <td><a href="https://github.com/zephyrproject-rtos/zephyr/issues/102912" target="_blank">#102912</a></td>
      <td><span style="color:#3fb950">Partially done, expanding</span></td>
      <td>~{len(label_only):,} label-only DTS overlays removable</td>
      <td>Requires registry doc + board DTS PRs per label added</td>
    </tr>
    <tr>
      <td><strong>families/ and series/ overlay directories</strong></td>
      <td><a href="https://github.com/zephyrproject-rtos/zephyr/issues/81458" target="_blank">#81458</a></td>
      <td><span style="color:#f0883e">Under architecture review</span></td>
      <td>Largest potential; top prefix <code>{top_soc_prefixes[0][0] if top_soc_prefixes else "?"}</code> has {top_soc_prefixes[0][1] if top_soc_prefixes else 0} overlays alone</td>
      <td>File ordering, build-without-parsing, discoverability for newcomers</td>
    </tr>
    <tr>
      <td><strong>Board-owned Zephyr snippets reused across tests</strong></td>
      <td><a href="https://github.com/zephyrproject-rtos/zephyr/issues/90750" target="_blank">#90750</a></td>
      <td><span style="color:#f0883e">POC done (sdk-nrf), drawbacks noted</span></td>
      <td>Would eliminate per-sample duplication for board-wide config</td>
      <td>Snippet discovery complexity; sdk-nrf POC showed major drawbacks</td>
    </tr>
    <tr>
      <td><strong>Hierarchical overlay discovery in CMake (4-tier)</strong></td>
      <td><a href="https://github.com/zephyrproject-rtos/zephyr/issues/90750" target="_blank">#90750</a></td>
      <td><span style="color:#ff7b72">Rejected by maintainers</span></td>
      <td>70–90% file count reduction claimed</td>
      <td>Too confusing; git storage not the real cost per @nordicjm</td>
    </tr>
    <tr>
      <td><strong>Move build-coverage overlays to tests/drivers/build_all/</strong></td>
      <td><a href="https://github.com/zephyrproject-rtos/zephyr/issues/102912" target="_blank">#102912</a></td>
      <td><span style="color:#3fb950">Agreed by maintainers</span></td>
      <td>~{len(build_cov):,} small enable-only DTS overlays are candidates</td>
      <td>Requires manual triage per driver area</td>
    </tr>
    <tr>
      <td><strong>CI lint gate: reject new trivial / label-only overlays</strong></td>
      <td><a href="https://github.com/zephyrproject-rtos/zephyr/issues/102912" target="_blank">#102912</a></td>
      <td><span style="color:#f0883e">Proposed, not implemented</span></td>
      <td>Prevents future inflow; current stock still needs cleanup</td>
      <td>Needs agreement on "trivial" definition and failure criteria</td>
    </tr>
    <tr>
      <td><strong>PM / board-default config corrections (push to board Kconfig)</strong></td>
      <td><a href="https://github.com/zephyrproject-rtos/zephyr/issues/102912" target="_blank">#102912</a></td>
      <td><span style="color:#f0883e">Identified, no systematic effort</span></td>
      <td>{len(pm_conf):,} PM-related .conf overlays indicate bad board defaults</td>
      <td>Requires per-board investigation; vendor buy-in needed</td>
    </tr>
    <tr>
      <td><strong>samples/v2 + tests/v2 sandbox for clean-room trials</strong></td>
      <td><a href="https://github.com/zephyrproject-rtos/zephyr/issues/90750" target="_blank">#90750</a></td>
      <td><span style="color:#8b949e">Suggested by @hakehuang, not discussed further</span></td>
      <td>Allow migration without disrupting existing CI</td>
      <td>Risk of permanent two-track maintenance burden</td>
    </tr>
  </tbody>
</table>
""")
    parts_html.append("</div>")

    # -- nav
    nav_links = "".join(
        f'<a href="#s{i}">{label}</a>'
        for i, label in [
            (0, "Tree Stats"),
            (1, "Board Ranking"),
            (2, "Sample/Test Ranking"),
            (3, "DTS Patterns"),
            (4, "Kconfig Patterns"),
            (5, "Exact Duplicates"),
            (6, "Near-Duplicates"),
            (7, "Recommendations"),
            (8, "Single-Symbol .conf"),
            (9, "Community Discussion"),
        ]
    )
    nav_html = f'<nav>{nav_links}</nav>'

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Zephyr Board Overlay Analysis</title>
<style>{CSS}</style>
</head>
<body>
<h1>Zephyr Board Overlay Analysis</h1>
<p class="subtitle">
  Crawled <strong>samples/</strong> and <strong>tests/</strong> for
  <code>boards/*.overlay</code> and <code>boards/*.conf</code> files.
  Generated from {e(str(zephyr_root))}.
</p>
{nav_html}
{''.join(parts_html)}
</body>
</html>"""


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> None:
    ap = argparse.ArgumentParser(description="Analyse board overlays in Zephyr tree")
    ap.add_argument(
        "--zephyr-root",
        default=os.environ.get("ZEPHYR_BASE", "."),
        help="Path to Zephyr root (default: $ZEPHYR_BASE or .)",
    )
    ap.add_argument(
        "--output",
        default="board_overlay_analysis.html",
        help="Output HTML file (default: board_overlay_analysis.html)",
    )
    ap.add_argument(
        "--near-dup-threshold",
        type=float,
        default=0.85,
        help="Jaccard similarity threshold for near-duplicate detection (default: 0.85)",
    )
    ap.add_argument(
        "--no-near-dup",
        action="store_true",
        help="Skip near-duplicate detection (faster for large trees)",
    )
    args = ap.parse_args()

    zroot = Path(args.zephyr_root).resolve()
    roots = [zroot / "samples", zroot / "tests"]
    missing = [r for r in roots if not r.is_dir()]
    if missing:
        sys.exit(f"ERROR: directories not found: {missing}")

    print(f"Scanning {zroot} ...")
    overlays = collect_overlays(roots)
    print(f"  Collected {len(overlays)} overlay files across {len(set(o.board for o in overlays))} boards")

    print("Classifying DTS patterns ...")
    dts_patterns = extract_dts_patterns(overlays)

    print("Classifying Kconfig patterns ...")
    conf_patterns = extract_kconfig_patterns(overlays)

    print("Detecting exact duplicates ...")
    duplicates = find_duplicates(overlays)
    print(f"  Found {len(duplicates)} duplicate groups ({sum(len(v) for v in duplicates.values())} files)")

    near_dups: List[Tuple] = []
    if not args.no_near_dup:
        print("Detecting near-duplicates (this may take a moment) ...")
        near_dups = find_near_duplicates(overlays, threshold=args.near_dup_threshold)
        print(f"  Found {len(near_dups)} near-duplicate pairs")

    print("Building recommendations ...")
    recs = applicable_recommendations(dts_patterns, conf_patterns)

    print("Collecting tree-wide file statistics ...")
    tree_stats = collect_tree_stats(roots, zroot)
    print(f"  {tree_stats['whole_tree_files']:,} total repo files, "
          f"{tree_stats['total_files']:,} in samples+tests, "
          f"{tree_stats['testcase_dirs']:,} test/sample dirs, "
          f"{tree_stats['dirs_with_overlay']:,} dirs with overlays")

    print(f"Writing HTML to {args.output} ...")
    html_out = build_html(overlays, dts_patterns, conf_patterns, duplicates, near_dups, recs, zroot, tree_stats)
    Path(args.output).write_text(html_out)
    print(f"Done. Open {args.output} in a browser.")


if __name__ == "__main__":
    main()
