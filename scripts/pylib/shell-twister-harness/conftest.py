# Copyright (c) 2025 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

import os


def pytest_addoption(parser):
    parser.addoption('--testdata')


def pytest_configure(config):
    # Retrieve the value of the command-line option.
    param_file = config.getoption("--testdata")
    # Set an environment variable so that it's accessible at module import time.
    os.environ["TESTDATA_FILE"] = param_file
