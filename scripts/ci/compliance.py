#!/usr/bin/env python
import sys
import subprocess
import re
import os
import xml.etree.ElementTree as ET

commit_range = os.environ['COMMIT_RANGE']
cwd = os.environ['ZEPHYR_BASE']

def run_gitlint(tc):
    proc = subprocess.Popen('gitlint --commits %s' %(commit_range),
            cwd=cwd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    msg = ""
    if proc.wait() != 0:
        msg = proc.stdout.read()

    if msg != "":
        failure = ET.SubElement(tc, 'failure', type="failure", message="commit message error")
        failure.text = (str(msg))
        return 1

    return 0


def run_checkpatch(tc):
    output = None
    out = ""

    diff = subprocess.Popen(('git', 'diff', '%s' %(commit_range)), stdout=subprocess.PIPE)
    try:
        output = subprocess.check_output(('%s/scripts/checkpatch.pl' %cwd,
            '--mailback', '--no-tree', '-'), stdin=diff.stdout,
            stderr=subprocess.STDOUT, shell=True)

    except subprocess.CalledProcessError as ex:
        failure = ET.SubElement(tc, 'failure', type="failure", message="check patch issues")
        failure.text = (str(ex.output))
        return 1

    return 0





tests = {"gitlint":run_gitlint, "checkpatch":run_checkpatch}

def run_tests():
    run = "Commit"
    eleTestsuite = None
    fails = 0
    passes = 0
    errors = 0
    total = 0
    filename = "compliance.xml"

    eleTestsuites = ET.Element('testsuites')
    eleTestsuite = ET.SubElement(eleTestsuites, 'testsuite', name=run,
            tests="%d" %(errors + passes + fails),  failures="%d" %fails,  errors="%d" %errors, skip="0")

    for test in tests.keys():

        total += 1
        eleTestcase = ET.SubElement(eleTestsuite, 'testcase', name="%s" %(test),
                time="0")

        fails += tests[test](eleTestcase)


    eleTestsuite.set("tests", "%s" %total)
    eleTestsuite.set("failures",  "%s" %fails)

    result = ET.tostring(eleTestsuites)
    f = open(filename, 'wb')
    f.write(result)
    f.close()

run_tests()
