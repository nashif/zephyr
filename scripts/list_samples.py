#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

'''Discover Zephyr code samples.

A sample is declared by a ``zephyr:code-sample::`` directive in a
reStructuredText file, which is also what renders it in the documentation:

    .. zephyr:code-sample:: hello_world
       :name: Hello World

       Print "Hello World" to the console.

The directive is the single source of truth for sample identity. The directory
holding the declaring file is the sample directory, and every application
underneath it (a directory with a Zephyr ``CMakeLists.txt``) belongs to that
sample, unless a nested sample claims it first. A sample therefore may own more
than one application, as is the case for the various initiator/reflector and
host/remote sample pairs.

Categories are resolved the same way, from the ``zephyr:code-sample-category::``
directive of the nearest enclosing directory.
'''

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

#
# This module backs the 'west samples' extension command, and is written so
# that it can be imported, or run directly, without west being installed. If
# you change it, make sure to test both ways it can be used.
#

# Directives are only recognized at column 0. This is what keeps the examples
# in doc/contribute/documentation/guidelines.rst, which are indented inside a
# literal block, from being picked up as real samples.
SAMPLE_RE = re.compile(r'^\.\. zephyr:code-sample:: +(\S+) *$')
CATEGORY_RE = re.compile(r'^\.\. zephyr:code-sample-category:: +(\S+) *$')
OPTION_RE = re.compile(r'^\s+:([\w-]+): *(.*)$')

# Enough inline reStructuredText to turn a one-line sample description into
# something worth printing on a terminal.
ROLE_RE = re.compile(r':[\w:+.-]+:`([^`]*)`')
LINK_RE = re.compile(r'`([^`<]+?) *<[^`>]*>`_+')
LITERAL_RE = re.compile(r'``([^`]*)``')
INTERPRETED_RE = re.compile(r'`([^`]*)`')
EMPHASIS_RE = re.compile(r'\*{1,2}([^*]+)\*{1,2}')

CMAKELISTS = 'CMakeLists.txt'


@dataclass(frozen=True)
class Sample:
    id: str
    name: str
    dir: Path
    doc: Path
    description: str = ''
    category: str | None = None
    relevant_api: list[str] = field(default_factory=list)
    applications: list[Path] = field(default_factory=list)


def sample_key(sample):
    return sample.id


def plain_text(text):
    '''Strip the inline markup a sample description is likely to contain.'''
    text = ROLE_RE.sub(lambda m: (m.group(1).split('<')[0].strip() or m.group(1)), text)
    text = LINK_RE.sub(r'\1', text)
    text = LITERAL_RE.sub(r'\1', text)
    text = INTERPRETED_RE.sub(r'\1', text)
    text = EMPHASIS_RE.sub(r'\1', text)
    return ' '.join(text.split())


def parse_directive(sample_id, lines, doc):
    '''Parse the options and first description paragraph of a directive.

    'lines' are the lines following the directive itself.
    '''
    options = {}
    description = []
    in_options = True

    for line in lines:
        if not line.strip():
            # A blank line ends the description, but not the option block.
            if description:
                break
            continue
        if not line[0].isspace():
            # Dedent: the directive body is over.
            break
        match = OPTION_RE.match(line)
        if in_options and match:
            options[match.group(1)] = match.group(2).strip()
            continue
        in_options = False
        description.append(line.strip())

    return Sample(
        id=sample_id,
        name=options.get('name', sample_id),
        dir=doc.parent,
        doc=doc,
        description=plain_text(' '.join(description)),
        relevant_api=options.get('relevant-api', '').split(),
    )


def parse_rst(doc):
    '''Return the (sample, category) declared by a single file.

    Either may be None. Only the first declaration of each kind is used.
    '''
    try:
        lines = doc.read_text(encoding='utf-8', errors='replace').splitlines()
    except OSError:
        return None, None

    sample = None
    category = None

    for i, line in enumerate(lines):
        if category is None:
            match = CATEGORY_RE.match(line)
            if match:
                category = match.group(1)
                continue
        if sample is None:
            match = SAMPLE_RE.match(line)
            if match:
                sample = parse_directive(match.group(1), lines[i + 1:], doc)

    return sample, category


