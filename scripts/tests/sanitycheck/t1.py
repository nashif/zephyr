# content of test_example.py
import pytest
import imp
import sys
import os
import string

sc = imp.load_source(".", "sanitycheck")

ZEPHYR_BASE = os.environ.get('ZEPHYR_BASE')

sys.path.insert(0, os.path.join(ZEPHYR_BASE, "scripts/"))

from sanity_chk import scl
from sanity_chk import expr_parser


def test_schema_parser_pass():
    d = {'tests': {'kernel.semaphore': {'min_ram': 32, 'tags': 'kernel userspace'}}}
    schema = scl.yaml_load("sanity_chk/testcase-schema.yaml")
    p = sc.SanityConfigParser("tests/sanitycheck/1.yaml", schema)
    p.load()

    assert p.data == d

def test_schema_parser_fail():
    schema = scl.yaml_load("sanity_chk/testcase-schema.yaml")
    p = sc.SanityConfigParser("tests/sanitycheck/2.yaml", schema)
    with pytest.raises(Exception):
        p.load()





