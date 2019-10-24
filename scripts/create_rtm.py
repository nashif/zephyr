#!/usr/bin/env python3

from lxml import etree
from lxml import objectify
import xlwt
import xlrd
import argparse
import os
import sys

from junitparser import TestCase, TestSuite, JUnitXml, Skipped, Error


args = None

class TestCase:
    def __init__(self, name, refid=None, anchor=None):
        self.name = name
        self.refid = refid
        self.anchor = anchor
        self.group = None
        self.brief = ""

    def __str__(self):
        return self.name

class TestSuite:
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.testcases = []

    def get_or_create(self, name):
        for tc in self.testcases:
            if tc.name == name:
                return tc
        tc = TestCase(name=name)
        self.add(tc)
        return tc

    def get_by_id(self, refid):
        for tc in self.testcases:
            if tc.refid == refid:
                return tc
        return None


    def dump(self, group=None):
        for tc in self.testcases:
            if group:
                if group in tc.group:
                    print("{}:\n  id: {}\n  group: {}\n".format(tc.name, tc.refid, tc.group))
            else:
                print("{}:\n  id: {}\n  group: {}\n".format(tc.name, tc.refid, tc.group))

    def add(self, testcase):
        self.testcases.append(testcase)

    def stringify_children(self, node):
        parts = node.text
        for c in node.getchildren():
            parts = parts + c.text

        if node.tail:
            parts = parts + node.tail

            # filter removes possible Nones in texts and tails
            return ''.join(filter(None, parts))

    def parse(self, filename="group__all__tests.xml"):

        all_tests = objectify.parse("{}/{}".format(args.xmlroot, filename))
        groups = all_tests.xpath("//compounddef/innergroup")

        for g in groups:
            #print("\n=> Parsing {}...\n".format(g))
            refid = str(g.xpath('@refid')[0])

            identifier = refid.replace("group__", "")
            identifier = identifier.replace("__tests", "")
            identifier = identifier.replace("__", ".")

            group_tests = objectify.parse("{}/{}.xml".format(args.xmlroot, str(refid)))
            title = group_tests.xpath("//compounddef/title")[0]
            testcases = group_tests.xpath("//compounddef/sectiondef/memberdef")
            from lxml.etree import tostring
            for case in testcases:
                refid = case.xpath("@id")[0]

                # Strong FIXME
                if str(case.name).startswith("test_"):
                    brief = ""
                    brief_el = case.xpath("briefdescription/para")
                    if brief_el:
                        b = brief_el[0]
                        brief = b.text
                        if isinstance(b, objectify.ObjectifiedElement) and b.getchildren():
                            for i in b.getchildren():
                                brief = brief + i.text

                            if b.tail:
                                brief = brief + b.tail

                    #print(brief)
                    n = str(case.name).replace("test_", identifier + ".")
                    tc = TestCase(n, refid=str(refid))
                    tc.group = str(title)
                    tc.brief = brief
                    self.add(tc)

class Implementation:
    def __init__(self, name, refid=None, anchor=None):
        self.name = name
        self.refid = refid
        self.anchor = anchor

class Requirement:
    def __init__(self, name, refid=None, anchor=None):
        self.name = name
        self.refid = refid
        self.anchor = anchor
        self.tests = []
        self.implementations = []
        self.title = None
        self.text = None

    def add_test(self, test):
        self.tests.append(test)

    def add_implementation(self, implementation):
        self.implementations.append(implementation)


    def dump(self):
        print("Name={} ({})".format(self.name, self.refid))
        print("  Title: {}".format(self.title))
        print("  Text: {}".format(self.text))
        print("  Tests:")
        for t in self.tests:
            print("   - {}".format(t.name))
        print("  Implementations:")
        for i in self.implementations:
            print("   - {}".format(i.name))

        print("\n")

class Requirements:
    def __init__(self):
        self.requirements = []

    def add(self, req):
        self.requirements.append(req)

    def get_or_create(self, name):
        for r in self.requirements:
            if r.name == name:
                return r
        r = Requirement(name=name)
        self.add(r)
        return r


    def tests(self):
        tests = set()
        for r in self.requirements:
            tests.update(r.tests)
        return tests

    def verifies(self, req):
        return self.get_or_create(req).tests

    def satisfies(self, req):
        return self.get_or_create(req).implementations

    def requirement(self, test):
        reqs = set()
        for r in self.requirements:
            for t in r.tests:
                if t.name == test:
                    reqs.add(r.name)
        return reqs

    def dump(self):
        for r in self.requirements:
            r.dump()


    def parse(self, filename="Requirements.xml"):

        req_file = objectify.parse("{}/{}".format(args.xmlroot, filename))
        req_list = req_file.xpath("//sect2/@id")

        for req in req_list:
            id = str(req).replace("Requirements_1", "")

            r = self.get_or_create(id)
            r.name = id
            r.refid = str(req)

            req_details = req_file.xpath("//sect2[@id='{}']".format(r.refid))[0]
            r.title = str(req_details.xpath("title")[0])
            r.text = str(req_details.xpath("para")[0])

