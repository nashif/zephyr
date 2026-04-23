#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

"""Move DTS overlays from a test/sample boards/ directory to the testing snippet.

For each .overlay file found in the test/sample's boards/ directory, this
script:

  1. Copies the overlay to snippets/testing/boards/.
  2. For files that correspond to a known Zephyr board identifier, appends an
     entry to snippets/testing/snippet.yml that sets EXTRA_DTC_OVERLAY_FILE.
  3. Removes the original overlay from the test/sample (unless --no-remove).

Helper overlays (e.g. *_common.overlay that boards #include) are copied to the
snippet's boards/ directory so relative #include paths keep working, but no
snippet.yml entry is generated for them because they have no matching board
identifier.

Usage
-----
    # Preview what would happen (no changes made):
    python3 scripts/move_overlays_to_snippet.py tests/drivers/dma/loop_transfer --dry-run

    # Move overlays and update snippet.yml:
    python3 scripts/move_overlays_to_snippet.py tests/drivers/dma/loop_transfer

    # Copy only (keep originals in the test directory):
    python3 scripts/move_overlays_to_snippet.py tests/drivers/dma/loop_transfer --no-remove

    # Use a custom snippet directory:
    python3 scripts/move_overlays_to_snippet.py tests/drivers/dma/loop_transfer \\
        --snippet-dir snippets/my_snippet
"""

import argparse
import re
import shutil
import sys
from pathlib import Path

import yaml


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def find_zephyr_root(start: Path) -> Path:
    """Walk up from *start* until zephyr-env.sh is found, or raise."""
    path = start.resolve()
    while path != path.parent:
        if (path / "zephyr-env.sh").exists():
            return path
        path = path.parent
    raise FileNotFoundError("Could not locate Zephyr root (zephyr-env.sh not found)")


def _build_soc_map(zephyr_root: Path) -> dict:
    """Return a mapping from board name to its single SoC name, from board.yml files.

    Only boards with exactly one SoC entry are included.  Boards with
    multiple SoCs are excluded because there is no single unambiguous SoC to
    append.  SoC entries that have **cpucluster** variants are also excluded:
    those boards produce per-cluster ``*.yaml`` files whose ``identifier``
    already carries the full ``board/soc/cluster`` qualifier, so the base
    identifier (with only the board name) is ambiguous.

    Non-cpucluster variants (e.g. ``mcuboot``, ``64``) are fine: they have
    their own ``*.yaml`` file with a qualified identifier, while the base
    ``*.yaml`` still carries a bare board-only identifier that needs the SoC
    appended.
    """
    soc_map: dict = {}
    for board_yml in (zephyr_root / "boards").rglob("board.yml"):
        try:
            with open(board_yml) as fh:
                data = yaml.safe_load(fh)
            if not isinstance(data, dict):
                continue
            # Support both `board: {...}` and `boards: [...]` formats
            boards_raw = data.get("board") or data.get("boards")
            if not boards_raw:
                continue
            entries = boards_raw if isinstance(boards_raw, list) else [boards_raw]
            for board in entries:
                if not isinstance(board, dict):
                    continue
                name = board.get("name")
                socs = board.get("socs", [])
                if not name or len(socs) != 1:
                    continue
                soc_entry = socs[0]
                soc_name = soc_entry.get("name") if isinstance(soc_entry, dict) else None
                # Skip only when there are cpucluster variants; those boards
                # produce per-cluster *.yaml files with fully-qualified
                # identifiers so the bare board-name identifier is ambiguous.
                has_cpucluster = any(
                    isinstance(v, dict) and v.get("cpucluster")
                    for v in soc_entry.get("variants", [])
                )
                if soc_name and not has_cpucluster:
                    soc_map[name] = soc_name
        except Exception:  # noqa: BLE001 – best-effort scan
            pass
    return soc_map


