#!/usr/bin/env python3
# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0
"""Merge multiple twister.json report files into a single JSON file.

Each twister.json contains an 'environment' dict and a 'testsuites' list.
The merged output keeps the 'environment' from the first input file (placed at
the top) and concatenates all 'testsuites' lists.
"""

from __future__ import annotations

import argparse
import glob
import json
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description='Merge multiple twister.json files into one',
        allow_abbrev=False,
    )
    parser.add_argument(
        'inputs',
        nargs='+',
        help='Input twister.json files or glob patterns',
    )
    parser.add_argument(
        '-o', '--output',
        required=True,
        help='Output JSON file path',
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    # Expand any glob patterns in the input list
    input_files: list[str] = []
    for pattern in args.inputs:
        matched = sorted(glob.glob(pattern, recursive=True))
        if matched:
            input_files.extend(matched)
        else:
            # Treat as a literal path; let open() raise if missing
            input_files.append(pattern)

    if not input_files:
        print(f"No input files found for patterns: {args.inputs}", file=sys.stderr)
        return 1

    environment: dict | None = None
    testsuites: list = []

    for path in input_files:
        try:
            with open(path) as fh:
                data = json.load(fh)
        except (OSError, json.JSONDecodeError) as exc:
            print(f"Error reading {path}: {exc}", file=sys.stderr)
            return 1

        # Use the environment block from the first file only
        if environment is None:
            environment = data.get('environment', {})

        testsuites.extend(data.get('testsuites', []))

    merged: dict = {}
    if environment is not None:
        merged['environment'] = environment
    merged['testsuites'] = testsuites

    with open(args.output, 'w') as out:
        json.dump(merged, out, indent=4)

    print(f"Merged {len(input_files)} file(s) -> {args.output} "
          f"({len(testsuites)} testsuites)")
    return 0


if __name__ == '__main__':
    sys.exit(main())