class TestReport:

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.testcases = []

    def parse(self, file):
        junit_xml = JUnitXml.fromfile(file)
        for suite in junit_xml:
            for testcase in suite:
                self.testcases.append(testcase)

class RTM():
    def __init__(self, xml_root, xls_file, report_file=None):
        self.rtm = {}
        self.rtm_req = []
        self.req_id_list = []
        self.xml_root = xml_root
        self.xls_file = xls_file
        self.report_file = report_file

        self.requirements = None
        self.suite = None



    def find_file(self, group_dict, struct_dict, refid):
        for k, refs in group_dict.items():
            if refid in refs:
                return k
        for k, refs in struct_dict.items():
            if refid in refs:
                return k

    def parse_xml(self):
        verify_list = objectify.parse("{}/verify.xml".format(self.xml_root))
        varlist = verify_list.xpath("//variablelist/node()")

        for ve in varlist:
            refs = ve.xpath("term/ref")
            for ref in refs:
                refid_raw = str(ref.xpath("@refid")[0])
                rr = refid_raw.split("_")
                rr.pop(len(rr)-1)
                l = rr[-1]
                rr[-1] = "1%s" %l
                refid="_".join(rr)
                #print(refid)

                check = self.suite.get_by_id(refid)
                if check:
                    #print("found")
                    tc = check
                else:
                    tc = self.suite.get_or_create(str(ref))
                    tc.refid = refid

            reqs = ve.xpath("para/ref")
            for req in reqs:
                r = self.requirements.get_or_create(str(req))
                r.add_test(tc)

        satisfy_list = objectify.parse("{}/satisfy.xml".format(self.xml_root))
        varlist = satisfy_list.xpath("//variablelist/node()")
        for ve in varlist:
            refs = ve.xpath("term/ref")
            for ref in refs:
                impl = Implementation(
                    name=str(ref),
                    refid=str(ref.xpath("@refid")[0])
                )
            listitems = ve.xpath("para/ref")
            for item in listitems:
                requirement = str(item)
                r = self.requirements.get_or_create(requirement)
                r.add_implementation(impl)


    def write_xls(self):

        book = xlwt.Workbook(encoding="utf-8")
        sheet1 = book.add_sheet("RTM")
        testplan_sheet = book.add_sheet("Tests")
        sheet3 = book.add_sheet("Requirements")
        sheet1.row(0).height_mismatch = True
        sheet1.row(0).height = 256*4


        # Write Testplan

        testplan_sheet.write(0, 0, "ID", xlwt.easyxf("align: vert top; align: horz center; font: bold 1"))
        testplan_sheet.col(0).width = 2962 * 4
        testplan_sheet.write(0, 1, "Brief", xlwt.easyxf("align: vert top; align: horz center; font: bold 1"))
        testplan_sheet.col(1).width = 2962 * 4
        testplan_sheet.write(0, 2, "Details", xlwt.easyxf("align: vert top; align: horz center; font: bold 1"))
        testplan_sheet.col(2).width = 16000

        row = 1
        test_links = {}
        for tc in self.suite.testcases:
            testplan_sheet.write(row, 0, tc.name)
            if tc.brief != "":
                #print("dddd {}".format(tc.brief))
                testplan_sheet.write(row, 1, tc.brief)
            #testplan_sheet.write(row, 2, r.text, xlwt.easyxf('alignment: wrap True'))
            test_links[tc.name] = 'A{}'.format(row + 1)
            row = row + 1

        # Write Requirements sheet

        sheet3.write(0, 0, "ID", xlwt.easyxf("align: vert top; align: horz center; font: bold 1"))
        sheet3.write(0, 1, "Title", xlwt.easyxf("align: vert top; align: horz center; font: bold 1"))
        sheet3.col(1).width = 2962 * 4
        sheet3.write(0, 2, "text", xlwt.easyxf("align: vert top; align: horz center; font: bold 1"))
        sheet3.col(2).width = 16000

        row = 1
        links = {}
        for r in self.requirements.requirements:
            sheet3.write(row, 0, r.name)
            sheet3.write(row, 1, r.title)
            sheet3.write(row, 2, r.text, xlwt.easyxf('alignment: wrap True'))
            links[r.name] = 'A{}'.format(row + 1)
            row = row + 1


        row = 2
        req_cols = {}
        # Now go through all tests
        for t in self.suite.testcases:
            sheet1.write(row, 0,
                xlwt.Formula('HYPERLINK("#Tests!{}","{}")'.format(test_links[t.name], t.name)))
            # Get all requirements for a test
            for r in self.requirements.requirement(t.name):
                if r not in req_cols:
                    sheet1.write(0, len(req_cols) + 2,
                        xlwt.Formula('HYPERLINK("#Requirements!{}","{}")'.format(links[r], r)),
                        xlwt.easyxf("align: rotation 90"))

                    sheet1.col(len(req_cols) + 2).width = 256 * 4
                    req_cols[r] = len(req_cols) + 2

                sheet1.write(row, req_cols[r], "X", xlwt.easyxf("align: horz center"))

            row += 1

        for r in self.requirements.requirements:
            if r.name not in req_cols:
                sheet1.write(0, len(req_cols) + 2,
                    xlwt.Formula('HYPERLINK("#Requirements!{}","{}")'.format(links[r.name], r.name)),
                    xlwt.easyxf("align: rotation 90"))
                sheet1.col(len(req_cols) + 2).width = 256 * 4

                req_cols[r.name] = len(req_cols) + 2


        sheet1.col(0).width = 256 * 60
        sheet1.write(0, 0, "Requirements", xlwt.easyxf("align: vert top; align: horz center; font: bold 1"))
        sheet1.write(1, 0, "Test Cases", xlwt.easyxf("align: vert top; align: horz center; font: bold 1"))
        sheet1.write(0, 1, "Reqs Tested", xlwt.easyxf("align: vert top; align: horz center; align: wrap 1; font: bold 1"))
        sheet1.col(1).width = 256 * 8

        row1 = 2
        tests = self.suite.testcases
        for t in tests:
            if 1:
                cell= "COUNTA({}{}:{}{})".format(xlrd.colname(2), row1 + 1, xlrd.colname(1 + len(self.requirements.requirements)), row1 + 1)
                sheet1.write(row1, 1, xlwt.Formula(cell), xlwt.easyxf("align: horz center"))

            row1 = row1 + 1

        sheet1.write(1, 1, xlwt.Formula("SUM(B3:B{})".format(3+len(tests))), xlwt.easyxf("align: horz center"))

        col = 2
        for t in self.requirements.requirements:
            sheet1.write(1, col, xlwt.Formula("COUNTA({}3:{}{})".format(xlrd.colname(col),
                xlrd.colname(col), 3 + len(tests))),
                xlwt.easyxf("align: horz center"))
            col = col + 1

        results = {}
        if self.report_file:
            test_report = book.add_sheet("Test Report")
            test_report.write(0, 0, "Testcase", xlwt.easyxf("align: vert top; align: horz center; font: bold 1"))
            test_report.write(0, 1, "Class", xlwt.easyxf("align: vert top; align: horz center; font: bold 1"))
            test_report.write(0, 2, "Results", xlwt.easyxf("align: vert top; align: horz center; font: bold 1"))
            r = TestReport()
            r.parse(self.report_file)
            row = 1
            for t in r.testcases:
                test_report.write(row, 0, t.name)
                test_report.write(row, 1, t.classname)
                if t.result:
                    test_report.write(row, 2, t.result.type)
                else:
                    test_report.write(row, 2, "pass")

                results[t.name] = 'A{}'.format(row + 1)
                row = row + 1

        book.save(self.xls_file)