def _build_default_revision_map(zephyr_root: Path) -> dict:
    """Return a mapping from board name to its default revision string.

    Only boards that declare a ``revision.default`` in ``board.yml`` are
    included.  Both the singular ``board:`` and the plural ``boards:``
    list formats are supported.
    """
    default_rev_map: dict = {}
    for board_yml in (zephyr_root / "boards").rglob("board.yml"):
        try:
            with open(board_yml) as fh:
                data = yaml.safe_load(fh)
            if not isinstance(data, dict):
                continue
            # Support both `board: {...}` and `boards: [...]` formats
            boards_raw = data.get("board") or data.get("boards")
            if not boards_raw:
                continue
            entries = boards_raw if isinstance(boards_raw, list) else [boards_raw]
            for board in entries:
                if not isinstance(board, dict):
                    continue
                name = board.get("name")
                default_rev = board.get("revision", {}).get("default")
                if name and default_rev:
                    default_rev_map[name] = str(default_rev)
        except Exception:  # noqa: BLE001 – best-effort scan
            pass
    return default_rev_map


def build_board_id_map(zephyr_root: Path) -> dict:
    """Return a mapping from overlay filename-stem to full qualified board target.

    The full target name matches the ``board/soc[/cpucluster][@rev]`` format
    used in snippet.yml board entries.  It is derived from two sources:

    * ``boards/**/*.yaml`` files – each has an ``identifier`` field.  For
      boards with multiple SoCs or CPU clusters this already contains the full
      qualifier (e.g. ``cyw920829m2evk_02/cyw20829b1340``).

    * ``boards/**/board.yml`` files – for single-SoC boards whose ``*.yaml``
      identifier is just the board name (e.g. ``frdm_k64f``), the SoC name
      from ``board.yml`` is appended to form the full target
      (e.g. ``frdm_k64f/mk64f12``).

    Four stem variants are registered per board so that the range of overlay
    naming conventions found in tests is covered:

    1. **YAML file stem** – the ``*.yaml`` filename without extension.
       e.g. ``esp32c6_devkitc_hpcore.yaml`` → key ``'esp32c6_devkitc_hpcore'``

       Special case: when the YAML file for a revisioned board has no revision
       suffix in its name (e.g. ``mimxrt1170_evk_mimxrt1176_cm7.yaml`` for
       identifier ``mimxrt1170_evk@A/mimxrt1176/cm7``), the stem is instead
       mapped to the **default** revision from ``board.yml`` (e.g. ``@B``).
       This matches the convention that an overlay without a revision suffix
       targets the default/latest revision, not the first one.

    2. **Full identifier stem** – all ``/`` and ``@`` replaced with ``_``.
       e.g. identifier ``kit_pse84_ai/pse846gps2dbzc4a/m33``
            → key ``'kit_pse84_ai_pse846gps2dbzc4a_m33'``

    3. **Revision-at-end stem** (only for ``@``-revision identifiers).
       e.g. identifier ``mimxrt1170_evk@A/mimxrt1176/cm7``
            → key ``'mimxrt1170_evk_mimxrt1176_cm7_A'``

    4. **No-revision stem** mapped to the default revision (only for
       ``@``-revision identifiers whose board has a default revision).
       e.g. board default B, identifier ``mimxrt1170_evk@A/mimxrt1176/cm7``
            → key ``'mimxrt1170_evk_mimxrt1176_cm7'`` → ``mimxrt1170_evk@B/…``
    """
    soc_map = _build_soc_map(zephyr_root)
    default_rev_map = _build_default_revision_map(zephyr_root)

    board_map: dict = {}
    for yaml_file in (zephyr_root / "boards").rglob("*.yaml"):
        try:
            with open(yaml_file) as fh:
                data = yaml.safe_load(fh)
            if not isinstance(data, dict) or "identifier" not in data:
                continue
            identifier: str = data["identifier"]
            yaml_stem = yaml_file.stem

            # Qualify bare board-name identifiers with the SoC name when
            # board.yml provides an unambiguous single-SoC mapping.
            if "/" not in identifier and "@" not in identifier:
                soc = soc_map.get(identifier)
                if soc:
                    identifier = f"{identifier}/{soc}"

            if "@" in identifier:
                at_idx = identifier.index("@")
                board_base = identifier[:at_idx]
                rest = identifier[at_idx + 1:]  # "rev[/qual...]"
                rest_parts = rest.split("/")
                rev = rest_parts[0]
                qualifiers = rest_parts[1:]

                # The no-revision stem: board_base + qualifiers joined by "_"
                no_rev_stem = "_".join([board_base] + qualifiers)

                # (1) YAML file stem
                # When the YAML file name has no explicit revision suffix
                # (yaml_stem == no_rev_stem), map it to the default revision
                # rather than to whichever revision this YAML file happens to
                # represent.  An overlay without a revision suffix is intended
                # for the default/latest revision.
                if yaml_stem == no_rev_stem:
                    default_rev = default_rev_map.get(board_base)
                    if default_rev:
                        default_id = (
                            f"{board_base}@{default_rev}"
                            + ("/" + "/".join(qualifiers) if qualifiers else "")
                        )
                        board_map[yaml_stem] = default_id
                    else:
                        board_map[yaml_stem] = identifier
                else:
                    board_map[yaml_stem] = identifier

                # (2) Full identifier stem (replace / and @ with _)
                full_stem = identifier.replace("/", "_").replace("@", "_")
                board_map.setdefault(full_stem, identifier)

                # (3) Revision-at-end stem: board_base + qualifiers + rev
                rev_at_end = "_".join([board_base] + qualifiers + [rev])
                board_map.setdefault(rev_at_end, identifier)

                # (4) No-revision stem → default revision (if not already set
                # by the YAML file stem logic above)
                default_rev = default_rev_map.get(board_base)
                if default_rev:
                    default_id = (
                        f"{board_base}@{default_rev}"
                        + ("/" + "/".join(qualifiers) if qualifiers else "")
                    )
                    board_map.setdefault(no_rev_stem, default_id)
            else:
                # (1) YAML file stem
                board_map[yaml_stem] = identifier

                # (2) Full identifier stem
                full_stem = identifier.replace("/", "_").replace("@", "_")
                board_map.setdefault(full_stem, identifier)

        except Exception:  # noqa: BLE001 – best-effort scan
            pass
    return board_map


