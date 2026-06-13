#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0
"""
find_stale_board_overlays.py - Find board overlay files that reference
non-existent, misnamed, or removed boards.

Board overlays are .conf and .overlay files placed in a boards/ subdirectory
within a test or sample. Their filenames encode the target board in the form:

    {BOARD}[_{QUALIFIER_PARTS}][.conf|.overlay]
    {BOARD}@{REVISION}[_{QUALIFIER_PARTS}][.conf|.overlay]

where BOARD is the board name, QUALIFIER_PARTS are the SoC/cpucluster/variant
path components joined by underscores instead of slashes, and REVISION is an
optional board revision string.

The script builds a set of all known board targets by reading:
  - boards/**/*.yaml           individual target YAML files (identifier: field)
  - boards/**/board.yml        v2 board descriptor (parsed directly for boards
                               that do not generate individual .yaml files)

and converts each target to its filename-stem representation for matching.

Usage:
    python3 scripts/find_stale_board_overlays.py [--zephyr-base DIR]
                                                  [--search-path DIR ...]
                                                  [--summary]
                                                  [--porcelain]
"""

import argparse
import pathlib
import sys

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML is required. Install with: pip install pyyaml", file=sys.stderr)
    sys.exit(1)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _target_to_stem(target):
    """Convert a board target identifier to its overlay filename stem.

    Slashes that separate board/soc/cpucluster/variant components become
    underscores; the @ revision separator is kept verbatim.

    Examples:
        nrf5340dk/nrf5340/cpuapp          -> nrf5340dk_nrf5340_cpuapp
        mimxrt1170_evk@A/mimxrt1176/cm7   -> mimxrt1170_evk@A_mimxrt1176_cm7
        nrf52dk/nrf52832                  -> nrf52dk_nrf52832
        native_sim                        -> native_sim
    """
    return target.replace("/", "_")


def _variant_qualifiers(variant, prefix=""):
    """Recursively yield qualifier strings for a variant node.

    Each variant dict may contain a nested 'variants' list.
    """
    name = variant.get("name", "")
    if not name:
        return
    qual = f"{prefix}/{name}" if prefix else name
    yield qual
    for child in variant.get("variants", []) or []:
        yield from _variant_qualifiers(child, qual)


def _board_qualifiers_from_yml(board_entry):
    """Yield all qualifier strings for a board described by a board.yml entry.

    Handles v2 format with socs/cpuclusters/variants.  Returns only the
    qualifier part (everything after board_name/).
    """
    socs = board_entry.get("socs", []) or []
    board_variants = board_entry.get("variants", []) or []

    if not socs:
        # Board with no SoC qualifiers — target is just the board name.
        yield ""
        for v in board_variants:
            yield from _variant_qualifiers(v)
        return

    for soc in socs:
        if not isinstance(soc, dict):
            continue
        sname = soc.get("name", "")
        if not sname:
            continue
        soc_variants = soc.get("variants", []) or []
        cpuclusters = soc.get("cpuclusters", []) or []

        if cpuclusters:
            for cluster in cpuclusters:
                if not isinstance(cluster, dict):
                    continue
                cname = cluster.get("name", "")
                if not cname:
                    continue
                cpu_prefix = f"{sname}/{cname}"
                yield cpu_prefix
                for v in cluster.get("variants", []) or []:
                    yield from _variant_qualifiers(v, cpu_prefix)
        else:
            yield sname
            for v in soc_variants:
                yield from _variant_qualifiers(v, sname)

    for v in board_variants:
        yield from _variant_qualifiers(v)


# ---------------------------------------------------------------------------
# Board target collection
# ---------------------------------------------------------------------------

def collect_valid_stems(zephyr_base):
    """Return a set of all known overlay filename stems.

    Primary source: individual target .yaml files (boards/**/*.yaml with an
    'identifier:' key).  These are generated for every buildable target by
    the Zephyr build system and are authoritative.

    Fallback: for board directories that contain board.yml but no individual
    .yaml target files, parse board.yml directly to derive targets.
    """
    valid = set()
    boards_root = zephyr_base / "boards"
    if not boards_root.is_dir():
        print(f"WARNING: boards/ directory not found at {boards_root}", file=sys.stderr)
        return valid

    # --- Pass 1: individual target .yaml files (authoritative) ---
    # Track which directories already provided targets this way.
    dirs_with_yaml_targets = set()

    for yaml_path in boards_root.glob("**/*.yaml"):
        if yaml_path.name in ("board.yml",):
            continue
        try:
            data = yaml.safe_load(yaml_path.read_text(encoding="utf-8"))
        except Exception:
            continue
        if not isinstance(data, dict):
            continue
        ident = data.get("identifier", "")
        if ident:
            valid.add(_target_to_stem(ident))
            # Also add the bare board name (part before first '/') so that
            # shared overlay fragments like 'boardname_common.overlay' are
            # not falsely flagged when 'boardname' is a real board.
            board_name = ident.split("/")[0].split("@")[0]
            if board_name:
                valid.add(board_name)
            dirs_with_yaml_targets.add(yaml_path.parent)

    # --- Pass 2: board.yml files for dirs without individual yaml targets ---
    for yml_path in boards_root.glob("**/board.yml"):
        if yml_path.parent in dirs_with_yaml_targets:
            continue
        try:
            data = yaml.safe_load(yml_path.read_text(encoding="utf-8"))
        except Exception as exc:
            print(f"WARNING: could not parse {yml_path}: {exc}", file=sys.stderr)
            continue
        if not isinstance(data, dict):
            continue

        # board.yml may use 'board:' (singular) or 'boards:' (list).
        entries = data.get("boards", None)
        if entries is None:
            entry = data.get("board", None)
            entries = [entry] if isinstance(entry, dict) else []

        for entry in entries:
            if not isinstance(entry, dict):
                continue
            bname = entry.get("name", "")
            if not bname:
                continue

            rev_info = entry.get("revision", {}) or {}
            revisions = [
                r.get("name", "")
                for r in (rev_info.get("revisions", []) or [])
                if isinstance(r, dict) and r.get("name", "")
            ]

            for qualifier in _board_qualifiers_from_yml(entry):
                target = f"{bname}/{qualifier}" if qualifier else bname
                stem = _target_to_stem(target)
                valid.add(stem)
                for rev in revisions:
                    valid.add(_target_to_stem(f"{bname}@{rev}/{qualifier}" if qualifier
                                              else f"{bname}@{rev}"))
            # Also add bare board name for shared fragment matching.
            valid.add(bname)

    return valid


