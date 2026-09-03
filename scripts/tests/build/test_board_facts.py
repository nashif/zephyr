#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

"""Tests for the board target resolution in board_facts.py."""

import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, os.path.join(os.environ["ZEPHYR_BASE"], "scripts"))
import board_facts as iut  # Implementation Under Test
import list_boards


def make_board(name, socs, revision_format=None, revisions=(), default=None, exact=False):
    return list_boards.Board(
        name=name,
        directories=[Path('/boards') / name],
        hwm='v2',
        revision_format=revision_format,
        revision_default=default,
        revision_exact=exact,
        revisions=[list_boards.Revision(r) for r in revisions],
        socs=[list_boards.Soc(s, variants=[list_boards.Variant('ns')]) for s in socs],
    )


BOARDS = {
    'single': make_board('single', ['soc1']),
    'multi': make_board('multi', ['soc1', 'soc2']),
    'mmp': make_board(
        'mmp', ['soc1'], 'major.minor.patch', ['0.7.0', '0.14.0', '1.2.3'], default='0.14.0'
    ),
    'mmp_exact': make_board('mmp_exact', ['soc1'], 'major.minor.patch', ['1.0.0'], exact=True),
    'letter': make_board('letter', ['soc1'], 'letter', ['A', 'C'], default='A'),
    'number': make_board('number', ['soc1'], 'number', ['2', '10'], default='2'),
    'custom': make_board('custom', ['soc1'], 'custom'),
    'custom_default': make_board('custom_default', ['soc1'], 'custom', ['r1'], default='r1'),
}


@pytest.mark.parametrize(
    'target, expected',
    [
        ('board', ('board', None, None)),
        ('board@1.0', ('board', '1.0', None)),
        ('board/soc/ns', ('board', None, 'soc/ns')),
        ('board@A/soc', ('board', 'A', 'soc')),
        ('board//ns', ('board', None, '/ns')),
    ],
)
def test_parse_board_components(target, expected):
    assert iut.parse_board_components(target) == expected


@pytest.mark.parametrize('target', ['board@1@2', 'board/soc@1', '@1', 'board@'])
def test_parse_board_components_invalid(target):
    with pytest.raises(iut.BoardFactsError):
        iut.parse_board_components(target)


@pytest.mark.parametrize(
    'target, expected',
    [
        ('single', 'single/soc1'),
        ('single/soc1', 'single/soc1'),
        ('single//ns', 'single/soc1/ns'),
        ('multi/soc2', 'multi/soc2'),
        ('multi/soc1/ns', 'multi/soc1/ns'),
    ],
)
def test_qualifiers(target, expected):
    bt = iut.resolve_board_target(target, BOARDS, deprecated={})
    assert bt.target == expected
    assert bt.normalized == expected.replace('/', '_')
    assert bt.soc == 'soc1' if 'soc1' in expected else 'soc2'


@pytest.mark.parametrize('target', ['multi', 'multi//ns', 'single/soc2', 'nosuch', 'single@1'])
def test_invalid_targets(target):
    with pytest.raises(iut.BoardFactsError):
        iut.resolve_board_target(target, BOARDS, deprecated={})


@pytest.mark.parametrize(
    'target, requested, active',
    [
        ('mmp', '0.14.0', '0.14.0'),
        ('mmp@0.7.0', '0.7.0', '0.7.0'),
        ('mmp@1', '1.0.0', '0.14.0'),
        ('mmp@1.2', '1.2.0', '0.14.0'),
        ('mmp@2.0.0', '2.0.0', '1.2.3'),
        ('letter', 'A', 'A'),
        ('letter@B', 'B', 'A'),
        ('number@9', '9', '2'),
        ('number@11', '11', '10'),
        ('custom', None, None),
        ('custom@foo', 'foo', 'foo'),
        ('custom_default', 'r1', 'r1'),
        ('custom_default@r9', 'r9', 'r9'),
    ],
)
def test_revisions(target, requested, active):
    bt = iut.resolve_board_target(target, BOARDS, deprecated={})
    assert (bt.revision, bt.active_revision) == (requested, active)
    if active:
        assert bt.target == f'{target.split("@")[0]}@{active}/soc1'


@pytest.mark.parametrize(
    'target', ['mmp@0.1.0', 'mmp@x', 'mmp_exact@1.1.0', 'mmp_exact', 'letter@a', 'number@1']
)
def test_invalid_revisions(target):
    with pytest.raises(iut.BoardFactsError):
        iut.resolve_board_target(target, BOARDS, deprecated={})


def test_file_stems():
    bt = iut.resolve_board_target('mmp@0.7.0/soc1/ns', BOARDS, deprecated={})
    assert bt.file_stems(with_revision=False) == ('mmp_soc1_ns', 'mmp_ns')
    assert bt.file_stems(with_revision=True) == ('mmp_soc1_ns_0_7_0', 'mmp_ns_0_7_0')


def test_deprecated_and_aliases():
    deprecated = {'old/soc1': 'single/soc1/ns', 'gone/soc9': 'mmp@0.7.0/soc1'}
    aliases = {'nick': 'multi/soc2'}

    assert iut.resolve_board_target('old/soc1', BOARDS, deprecated=deprecated).target == (
        'single/soc1/ns'
    )
    assert iut.resolve_board_target('gone/soc9', BOARDS, deprecated=deprecated).target == (
        'mmp@0.7.0/soc1'
    )
    with pytest.raises(iut.BoardFactsError):
        iut.resolve_board_target('gone@0.14.0/soc9', BOARDS, deprecated=deprecated)
    # Entries without qualifiers are dead in CMake as well.
    with pytest.raises(iut.BoardFactsError):
        iut.resolve_board_target('old', BOARDS, deprecated=deprecated)
    assert iut.resolve_board_target('nick', BOARDS, aliases=aliases, deprecated={}).target == (
        'multi/soc2'
    )
    assert iut.resolve_board_target('nick/ns', BOARDS, aliases=aliases, deprecated={}).target == (
        'multi/soc2/ns'
    )


def test_parse_deps_file(tmp_path):
    deps = tmp_path / 'x.d'
    deps.write_text('empty_file.o: misc/empty_file.c \\\n a/b.dts \\\n c.dtsi d.dtsi\n')
    assert iut.parse_deps_file(deps) == [
        Path('misc/empty_file.c'),
        Path('a/b.dts'),
        Path('c.dtsi'),
        Path('d.dtsi'),
    ]