def _strip_comments_and_blanks(text: str) -> str:
    """Return *text* with C-style block comments and blank lines removed.

    Used to compare the semantic DTS content of two overlay files while
    ignoring copyright headers and whitespace differences.
    """
    # Remove /* ... */ block comments (including multi-line)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    # Collapse multiple blank lines / strip leading-trailing whitespace
    lines = [line.strip() for line in text.splitlines()]
    return "\n".join(line for line in lines if line)


def _dts_body(text: str) -> str:
    """Return only the DTS statements from *text*, stripping boilerplate.

    Boilerplate is the leading block of ``/* ... */`` comments (copyright,
    SPDX headers, etc.) and any blank lines that follow them.  Everything
    after that block is considered real DTS content.
    """
    # Drop all /* ... */ comments then collect non-blank lines
    stripped = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    lines = [line.rstrip() for line in stripped.splitlines()]
    # Skip leading blank lines that remain after comment removal
    start = 0
    while start < len(lines) and not lines[start].strip():
        start += 1
    return "\n".join(lines[start:]).rstrip()


def overlay_body(path: Path) -> str:
    """Return the normalised DTS body of *path* (no comments, no blanks)."""
    return _strip_comments_and_blanks(path.read_text())


def _extract_dts_blocks(body: str) -> list:
    """Split a DTS body into a list of ``(text, normalized)`` top-level blocks.

    A "block" is one complete top-level DTS construct:
      - A brace-delimited node reference:  ``[label:] &ref { ... };``
      - A single-line statement such as ``#include <...>`` or a bare alias.

    *text* preserves the original indentation; *normalized* is a
    whitespace-collapsed version used for equality checks.
    """
    blocks: list = []
    current: list = []
    depth = 0

    for line in body.splitlines():
        rstripped = line.rstrip()
        if not rstripped.strip():
            # Blank line at top level: flush any pending single-line block
            if depth == 0 and current:
                block = "\n".join(current)
                blocks.append((block, " ".join(block.split())))
                current = []
            continue

        current.append(rstripped)
        depth += rstripped.count("{") - rstripped.count("}")
        if depth <= 0:
            depth = 0
            if current:
                block = "\n".join(current)
                blocks.append((block, " ".join(block.split())))
                current = []

    if current:
        block = "\n".join(current)
        blocks.append((block, " ".join(block.split())))

    return blocks


