#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

"""
Devicetree facts for a board target, without configuring a build.

The pipeline is deliberately short:

  resolve board target -> resolve revision/qualifiers -> preprocess DTS
  -> edtlib -> extract facts

No toolchain is probed, no Kconfig is run and nothing is compiled. The
board target, revision and qualifier resolution mirrors what
cmake/modules/boards.cmake does, and the devicetree input file discovery
mirrors cmake/modules/pre_dt.cmake and cmake/modules/dts.cmake, so the
resulting devicetree is the board's base devicetree: the one a build
without application overlays, shields or snippets would use.

This module is shared between the 'west boards --generate-facts' command
and standalone use (python3 scripts/board_facts.py --target <target>).
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

import list_boards
import list_hardware
from list_hardware import unique_paths

ZEPHYR_BASE = Path(__file__).resolve().parents[1]
DT_SCRIPTS = ZEPHYR_BASE / 'scripts' / 'dts'

sys.path.insert(0, str(DT_SCRIPTS))
sys.path.insert(0, str(DT_SCRIPTS / 'python-devicetree' / 'src'))

import edtlib_logger  # noqa: E402
from devicetree import edtlib  # noqa: E402

BOARD_TARGET_RE = re.compile(r'^([^@/]+)(@[^@/]+)?(/([^@]+))?$')
DEPRECATED_RE = re.compile(r'set\(\s*(\S+?)_DEPRECATED\s+(\S+)\s*\)', re.MULTILINE)
ALIAS_RE = re.compile(r'set\(\s*(\S+?)_BOARD_ALIAS\s+(\S+)\s*\)', re.MULTILINE)
EXTRA_DTC_FLAGS_RE = re.compile(r'list\(\s*APPEND\s+EXTRA_DTC_FLAGS\s+([^)]*)\)', re.MULTILINE)

STUB_DTS = ZEPHYR_BASE / 'boards' / 'common' / 'stub.dts'
EMPTY_FILE = ZEPHYR_BASE / 'misc' / 'empty_file.c'

# Preprocessors tried, in order, when none is given explicitly.
DEFAULT_PREPROCESSORS = ('gcc', 'clang', 'cpp')

# Subdirectories of each DTS root that become preprocessor include
# directories, in the order pre_dt.cmake adds them. 'dts/<arch>' entries
# are inserted before the final 'dts' entry at run time.
DTS_ROOT_INCLUDE_SUBDIRS = ('include', 'include/zephyr', 'dts/common', 'dts/vendor')


class BoardFactsError(RuntimeError):
    pass


@dataclass
class BoardTarget:
    """A fully resolved board target."""

    board: list_boards.Board
    requested: str
    revision: str | None
    active_revision: str | None
    qualifiers: str
    single_soc: bool

    @property
    def name(self) -> str:
        return self.board.name

    @property
    def target(self) -> str:
        revision = f'@{self.active_revision}' if self.active_revision else ''
        qualifiers = f'/{self.qualifiers}' if self.qualifiers else ''
        return f'{self.name}{revision}{qualifiers}'

    @property
    def normalized(self) -> str:
        return f'{self.name}/{self.qualifiers}'.rstrip('/').replace('/', '_')

    @property
    def soc(self) -> str | None:
        first = self.qualifiers.split('/')[0] if self.qualifiers else None
        return first if any(s.name == first for s in self.board.socs) else None

    def file_stems(self, with_revision: bool) -> tuple[str, str]:
        """Full and shortened file name stems, optionally with revision suffix.

        The shortened stem omits the SoC and is only valid for single-SoC
        boards; the caller must check that.
        """
        segments = self.qualifiers.split('/') if self.qualifiers else []
        suffix = []
        if with_revision and self.active_revision:
            suffix = [self.active_revision.replace('.', '_')]
        return (
            '_'.join([self.name, *segments, *suffix]),
            '_'.join([self.name, *segments[1:], *suffix]),
        )


def parse_board_components(target: str) -> tuple[str, str | None, str | None]:
    """Split '<board>[@<revision>][/<qualifiers>]' into its components."""
    match = BOARD_TARGET_RE.match(target)
    if match is None:
        raise BoardFactsError(
            f'Invalid revision / qualifiers format for {target}. '
            'Valid format is: <board>@<revision>/<qualifiers>'
        )
    revision = match.group(2)[1:] if match.group(2) else None
    return match.group(1), revision, match.group(4)


def _parse_cmake_sets(path: Path, regex: re.Pattern) -> dict[str, str]:
    if not path.is_file():
        return {}
    return dict(regex.findall(path.read_text(encoding='utf-8')))


def apply_board_aliases(name, revision, qualifiers, aliases):
    """Replace an aliased board name the way boards.cmake does."""
    alias = aliases.get(name)
    if alias is None:
        return name, revision, qualifiers
    name, alias_revision, alias_qualifiers = parse_board_components(alias)
    if revision is None:
        revision = alias_revision
    qualifiers = '/'.join(q for q in (alias_qualifiers, qualifiers) if q)
    return name, revision, qualifiers or None


def apply_deprecated_boards(name, revision, qualifiers, deprecated):
    """Replace a deprecated board name the way boards.cmake does.

    Entries in boards/deprecated.cmake are looked up by
    '<board>/<qualifiers>', so as in CMake an entry only applies when
    qualifiers were given.
    """
    key = f'{name}/{qualifiers}' if qualifiers else None
    replacement = deprecated.get(key)
    if replacement is None:
        return name, revision, qualifiers
    name, deprecated_revision, qualifiers = parse_board_components(replacement)
    if deprecated_revision is not None:
        if revision is not None:
            raise BoardFactsError(
                f'Invalid board revision: {revision}\n'
                f"Deprecated board '{key}' is now implemented as a revision of "
                f'another board ({name}@{deprecated_revision}), so the specified '
                'revision does not apply.'
            )
        revision = deprecated_revision
    return name, revision, qualifiers


def _revision_key(fmt: str, revision: str):
    if fmt == 'MAJOR.MINOR.PATCH':
        return tuple(int(p) for p in revision.split('.'))
    if fmt == 'NUMBER':
        return int(revision)
    return revision


def resolve_revision(board: list_boards.Board, revision: str | None):
    """Apply the board_check_revision() rules of extensions.cmake.

    Returns (requested revision, active revision). Both are None for a
    board without revisions. For the 'custom' format the revision.cmake
    logic cannot be evaluated here; the board's default revision is
    applied when none was given and the revision is otherwise used as-is.
    """
    fmt = board.revision_format
    if fmt is None:
        if revision is not None:
            raise BoardFactsError(
                f'Invalid board revision: {revision}\n'
                f"Board '{board.name}' does not define any revisions."
            )
        return None, None

    if fmt == 'custom':
        if revision is None:
            revision = board.revision_default
        return revision, revision

    if revision is None:
        if board.revision_default is None:
            raise BoardFactsError(
                f'No board revision specified, Board: `{board.name}` requires a revision.'
            )
        revision = board.revision_default

    fmt = fmt.upper()
    if fmt == 'LETTER':
        regex = r'[A-Z]'
    elif fmt == 'NUMBER':
        regex = r'[0-9]+'
    elif fmt == 'MAJOR.MINOR.PATCH':
        regex = r'(0|[1-9][0-9]*)\.[0-9]+\.[0-9]+'
        # Trailing zeroes may be omitted on the command line.
        if re.fullmatch(r'(0|[1-9][0-9]*)(\.[0-9]+)?(\.[0-9]+)?', revision):
            while revision.count('.') < 2:
                revision += '.0'
    else:
        raise BoardFactsError(f"Invalid revision format '{fmt}' for board '{board.name}'")

    if not re.fullmatch(regex, revision):
        raise BoardFactsError(
            f'Invalid revision format used for `{revision}`. '
            f'Board `{board.name}` uses revision format: {fmt}.'
        )

    valid = [r.name for r in board.revisions]
    if revision in valid:
        return revision, revision

    active = None
    if not board.revision_exact:
        requested_key = _revision_key(fmt, revision)
        lower = [r for r in valid if _revision_key(fmt, r) < requested_key]
        if lower:
            active = max(lower, key=lambda r: _revision_key(fmt, r))

    if active is None:
        raise BoardFactsError(
            f'Board revision `{revision}` for board `{board.name}` not found. '
            'Please specify a valid board revision.'
        )
    return revision, active


def resolve_qualifiers(board: list_boards.Board, qualifiers: str | None) -> tuple[str, bool]:
    """Default and validate qualifiers the way boards.cmake does."""
    valid = list_boards.board_v2_qualifiers(board)
    socs = [s.name for s in board.socs]
    single_soc = len(socs) == 1

    if valid:
        if single_soc:
            if qualifiers is None:
                qualifiers = socs[0]
            elif qualifiers.startswith('/'):
                qualifiers = socs[0] + qualifiers

        if qualifiers not in valid:
            targets = '\n'.join(f'{board.name}/{q}' for q in valid)
            raise BoardFactsError(
                f'Board qualifiers `{qualifiers}` for board `{board.name}` not found. '
                f'Valid board targets for {board.name} are:\n{targets}'
            )

    return qualifiers or '', single_soc


def resolve_board_target(
    target: str, boards: dict[str, list_boards.Board], aliases=None, deprecated=None
) -> BoardTarget:
    """Resolve a '<board>[@<revision>][/<qualifiers>]' string.

    'boards' is the dictionary returned by list_boards.find_v2_boards().
    """
    if deprecated is None:
        deprecated = _parse_cmake_sets(ZEPHYR_BASE / 'boards' / 'deprecated.cmake', DEPRECATED_RE)

    name, revision, qualifiers = parse_board_components(target)
    name, revision, qualifiers = apply_board_aliases(name, revision, qualifiers, aliases or {})
    name, revision, qualifiers = apply_deprecated_boards(name, revision, qualifiers, deprecated)

    board = boards.get(name)
    if board is None:
        raise BoardFactsError(f"No board named '{name}' found.")

    revision, active_revision = resolve_revision(board, revision)
    qualifiers, single_soc = resolve_qualifiers(board, qualifiers)

    return BoardTarget(board, target, revision, active_revision, qualifiers, single_soc)


def _find_board_file(bt: BoardTarget, full_name: str, short_name: str) -> list[Path]:
    """Find files named after the board target in the board directories.

    Applies the same naming checks as zephyr_file(CONF_FILES ... DTS).
    """
    found = []
    for directory in bt.board.directories:
        full = directory / full_name
        short = directory / short_name
        if short.exists() and not bt.single_soc:
            raise BoardFactsError(
                f'Board {bt.name} defines multiple SoCs. Shortened file name '
                f"({short_name}) not allowed, use '<board>_<soc>' naming"
            )
        if full.exists() and short.exists():
            raise BoardFactsError(
                f'Conflicting file names discovered. Cannot use both {full_name} '
                f'and {short_name}. Please choose one naming style, '
                f'{full_name} is recommended.'
            )
        if full.exists():
            found.append(full)
        elif short.exists():
            found.append(short)
    return found


def find_dts_files(bt: BoardTarget) -> tuple[Path, list[Path]]:
    """Locate the board .dts and the revision overlays applied on top."""
    full, short = bt.file_stems(with_revision=False)
    sources = _find_board_file(bt, f'{full}.dts', f'{short}.dts')
    if not sources:
        return STUB_DTS, []

    overlays = []
    if bt.active_revision:
        full, short = bt.file_stems(with_revision=True)
        overlays = _find_board_file(bt, f'{full}.overlay', f'{short}.overlay')
    return sources[-1], overlays


def extra_dtc_flags(bt: BoardTarget) -> list[str]:
    """Flags a board adds to EXTRA_DTC_FLAGS in its pre_dt_board.cmake."""
    path = bt.board.dir / 'pre_dt_board.cmake'
    if not path.is_file():
        return []
    flags = []
    for match in EXTRA_DTC_FLAGS_RE.findall(path.read_text(encoding='utf-8')):
        flags.extend(flag.strip('"') for flag in match.split())
    return flags


def find_preprocessor(preprocessor: str | None) -> list[str]:
    if preprocessor:
        return preprocessor.split()
    for candidate in DEFAULT_PREPROCESSORS:
        if shutil.which(candidate):
            return [candidate]
    raise BoardFactsError(
        'No C preprocessor found; install one of '
        f'{", ".join(DEFAULT_PREPROCESSORS)} or pass --preprocessor'
    )


def parse_deps_file(path: Path) -> list[Path]:
    """Return the input files listed in a 'gcc -MD' dependency file."""
    text = path.read_text(encoding='utf-8').replace('\\\n', ' ')
    _, _, deps = text.partition(':')
    return [Path(dep) for dep in deps.split()]


class BoardFacts:
    """Generates devicetree facts for board targets of one workspace."""

    def __init__(
        self,
        boards: dict[str, list_boards.Board],
        arch_roots: list[Path],
        dts_roots: list[Path],
        workspace_dir: Path | None = None,
        preprocessor: str | None = None,
        board_aliases: Path | None = None,
    ):
        self.boards = boards
        self.workspace_dir = workspace_dir or ZEPHYR_BASE.parent
        self.preprocessor = find_preprocessor(preprocessor)
        self.deprecated = _parse_cmake_sets(
            ZEPHYR_BASE / 'boards' / 'deprecated.cmake', DEPRECATED_RE
        )
        self.aliases = _parse_cmake_sets(board_aliases, ALIAS_RE) if board_aliases else {}

        # DTS_ROOT contributed by modules; the board directory and
        # ZEPHYR_BASE are appended per target, as pre_dt.cmake does.
        self.module_dts_roots = list(dts_roots)

        arch_args = argparse.Namespace(arch_roots=[*arch_roots, ZEPHYR_BASE], arch=None)
        archs = list_hardware.find_v2_archs(arch_args)['archs']
        self.include_subdirs = [
            *DTS_ROOT_INCLUDE_SUBDIRS,
            *(f'dts/{arch["name"]}' for arch in archs),
            'dts',
        ]

    def all_targets(self, boards=None) -> list[str]:
        """All '<board>/<qualifiers>' targets, at the boards' default revisions."""
        return [
            f'{board.name}/{qualifier}'
            for board in (boards or self.boards).values()
            for qualifier in list_boards.board_v2_qualifiers(board)
        ]

    def resolve(self, target: str) -> BoardTarget:
        return resolve_board_target(target, self.boards, self.aliases, self.deprecated)

    def _dts_roots_for(self, bt: BoardTarget) -> list[Path]:
        return list(unique_paths([*self.module_dts_roots, bt.board.dir, ZEPHYR_BASE]))

    def _include_dirs(self, dts_roots: list[Path]) -> list[Path]:
        dirs = []
        for root in dts_roots:
            for subdir in self.include_subdirs:
                path = (root / subdir).resolve()
                if path.exists() and path not in dirs:
                    dirs.append(path)
        return dirs

    @staticmethod
    def _bindings(dts_roots: list[Path]) -> tuple[list[Path], list[Path]]:
        bindings_dirs = []
        vendor_prefixes = []
        for root in dts_roots:
            bindings = root / 'dts' / 'bindings'
            if bindings.is_dir():
                bindings_dirs.append(bindings)
            prefixes = bindings / 'vendor-prefixes.txt'
            if prefixes.is_file():
                vendor_prefixes.append(prefixes)
        return bindings_dirs, vendor_prefixes

    def _preprocess(self, dts_files, include_dirs, out_file, deps_file, cwd) -> None:
        cmd = [*self.preprocessor, '-x', 'assembler-with-cpp', '-nostdinc']
        for directory in include_dirs:
            cmd += ['-isystem', str(directory)]
        for dts_file in dts_files:
            cmd += ['-include', str(dts_file)]
        cmd += ['-undef', '-D__DTS__', '-E', '-MD', '-MF', str(deps_file)]
        cmd += ['-o', str(out_file), str(EMPTY_FILE)]

        result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
        if result.returncode != 0:
            raise BoardFactsError(
                f'failed to preprocess devicetree files (error code {result.returncode}): '
                f'{" ".join(str(f) for f in dts_files)}\n{result.stderr.strip()}'
            )

    def _rel(self, path) -> str:
        path = Path(path)
        try:
            return path.resolve().relative_to(self.workspace_dir.resolve()).as_posix()
        except ValueError:
            return path.as_posix()

    def generate(self, target: str, dts_out: Path | None = None) -> dict:
        """Generate the facts dictionary for one board target.

        The final merged devicetree is written to 'dts_out' when given.
        """
        bt = self.resolve(target)
        dts_source, overlays = find_dts_files(bt)
        dts_roots = self._dts_roots_for(bt)
        include_dirs = self._include_dirs(dts_roots)
        bindings_dirs, vendor_prefix_files = self._bindings(dts_roots)
        dtc_flags = extra_dtc_flags(bt)

        with tempfile.TemporaryDirectory(prefix='board_facts_') as tmp:
            dts_pre = Path(tmp) / f'{bt.normalized}.dts.pre'
            dts_deps = Path(tmp) / f'{bt.normalized}.dts.d'
            self._preprocess([dts_source, *overlays], include_dirs, dts_pre, dts_deps, bt.board.dir)
            dts_files = parse_deps_file(dts_deps)

            vendor_prefixes = {}
            for prefixes in vendor_prefix_files:
                vendor_prefixes.update(edtlib.load_vendor_prefixes_txt(str(prefixes)))

            try:
                edt = edtlib.EDT(
                    str(dts_pre),
                    [str(d) for d in bindings_dirs],
                    workspace_dir=str(self.workspace_dir),
                    warn_reg_unit_address_mismatch='-Wno-simple_bus_reg' not in dtc_flags,
                    default_prop_types=True,
                    infer_binding_for_paths=['/zephyr,user', '/cpus'],
                    vendor_prefixes=vendor_prefixes,
                )
            except edtlib.EDTError as e:
                raise BoardFactsError(f'devicetree error: {e}') from e

        if dts_out is not None:
            dts_out.write_text(edt.dts_source + '\n', encoding='utf-8')

        return {
            'board': self._board_facts(bt),
            'devicetree': {
                'source': self._rel(dts_source),
                'overlays': [self._rel(o) for o in overlays],
                'files': [self._rel(f) for f in dts_files if f != EMPTY_FILE],
                'include_dirs': [self._rel(d) for d in include_dirs],
                'bindings_dirs': [self._rel(d) for d in bindings_dirs],
                'vendor_prefixes': [self._rel(f) for f in vendor_prefix_files],
                'extra_dtc_flags': dtc_flags,
            },
            'chosen': {name: node.path for name, node in edt.chosen_nodes.items()},
            'aliases': {alias: node.path for node in edt.nodes for alias in node.aliases},
            'labels': {label: node.path for label, node in edt.label2node.items()},
            'compatibles': {
                'okay': sorted(edt.compat2okay),
                'all': sorted(edt.compat2nodes),
            },
            'nodes': [
                self._node_facts(node) for node in sorted(edt.nodes, key=lambda n: n.dep_ordinal)
            ],
        }

    @staticmethod
    def _board_facts(bt: BoardTarget) -> dict:
        return {
            'name': bt.name,
            'full_name': bt.board.full_name,
            'vendor': bt.board.vendor,
            'hwm': bt.board.hwm,
            'target': bt.target,
            'requested_target': bt.requested,
            'normalized_target': bt.normalized,
            'qualifiers': bt.qualifiers,
            'soc': bt.soc,
            'revision': {
                'format': bt.board.revision_format,
                'default': bt.board.revision_default,
                'requested': bt.revision,
                'active': bt.active_revision,
            },
            'directories': [d.as_posix() for d in bt.board.directories],
        }

    def _node_facts(self, node: edtlib.Node) -> dict:
        return {
            'path': node.path,
            'name': node.name,
            'unit_addr': node.unit_addr,
            'labels': node.labels,
            'aliases': node.aliases,
            'status': node.status,
            'compats': node.compats,
            'matching_compat': node.matching_compat,
            'binding': self._rel(node.binding_path) if node.binding_path else None,
            'parent': node.parent.path if node.parent else None,
            'on_bus': node.on_bus,
            'buses': node.buses,
            'dep_ordinal': node.dep_ordinal,
            'regs': [{'name': r.name, 'addr': r.addr, 'size': r.size} for r in node.regs],
            'interrupts': [self._controller_and_data(i) for i in node.interrupts],
            'props': {name: self._prop_value(prop.val) for name, prop in node.props.items()},
        }

    @classmethod
    def _controller_and_data(cls, cad: edtlib.ControllerAndData) -> dict:
        return {
            'name': cad.name,
            'controller': cad.controller.path,
            'data': {k: cls._prop_value(v) for k, v in cad.data.items()},
        }

    @classmethod
    def _prop_value(cls, val):
        if isinstance(val, edtlib.Node):
            return val.path
        if isinstance(val, edtlib.ControllerAndData):
            return cls._controller_and_data(val)
        if isinstance(val, bytes):
            return list(val)
        if isinstance(val, list):
            return [cls._prop_value(v) for v in val]
        return val


