
import os
import sys
from pathlib import Path
from lxml import objectify
import re

xml_root = "doc/_build/doxygen/xml"


def debug(str):
    print(str)

try:
    verify_file = os.path.join(xml_root, "verify.xml")
    verify_list = objectify.parse(verify_file)
    varlist = verify_list.xpath("//variablelist/node()")
    for ve in varlist:
        term = ve.xpath("term/text()")
        if term:
            tt = term[-1].strip()
            ttm = re.compile(r"^\s*\(\s*(?P<suite_name>[a-zA-Z0-9_]+)\s*,"
                             r"\s*(?P<testcase_name>[a-zA-Z0-9_]+)\s*", re.MULTILINE)
            m = ttm.match(tt)
            suite = m.group("suite_name")
            case = m.group("testcase_name")
            print(f"suite: {suite}")
            print(f"case: {case}")

        refs = ve.xpath("term/ref")
        for ref in refs:
            refid = str(ref.xpath("@refid")[0])
            debug(f"Working on {ref} ({refid})...")

        reqs = ve.xpath("para/ref")
        for req in reqs:
            print(f"Requirement {req}")

except OSError as e:
    print(str(e))
    sys.exit(2)