def merge_overlay(dest: Path, src: Path, dry_run: bool) -> str:
    """Merge *src* into existing *dest*, appending only content not already present.

    Comparison and append work at the **top-level DTS block** level, not
    individual lines.  This ensures that a node reference such as

        &dma0 {
            status = "okay";
            interrupts = <95 1>, <94 1>;
        };

    is appended as a whole when the dest already contains a different variant
    of ``&dma0 { ... }`` — rather than erroneously appending only the inner
    lines that differ.

    Copyright headers, SPDX lines, and other ``/* */`` boilerplate are ignored
    in both comparison and append.

    Returns a short status string:
      ``'identical'``  – bodies are the same; nothing to do
      ``'subset'``     – every block in src already exists in dest; nothing to do
      ``'append'``     – (dry-run) new blocks would be appended
      ``'appended'``   – new blocks from src were appended to dest
    """
    dest_body = overlay_body(dest)
    src_body = overlay_body(src)

    if src_body == dest_body:
        return "identical"

    if src_body in dest_body:
        return "subset"

    # Compare at the block level using the structure-preserving DTS body.
    dest_blocks_norm = {nb for _, nb in _extract_dts_blocks(_dts_body(dest.read_text()))}
    src_blocks = _extract_dts_blocks(_dts_body(src.read_text()))

    new_blocks = [(bt, bn) for bt, bn in src_blocks if bn not in dest_blocks_norm]

    if not new_blocks:
        return "subset"

    if dry_run:
        return "append"

    existing = dest.read_text()
    with open(dest, "a") as fh:
        if not existing.endswith("\n"):
            fh.write("\n")
        for block_text, _ in new_blocks:
            fh.write("\n" + block_text + "\n")
    return "appended"


def _parse_revision_identifier(identifier: str):
    """Return (base_target, revision) if identifier contains @rev, else None.

    Example::

        'mimxrt1170_evk@A/mimxrt1176/cm7'  ->  ('mimxrt1170_evk/mimxrt1176/cm7', 'A')
        'nrf9160dk@0.7.0/nrf9160'          ->  ('nrf9160dk/nrf9160', '0.7.0')
        'frdm_k64f/mk64f12'                ->  None
    """
    if "@" not in identifier:
        return None
    at_idx = identifier.index("@")
    board_part = identifier[:at_idx]
    rest = identifier[at_idx + 1:]
    parts = rest.split("/")
    rev = parts[0]
    qualifiers = parts[1:]
    base_target = "/".join([board_part] + qualifiers)
    return base_target, rev


def _load_snippet_data(snippet_yml: Path) -> dict:
    """Load and return the parsed content of snippet.yml."""
    try:
        with open(snippet_yml) as fh:
            data = yaml.safe_load(fh)
        return data if isinstance(data, dict) else {}
    except Exception:  # noqa: BLE001
        return {}


def _normalize_snippet_boards(boards: dict) -> dict:
    """Convert any flat @rev board entries to ``revisions:`` sub-entries.

    Old-style flat entries such as::

        'mimxrt1170_evk@A/mimxrt1176/cm7': {append: {EXTRA_DTC_OVERLAY_FILE: ...}}

    are converted to the schema-compliant form::

        'mimxrt1170_evk/mimxrt1176/cm7': {
            revisions: {'A': {append: {EXTRA_DTC_OVERLAY_FILE: ...}}}
        }

    Entries without ``@rev`` are kept unchanged.  When multiple revision
    entries share the same base target they are merged into one block.
    """
    normalized: dict = {}
    for board_id, board_data in boards.items():
        parsed = _parse_revision_identifier(board_id)
        if parsed is None:
            # Non-revision entry: keep as-is
            if board_id not in normalized:
                normalized[board_id] = dict(board_data) if isinstance(board_data, dict) else {}
        else:
            # @rev entry: migrate under base_target -> revisions -> rev
            base_target, rev = parsed
            if base_target not in normalized:
                normalized[base_target] = {}
            revisions = normalized[base_target].setdefault("revisions", {})
            if rev not in revisions and isinstance(board_data, dict) and "append" in board_data:
                revisions[rev] = {"append": dict(board_data["append"])}
    return normalized