# ---------------------------------------------------------------------------
# Overlay scanning
# ---------------------------------------------------------------------------

def overlay_stem_is_valid(stem, valid_stems):
    """Return True if *stem* encodes a known board target.

    The overlay filename stem may be:
      - An exact match for a known target stem, or
      - A longer stem where a prefix (obtained by dropping trailing
        underscore-separated segments) matches a known target stem.

    The second case covers shared overlay fragments like
    ``cyw920829m2evk_02_common`` (board prefix ``cyw920829m2evk_02``
    is valid) and mismatched qualifiers that still use a valid board name.

    Only the *longest* matching prefix matters; if the board name itself
    appears as a known stem the file is considered valid.
    """
    if stem in valid_stems:
        return True

    # Walk from longest prefix to shortest (excluding the full stem, checked
    # above) to find if any known board name is a prefix of this stem.
    parts = stem.split("_")
    for length in range(len(parts) - 1, 0, -1):
        prefix = "_".join(parts[:length])
        if prefix in valid_stems:
            return True

    return False


def find_stale_overlays(search_paths, valid_stems):
    """Yield (path, stem) for overlay files that reference unknown boards."""
    overlay_extensions = {".conf", ".overlay"}
    for search_root in search_paths:
        for overlay_file in sorted(search_root.rglob("boards/*")):
            if not overlay_file.is_file():
                continue
            if overlay_file.suffix not in overlay_extensions:
                continue
            stem = overlay_file.stem
            if not overlay_stem_is_valid(stem, valid_stems):
                yield overlay_file, stem


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Find board overlay files referencing non-existent boards.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--zephyr-base",
        metavar="DIR",
        default=None,
        help="Zephyr repository root (default: auto-detect from script location).",
    )
    parser.add_argument(
        "--search-path",
        metavar="DIR",
        action="append",
        default=[],
        help="Directory to search for overlays (may be repeated). "
             "Defaults to <zephyr-base>/tests and <zephyr-base>/samples.",
    )
    parser.add_argument(
        "--summary",
        action="store_true",
        help="Print only a summary count line.",
    )
    parser.add_argument(
        "--porcelain",
        action="store_true",
        help="Print only file paths, one per line (for scripting).",
    )
    args = parser.parse_args()

    # Determine Zephyr base directory.
    if args.zephyr_base:
        zephyr_base = pathlib.Path(args.zephyr_base).resolve()
    else:
        zephyr_base = pathlib.Path(__file__).resolve().parent.parent

    if not (zephyr_base / "boards").is_dir():
        print(
            f"ERROR: {zephyr_base} does not look like a Zephyr root "
            "(no boards/ directory found).",
            file=sys.stderr,
        )
        sys.exit(1)

    # Determine search paths.
    if args.search_path:
        search_paths = [pathlib.Path(p).resolve() for p in args.search_path]
    else:
        search_paths = [
            p for p in (zephyr_base / "tests", zephyr_base / "samples")
            if p.is_dir()
        ]

    if not args.porcelain and not args.summary:
        print(f"Zephyr base:   {zephyr_base}")
        print(f"Search paths:  {[str(p) for p in search_paths]}")
        print("Collecting valid board targets...", flush=True)

    valid_stems = collect_valid_stems(zephyr_base)

    if not args.porcelain and not args.summary:
        print(f"Found {len(valid_stems)} valid board target stems.\n")
        print("Scanning for stale overlay files...\n")

    stale = list(find_stale_overlays(search_paths, valid_stems))

    if args.porcelain:
        for path, _ in stale:
            print(path)
    elif args.summary:
        print(f"Stale overlay files: {len(stale)}")
    else:
        if stale:
            for path, stem in stale:
                rel = path.relative_to(zephyr_base) if path.is_relative_to(zephyr_base) else path
                print(f"  {rel}  [{stem!r}]")
            print(f"\nTotal stale overlay files: {len(stale)}")
        else:
            print("No stale overlay files found.")

    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())