def is_application(cmakelists):
    try:
        return 'find_package(Zephyr' in cmakelists.read_text(encoding='utf-8', errors='replace')
    except OSError:
        return False


def nearest(directory, root, mapping):
    '''Look up 'directory' and then its parents, stopping above 'root'.'''
    for path in [directory, *directory.parents]:
        if path in mapping:
            return mapping[path]
        if path == root:
            break
    return None


def find_samples_in(root):
    root = Path(root).resolve()
    if not root.is_dir():
        return []

    found = []
    categories = {}

    for doc in sorted(root.rglob('*.rst')):
        sample, category = parse_rst(doc)
        if category is not None:
            categories.setdefault(doc.parent, category)
        if sample is not None:
            found.append(sample)

    samples = dedupe(found)
    by_dir = {sample.dir: sample for sample in samples}

    # A sample owns every application beneath it that no nested sample claims.
    for cmakelists in sorted(root.rglob(CMAKELISTS)):
        if not is_application(cmakelists):
            continue
        owner = nearest(cmakelists.parent, root, by_dir)
        if owner is not None:
            owner.applications.append(cmakelists.parent)

    return sorted(
        (
            Sample(
                id=sample.id,
                name=sample.name,
                dir=sample.dir,
                doc=sample.doc,
                description=sample.description,
                category=nearest(sample.dir, root, categories),
                relevant_api=sample.relevant_api,
                applications=sample.applications,
            )
            for sample in samples
        ),
        key=sample_key,
    )


def find_orphans_in(root):
    '''Return applications under 'root' that no sample claims.

    These are either samples still missing a code-sample directive, or
    applications that live under a sample root without being samples.
    '''
    root = Path(root).resolve()
    if not root.is_dir():
        return []

    by_dir = {sample.dir: sample for sample in find_samples_in(root)}
    orphans = [
        cmakelists.parent
        for cmakelists in sorted(root.rglob(CMAKELISTS))
        if is_application(cmakelists) and nearest(cmakelists.parent, root, by_dir) is None
    ]
    return orphans


def dedupe(samples):
    '''Drop samples whose id was already claimed, warning about each.

    Sample ids are a flat namespace shared by the whole workspace, because that
    is what the documentation cross references resolve against. The first
    declaration wins, which is also what the Sphinx domain does.
    '''
    seen = {}
    ret = []

    for sample in samples:
        first = seen.get(sample.id)
        if first is not None:
            print(f'WARNING: duplicate code sample id {sample.id!r} in {sample.doc}, '
                  f'already declared in {first.doc}', file=sys.stderr)
            continue
        seen[sample.id] = sample
        ret.append(sample)

    return ret


def find_samples(args):
    ret = []
    for root in args.sample_roots:
        ret.extend(find_samples_in(root))
    return dedupe(sorted(ret, key=sample_key))


def find_orphans(args):
    ret = []
    for root in args.sample_roots:
        ret.extend(find_orphans_in(root))
    return sorted(ret)


def as_dict(sample):
    return {
        'id': sample.id,
        'name': sample.name,
        'description': sample.description,
        'category': sample.category,
        'dir': sample.dir.as_posix(),
        'doc': sample.doc.as_posix(),
        'relevant_api': sample.relevant_api,
        'applications': [app.as_posix() for app in sample.applications],
    }


def parse_args():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    add_args(parser)
    add_args_formatting(parser)
    return parser.parse_args()


def add_args(parser):
    # Remember to update west-completion.bash if you add or remove
    # flags
    parser.add_argument('--sample-root', dest='sample_roots', default=[],
                        type=Path, action='append',
                        help='add a sample root, may be given more than once')


def add_args_formatting(parser):
    parser.add_argument('--json', action='store_true',
                        help='''output list of samples in JSON format''')


def dump_samples(samples):
    if args.json:
        print(json.dumps([as_dict(sample) for sample in samples]))
    else:
        for sample in samples:
            print(f'  {sample.id}')


if __name__ == '__main__':
    args = parse_args()
    dump_samples(find_samples(args))