def _append_yaml_value(lines: list, key: str, value, indent: int) -> None:
    """Append a ``key: value`` YAML pair to *lines* at *indent* spaces."""
    prefix = " " * indent
    if isinstance(value, list):
        if len(value) == 1:
            lines.append(f"{prefix}{key}: {value[0]}")
        else:
            lines.append(f"{prefix}{key}:")
            for item in value:
                lines.append(f"{prefix}  - {item}")
    else:
        lines.append(f"{prefix}{key}: {value}")


def _serialize_snippet_yml(data: dict) -> str:
    """Serialize snippet data to YAML text with consistent formatting.

    Boards are emitted in sorted order.  Revision sub-entries are also
    sorted.  All revision keys are double-quoted to match the schema examples
    (e.g. ``"A":``, ``"0.7.0":``).  The function does not use
    ``yaml.dump`` so as to preserve the exact indentation style expected by
    the Zephyr project.
    """
    lines: list = []
    name = data.get("name", "")
    lines.append(f"name: {name}")

    global_append = data.get("append")
    if global_append:
        lines.append("append:")
        for key, value in global_append.items():
            _append_yaml_value(lines, key, value, indent=2)

    boards = data.get("boards")
    if boards:
        lines.append("")
        lines.append("boards:")
        for board_id in sorted(boards.keys()):
            board_data = boards[board_id]
            lines.append(f"  {board_id}:")
            if not isinstance(board_data, dict):
                continue
            board_append = board_data.get("append")
            if board_append:
                lines.append("    append:")
                for key, value in board_append.items():
                    _append_yaml_value(lines, key, value, indent=6)
            revisions = board_data.get("revisions")
            if revisions:
                lines.append("    revisions:")
                for rev in sorted(revisions.keys()):
                    rev_data = revisions[rev]
                    lines.append(f'      "{rev}":')
                    if isinstance(rev_data, dict):
                        rev_append = rev_data.get("append")
                        if rev_append:
                            lines.append("        append:")
                            for key, value in rev_append.items():
                                _append_yaml_value(lines, key, value, indent=10)

    return "\n".join(lines) + "\n"


def load_existing_boards(snippet_yml: Path) -> set:
    """Return the set of board identifiers already present in snippet.yml.

    Handles both the legacy flat ``@rev`` format and the current
    ``revisions:`` sub-entry format:

    * Flat ``@rev`` keys (old format) are added as-is; their base target
      (without ``@rev``) is also added so a subsequent run does not create
      a duplicate base entry.
    * ``revisions:`` sub-entries are reconstructed into ``@rev`` qualified
      identifiers so that the duplicate-check in ``process_overlays`` works
      correctly regardless of which format the file currently uses.
    """
    data = _load_snippet_data(snippet_yml)
    boards = data.get("boards", {}) if isinstance(data, dict) else {}
    existing: set = set()
    for board_id, board_data in boards.items():
        existing.add(board_id)
        parsed = _parse_revision_identifier(board_id)
        if parsed is not None:
            # Old-style @rev flat key: expose the base target too
            existing.add(parsed[0])
        if isinstance(board_data, dict):
            for rev in board_data.get("revisions", {}):
                # New-style revisions sub-entry: reconstruct @rev identifier
                if "/" in board_id:
                    first_slash = board_id.index("/")
                    at_id = board_id[:first_slash] + f"@{rev}" + board_id[first_slash:]
                else:
                    at_id = f"{board_id}@{rev}"
                existing.add(at_id)
    return existing