def parse_args():
    parser = argparse.ArgumentParser(allow_abbrev=False, description=__doc__)
    list_boards.add_args(parser)
    add_args(parser)
    parser.add_argument(
        '--dts-root',
        dest='dts_roots',
        default=[],
        type=Path,
        action='append',
        help='add a devicetree root, may be given more than once',
    )
    parser.add_argument(
        '--workspace-dir',
        type=Path,
        help='directory used as reference for relative paths (e.g. WEST_TOPDIR)',
    )
    return parser.parse_args()


def add_args(parser):
    # Remember to update west-completion.bash if you add or remove
    # flags
    parser.add_argument(
        '-t',
        '--target',
        dest='targets',
        default=[],
        action='append',
        help='''board target (<board>[@<revision>][/<qualifiers>]) to
                generate facts for, may be given more than once; without it
                facts are generated for every target of every listed board,
                at each board's default revision''',
    )
    parser.add_argument(
        '--facts-dir',
        type=Path,
        help='''write one <board target>.json and .dts file per target here
                instead of printing the facts to stdout''',
    )
    parser.add_argument(
        '--preprocessor',
        help=f'''C preprocessor to run on devicetree files (default: the
                first of {", ".join(DEFAULT_PREPROCESSORS)} found)''',
    )


def generate_all(generator: BoardFacts, targets, facts_dir, out=sys.stdout, err=print):
    """Generate facts for 'targets'; returns the number of failures.

    Facts go to one file per target in 'facts_dir' when given, and are
    otherwise printed to 'out' as one JSON object keyed by target.
    """
    if facts_dir is not None:
        facts_dir.mkdir(parents=True, exist_ok=True)

    facts = {}
    failures = 0
    for target in targets:
        try:
            bt = generator.resolve(target)
            # Named like the board's own files, so revisions do not collide.
            stem = bt.file_stems(with_revision=True)[0]
            dts_out = facts_dir / f'{stem}.dts' if facts_dir else None
            target_facts = generator.generate(target, dts_out)
        except BoardFactsError as e:
            err(f'{target}: {e}')
            failures += 1
            continue

        if facts_dir is None:
            facts[bt.target] = target_facts
        else:
            json_out = facts_dir / f'{stem}.json'
            with json_out.open('w', encoding='utf-8') as f:
                json.dump(target_facts, f, indent=2)
                f.write('\n')
            out.write(f'{bt.target}: {json_out}\n')

    if facts_dir is None:
        json.dump(facts, out, indent=2)
        out.write('\n')
    return failures


def main():
    args = parse_args()
    edtlib_logger.setup_edtlib_logging()

    args.arch_roots.append(ZEPHYR_BASE)
    args.board_roots.append(ZEPHYR_BASE)
    args.soc_roots.append(ZEPHYR_BASE)

    boards = list_boards.find_v2_boards(args)
    generator = BoardFacts(
        boards,
        args.arch_roots,
        args.dts_roots,
        workspace_dir=args.workspace_dir,
        preprocessor=args.preprocessor,
    )
    targets = args.targets or generator.all_targets()
    failures = generate_all(
        generator, targets, args.facts_dir, err=lambda m: print(f'ERROR: {m}', file=sys.stderr)
    )
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, BoardFactsError) as e:
        print(f'ERROR: {e}', file=sys.stderr)
        sys.exit(1)
