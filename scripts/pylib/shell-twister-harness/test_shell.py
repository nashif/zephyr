# Copyright (c) 2025 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

import logging
import os
import re

import parametrize_from_file
from twister_harness import Shell

logger = logging.getLogger(__name__)

PARAM_FILE = os.environ.get("TESTDATA_FILE", "default_params.yaml")


@parametrize_from_file(path=PARAM_FILE)  # pylint: disable=not-callable
def test_shell_harness(shell: Shell, command, expected):
    logger.info('send command: %s', command)
    lines = shell.exec_command(command)
    match = False
    for line in lines:
        if re.match(expected, line):
            match = True
            break

    assert match, 'expected response not found'
    logger.info('response is valid')