def parse_args():
    parser = argparse.ArgumentParser(
                description="Generate Requirement Traceability Matrix (RTM).")
    parser.add_argument('-x', '--xmlroot', default=None,
            help="Root directory of XML files generated by doxygen.")
    parser.add_argument('-r', '--rtm-file', default="rtm.xls",
            help="RTM file in Excel format")
    parser.add_argument('-g', '--group',
            help="filter by group")

    parser.add_argument('-d', '--dump', action="store_true",
    help="Dump requirements and dependencies")

    parser.add_argument("-j", "--junit-file", help="file with test results in junit format")
    return parser.parse_args()

def main():
    global args

    args = parse_args()
    if not args.xmlroot:
        sys.exit(1)
    if not args.rtm_file:
        sys.exit(1)

    rtm = RTM(xml_root=args.xmlroot, xls_file=args.rtm_file, report_file=args.junit_file)

    rtm.suite = TestSuite()
    rtm.suite.parse()
    if args.group:
        rtm.suite.dump(args.group)
        sys.exit(0)

    rtm.requirements = Requirements()
    rtm.requirements.parse()


    rtm.parse_xml()

    if args.dump:
        rtm.requirements.dump()
        sys.exit(0)


    rtm.write_xls()

if __name__ == "__main__":
    main()


