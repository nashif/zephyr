# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

import argparse
import json
import os
import re
import sys
import textwrap
from pathlib import Path

from west.commands import WestCommand

from zephyr_ext_common import ZEPHYR_BASE

sys.path.append(os.fspath(Path(__file__).parent.parent))
import list_samples
import zephyr_module


class Samples(WestCommand):

    def __init__(self):
        super().__init__(
            'samples',
            '',
            description='Display information about code samples',
            accepts_unknown_args=False)

    def do_add_parser(self, parser_adder):
        default_fmt = '{id}'
        parser = parser_adder.add_parser(
            self.name,
            formatter_class=argparse.RawDescriptionHelpFormatter,
            description=self.description,
            epilog=textwrap.dedent(f'''\
            FORMAT STRINGS
            --------------

            Samples are listed using a Python 3 format string. Arguments
            to the format string are accessed by name.

            The default format string is:

            "{default_fmt}"

            The following arguments are available:

            - id: sample id, as declared by its code-sample directive
            - name: human readable sample name
            - description: one line sample description
            - category: category the sample is listed under, if any
            - dir: directory that contains the sample
            - doc: documentation file declaring the sample
            - relevant_api: comma separated list of relevant API groups
            - applications: comma separated list of application directories
            - app_count: number of applications the sample contains

            EXAMPLES
            --------

            List every sample in the bluetooth category:

              west samples --category bluetooth

            Find the samples exercising a given API:

              west samples --api gpio_interface -f '{{id}}: {{name}}'

            List the samples that build more than one application:

              west samples -f '{{id}} ({{app_count}})' --min-apps 2
            '''))

        # Remember to update west-completion.bash if you add or remove
        # flags
        parser.add_argument('-f', '--format', default=None,
                            help='''Format string to use to list each sample;
                                    see FORMAT STRINGS below.''')
        parser.add_argument('-l', '--long', action='store_true',
                            help='''display a detailed, multi line entry for
                                    each sample''')
        parser.add_argument('--json', action='store_true',
                            help='''output all sample metadata as JSON''')

        group = parser.add_argument_group('filtering')
        group.add_argument('-i', '--id', dest='id_re',
                           help='''a regular expression; only samples whose ids
                           match ID_RE will be listed''')
        group.add_argument('-n', '--name', dest='name_re',
                           help='''a regular expression; only samples whose
                           names match NAME_RE will be listed''')
        group.add_argument('-c', '--category', dest='categories', default=[],
                           action='append',
                           help='''only list samples in this category, may be
                           given more than once''')
        group.add_argument('--api', dest='apis', default=[], action='append',
                           help='''only list samples declaring this API as
                           relevant, may be given more than once''')
        group.add_argument('-p', '--path', dest='path_re',
                           help='''a regular expression; only samples whose
                           directories match PATH_RE will be listed''')
        group.add_argument('--min-apps', type=int, default=None,
                           help='''only list samples containing at least this
                           many applications''')
        group.add_argument('--orphans', action='store_true',
                           help='''instead of listing samples, list the
                           applications that no sample claims: either samples
                           missing a code-sample directive, or applications
                           that are not samples at all''')

        list_samples.add_args(parser)

        return parser

    def do_run(self, args, _):
        args.sample_roots += self.sample_roots()

        if args.orphans:
            for orphan in list_samples.find_orphans(args):
                self.inf(orphan.as_posix())
            return

        samples = [s for s in list_samples.find_samples(args) if self.matches(s, args)]

        if args.json:
            self.inf(json.dumps([list_samples.as_dict(s) for s in samples], indent=2))
        elif args.long:
            for sample in samples:
                self.long_entry(sample)
        else:
            for sample in samples:
                self.inf((args.format or '{id}').format(**self.format_args(sample)))

    def sample_roots(self):
        '''The in tree samples, plus whatever the modules declare.'''
        roots = [Path(ZEPHYR_BASE) / 'samples']

        for module in zephyr_module.parse_modules(ZEPHYR_BASE, self.manifest):
            for sample_root in module.meta.get('samples', []):
                if sample_root:
                    roots.append(Path(module.project) / sample_root)

        return roots

    @staticmethod
    def matches(sample, args):
        return all([
            args.id_re is None or re.search(args.id_re, sample.id),
            args.name_re is None or re.search(args.name_re, sample.name),
            args.path_re is None or re.search(args.path_re, sample.dir.as_posix()),
            not args.categories or sample.category in args.categories,
            not args.apis or set(args.apis) & set(sample.relevant_api),
            args.min_apps is None or len(sample.applications) >= args.min_apps,
        ])

    @staticmethod
    def format_args(sample):
        return {
            'id': sample.id,
            'name': sample.name,
            'description': sample.description,
            'category': sample.category or '',
            'dir': sample.dir.as_posix(),
            'doc': sample.doc.as_posix(),
            'relevant_api': ','.join(sample.relevant_api),
            'applications': ','.join(app.as_posix() for app in sample.applications),
            'app_count': len(sample.applications),
        }

    def long_entry(self, sample):
        self.inf(f'{sample.id}')
        self.inf(f'  name:        {sample.name}')
        if sample.description:
            for i, line in enumerate(textwrap.wrap(sample.description, width=64)):
                self.inf(f'  {"description:" if i == 0 else "            "} {line}')
        if sample.category:
            self.inf(f'  category:    {sample.category}')
        self.inf(f'  dir:         {sample.dir.as_posix()}')
        self.inf(f'  doc:         {sample.doc.as_posix()}')
        if sample.relevant_api:
            self.inf(f'  api:         {", ".join(sample.relevant_api)}')
        for i, app in enumerate(sample.applications):
            self.inf(f'  {"apps:" if i == 0 else "     "}        {app.as_posix()}')
        self.inf('')
