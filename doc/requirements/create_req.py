#!/usr/bin/env python3

import csv
import sys
import argparse
import json

filename = sys.argv[1]
print(filename)

HEADER = """
/**
@page Requirements
@tableofcontents
@section Requirements

"""

FOOTER = """
*/
"""

def debug(msg):
    print("DEBUG: {}".format(msg))


def parse_strictdoc_json(filename):
    grouped = dict()
    with open(filename) as fp:
        data = json.load(fp)
        docs = data.get('DOCUMENTS')
        for d in docs:
            for n in d['NODES']:
                if n['TYPE'] == 'SECTION':
                    continue
                rid = n['UID']
                name = n['TITLE']
                text= n['STATEMENT']
                group = n.get('COMPONENT', None)
                if not group:
                    debug("No group for {}".format(rid))
                    continue
                rtype = n['TYPE']
                if not grouped.get(group, None):
                    grouped[group] = []
                grouped[group].append({'rid': rid, 'req': text, 'name': name })
    return grouped

def parse_dng_csv(filename):

    grouped = dict()
    with open(filename) as fp:
            cr = csv.DictReader(fp)
            for row in cr:
                group = row.get('Component Allocation')
                rid = row.get('ZEP ID')
                text = row.get('Primary Text').rstrip()
                name = row.get('Name').rstrip()
                rtype = row.get('Requirement Type')
                level = row.get('Hierarchy', None)
                if not grouped.get(group, None):
                    grouped[group] = []

                grouped[group].append({'rid': rid, 'req': text, 'name': name })
    return grouped

def write_dox(grouped, output="requirements.dox"):
    with open(output, "w") as req:
        req.write(HEADER)

        for r in grouped.keys():
            comp = grouped[r]
            req.write("\n@section {}\n\n".format(r))
            for c in comp:
                req.write("@subsection {} {}: {}\n{}\n\n\n".format(c['rid'], c['rid'], c['name'], c['req']))

        req.write(FOOTER)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Create doxygen requirements page')
    parser.add_argument('--csv', help='CSV file to parse')
    parser.add_argument('--json', help='JSON file to parse')
    parser.add_argument('--output', help='Output file to write to')

    args = parser.parse_args()

    if args.csv:
        filename = args.csv
        write_dox(parse_dng_csv(filename))
    elif args.json:
        filename = args.json
        write_dox(parse_strictdoc_json(filename), args.output)