def update_snippet_yml(
    snippet_yml: Path,
    new_entries: dict,
    dry_run: bool,
    soc_map: dict | None = None,
) -> None:
    """Update *snippet_yml* with *new_entries* using the revisions-aware format.

    This function performs three actions in a single load/serialize/write cycle:

    1. **Canonicalization** – any existing board keys that are bare board names
       (no SoC qualifier) are renamed to their full target form using
       *soc_map* (e.g. ``native_sim`` → ``native_sim/native``).
    2. **Migration** – any existing flat ``board@rev`` entries are converted
       to the ``revisions:`` sub-entry format required by the snippet schema.
    3. **Addition** – entries in *new_entries* are inserted.  When a board
       identifier contains ``@rev``, the entry is nested under a
       ``revisions:`` block on the base target rather than being written as
       a flat ``board@rev`` key.

    Parameters
    ----------
    snippet_yml:  path to snippet.yml
    new_entries:  dict mapping board_identifier -> overlay_path (relative to
                  snippet directory, e.g. ``boards/frdm_k64f.overlay``)
    dry_run:      when True, only print what would be written
    soc_map:      optional mapping from board name to single SoC name,
                  used to expand bare board-name keys to full targets
    """
    data = _load_snippet_data(snippet_yml)
    if not isinstance(data, dict):
        data = {"name": "testing"}

    raw_boards = data.get("boards") or {}

    # Step 0: canonicalize bare board-name keys to full target names
    to_canonicalize = 0
    if soc_map:
        canonicalized: dict = {}
        for board_id, board_data in raw_boards.items():
            if "/" not in board_id and "@" not in board_id:
                soc = soc_map.get(board_id)
                if soc:
                    full_id = f"{board_id}/{soc}"
                    canonicalized[full_id] = board_data
                    to_canonicalize += 1
                    continue
            canonicalized[board_id] = board_data
        raw_boards = canonicalized

    # Step 1: migrate any existing @rev flat entries
    boards = _normalize_snippet_boards(raw_boards)
    to_migrate = sum(1 for k in raw_boards if "@" in k)

    # Step 2: add new entries
    for board_id in sorted(new_entries.keys()):
        overlay_path = new_entries[board_id]
        parsed = _parse_revision_identifier(board_id)
        if parsed is None:
            # Plain (non-revision) entry
            if board_id not in boards:
                boards[board_id] = {"append": {"EXTRA_DTC_OVERLAY_FILE": overlay_path}}
        else:
            # Revision-based entry: nest under base target
            base_target, rev = parsed
            if base_target not in boards:
                boards[base_target] = {}
            revisions = boards[base_target].setdefault("revisions", {})
            if rev not in revisions:
                revisions[rev] = {"append": {"EXTRA_DTC_OVERLAY_FILE": overlay_path}}

    data["boards"] = boards
    new_text = _serialize_snippet_yml(data)
    old_text = snippet_yml.read_text()

    if new_text == old_text:
        return

    if dry_run:
        if new_entries:
            print(f"\n[DRY RUN] Would add {len(new_entries)} board entry/entries to {snippet_yml}")
        if to_migrate:
            print(f"[DRY RUN] Would migrate {to_migrate} @rev board entries to revisions format")
        if to_canonicalize:
            print(f"[DRY RUN] Would expand {to_canonicalize} bare board name(s) to full target names")
    else:
        if new_entries:
            print(f"\nAdding {len(new_entries)} board entry/entries to {snippet_yml}")
        if to_migrate:
            print(f"Migrating {to_migrate} @rev board entries to revisions format in {snippet_yml}")
        if to_canonicalize:
            print(f"Expanding {to_canonicalize} bare board name(s) to full target names in {snippet_yml}")
        snippet_yml.write_text(new_text)


# ---------------------------------------------------------------------------
# Core logic
# ---------------------------------------------------------------------------


