#!/usr/bin/env python3

# Copyright (c) 2019 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

import argparse
import sys
from Maintainers import Maintainers, MaintainersError, GitError


def _serr(msg):
    # For reporting errors when get_maintainer.py is run as a script.
    # sys.exit() shouldn't be used otherwise.
    sys.exit("{}: error: {}".format(sys.argv[0], msg))


def _main():
    # Entry point when run as an executable

    args = _parse_args()
    try:
        args.cmd_fn(Maintainers(args.maintainers), args)
    except (MaintainersError, GitError) as e:
        _serr(e)


def _parse_args():
    # Parses arguments when run as an executable

    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=__doc__)

    parser.add_argument(
        "-m", "--maintainers",
        metavar="MAINTAINERS_FILE",
        help="Maintainers file to load. If not specified, MAINTAINERS.yml in "
             "the top-level repository directory is used, and must exist. "
             "Paths in the maintainers file will always be taken as relative "
             "to the top-level directory.")

    subparsers = parser.add_subparsers(
        help="Available commands (each has a separate --help text)")

    id_parser = subparsers.add_parser(
        "path",
        help="List area(s) for paths")
    id_parser.add_argument(
        "paths",
        metavar="PATH",
        nargs="*",
        help="Path to list areas for")
    id_parser.set_defaults(cmd_fn=Maintainers._path_cmd)

    commits_parser = subparsers.add_parser(
        "commits",
        help="List area(s) for commit range")
    commits_parser.add_argument(
        "commits",
        metavar="COMMIT_RANGE",
        nargs="*",
        help="Commit range to list areas for (default: HEAD~..)")
    commits_parser.set_defaults(cmd_fn=Maintainers._commits_cmd)

    list_parser = subparsers.add_parser(
        "list",
        help="List files in areas")
    list_parser.add_argument(
        "area",
        metavar="AREA",
        nargs="?",
        help="Name of area to list files in. If not specified, all "
             "non-orphaned files are listed (all files that do not appear in "
             "any area).")
    list_parser.set_defaults(cmd_fn=Maintainers._list_cmd)

    areas_parser = subparsers.add_parser(
        "areas",
        help="List areas and maintainers")
    areas_parser.add_argument(
        "maintainer",
        metavar="MAINTAINER",
        nargs="?",
        help="List all areas maintained by maintaier.")

    areas_parser.set_defaults(cmd_fn=Maintainers._areas_cmd)

    orphaned_parser = subparsers.add_parser(
        "orphaned",
        help="List orphaned files (files that do not appear in any area)")
    orphaned_parser.add_argument(
        "path",
        metavar="PATH",
        nargs="?",
        help="Limit to files under PATH")
    orphaned_parser.set_defaults(cmd_fn=Maintainers._orphaned_cmd)

    args = parser.parse_args()
    if not hasattr(args, "cmd_fn"):
        # Called without a subcommand
        sys.exit(parser.format_usage().rstrip())

    return args


if __name__ == "__main__":
    _main()