def process_overlays(
    test_dir: Path,
    snippet_dir: Path,
    board_id_map: dict,
    dry_run: bool,
    no_remove: bool,
    soc_map: dict | None = None,
) -> int:
    """Move overlays from *test_dir*/boards/ to *snippet_dir*/boards/.

    Returns 0 on success, non-zero on error.
    """
    test_boards_dir = test_dir / "boards"
    if not test_boards_dir.is_dir():
        print(f"No boards/ directory found in {test_dir}", file=sys.stderr)
        return 1

    overlay_files = sorted(test_boards_dir.glob("*.overlay"))
    if not overlay_files:
        print(f"No .overlay files found in {test_boards_dir}")
        return 0

    snippet_boards_dir = snippet_dir / "boards"
    snippet_yml = snippet_dir / "snippet.yml"

    existing_boards = load_existing_boards(snippet_yml)

    new_snippet_entries: dict = {}
    processed_files: list = []    # files handled (non-dry-run)
    would_process: list = []      # files that would be handled (dry-run tracking)

    print(f"Processing {len(overlay_files)} overlay file(s)\n")

    for overlay_file in overlay_files:
        stem = overlay_file.stem
        board_id = board_id_map.get(stem)
        dest_file = snippet_boards_dir / overlay_file.name

        is_board_overlay = board_id is not None
        tag = f"board: {board_id}" if is_board_overlay else "helper"
        print(f"  {overlay_file.name}  [{tag}]")

        # --- copy / merge step ---
        if dest_file.exists():
            status = merge_overlay(dest_file, overlay_file, dry_run)
            if status == "identical":
                print(f"    Skip: destination exists with identical content")
                would_process.append(overlay_file)
                processed_files.append(overlay_file)
            elif status == "subset":
                print(f"    Skip: content already present in destination")
                would_process.append(overlay_file)
                processed_files.append(overlay_file)
            elif status == "append":
                print(f"    [DRY RUN] Would append new content -> {dest_file}")
                would_process.append(overlay_file)
            else:  # "appended"
                print(f"    Appended new content -> {dest_file}")
                processed_files.append(overlay_file)
        else:
            action = "[DRY RUN] Copy" if dry_run else "Copy"
            print(f"    {action} -> {dest_file}")
            if dry_run:
                would_process.append(overlay_file)
            else:
                snippet_boards_dir.mkdir(parents=True, exist_ok=True)
                shutil.copy2(overlay_file, dest_file)
                processed_files.append(overlay_file)

        # --- snippet.yml step ---
        if not is_board_overlay:
            continue
        if board_id in existing_boards:
            print(f"    Warning: {board_id} already in snippet.yml, skipping entry")
            continue
        new_snippet_entries[board_id] = f"boards/{overlay_file.name}"

    # Update snippet.yml (also migrates any existing @rev flat entries and
    # expands bare board names to full target names)
    update_snippet_yml(snippet_yml, new_snippet_entries, dry_run, soc_map)

    # Remove originals
    pending = would_process if dry_run else processed_files
    if pending and not no_remove:
        if dry_run:
            print(f"\n[DRY RUN] Would remove {len(pending)} original overlay file(s)")
        else:
            print(f"\nRemoving {len(pending)} original overlay file(s)")
            for f in pending:
                print(f"  Remove {f}")
                f.unlink()
    elif no_remove and pending:
        print("\nOriginals kept (--no-remove)")

    # Report boards with .conf but no .overlay
    conf_only = []
    for conf_file in sorted(test_boards_dir.glob("*.conf")):
        stem = conf_file.stem
        if board_id_map.get(stem) and not (test_boards_dir / f"{stem}.overlay").exists():
            conf_only.append(stem)
    if conf_only:
        print("\nBoards with .conf only (no overlay to migrate):")
        for stem in conf_only:
            print(f"  {stem}  [board: {board_id_map[stem]}]")

    return 0


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Move DTS overlays from a test/sample boards/ dir to the testing snippet",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "test_dir",
        help="Path to the test or sample directory (absolute or relative)",
    )
    parser.add_argument(
        "--snippet-dir",
        default=None,
        metavar="DIR",
        help="Snippet directory (default: <zephyr-root>/snippets/testing)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be done without making any changes",
    )
    parser.add_argument(
        "--no-remove",
        action="store_true",
        help="Copy overlays to the snippet but keep the originals in the test",
    )
    args = parser.parse_args()

    test_dir = Path(args.test_dir).resolve()
    if not test_dir.is_dir():
        print(f"Error: {test_dir} is not a directory", file=sys.stderr)
        return 1

    try:
        zephyr_root = find_zephyr_root(test_dir)
    except FileNotFoundError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    snippet_dir = (
        Path(args.snippet_dir).resolve()
        if args.snippet_dir
        else zephyr_root / "snippets" / "testing"
    )
    if not snippet_dir.is_dir():
        print(f"Error: snippet directory not found: {snippet_dir}", file=sys.stderr)
        return 1

    print(f"Zephyr root : {zephyr_root}")
    print(f"Test dir    : {test_dir}")
    print(f"Snippet dir : {snippet_dir}")
    if args.dry_run:
        print("Mode        : DRY RUN (no changes)\n")
    print()

    print("Building board identifier map ...")
    soc_map = _build_soc_map(zephyr_root)
    board_id_map = build_board_id_map(zephyr_root)
    print(f"Found {len(board_id_map)} board identifiers\n")

    return process_overlays(test_dir, snippet_dir, board_id_map, args.dry_run, args.no_remove, soc_map)


if __name__ == "__main__":
    sys.exit(main())
