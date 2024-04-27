#!/usr/bin/env python3

import csv
import sys
import argparse
import json

HEADER = """
/**
@page Requirements
@tableofcontents

"""

FOOTER = """
*/
"""

def debug(msg):
    print("DEBUG: {}".format(msg))

def get_nodes(nodes, grouped=dict()):
    for n in nodes:
        print(n['TITLE'])
        if n['TYPE'] == 'SECTION':
            grouped = get_nodes(n['NODES'], grouped)
            continue
        rid = n['UID']
        name = n['TITLE']
        text= n['STATEMENT']
        group = n.get('COMPONENT', None)
        if not group:
            debug("No group for {}".format(rid))
            continue
        if not grouped.get(group, None):
            grouped[group] = []
        grouped[group].append({'rid': rid, 'req': text, 'name': name, 'type': n['TYPE']})

    return grouped

def parse_strictdoc_json(filename):
    parsed_docs = dict()
    with open(filename) as fp:
        data = json.load(fp)
        docs = data.get('DOCUMENTS')
        for d in docs:
            print(f"{d['TITLE']}\n--------------------------------\n")
            grouped = get_nodes(d['NODES'], grouped=dict())
            parsed_docs['{}'.format(d['TITLE'])] = grouped

    return parsed_docs

def write_dox(parsed_docs, output="requirements.dox"):
    with open(output, "w") as req:
        req.write(HEADER)
        d = 0
        for k,v in parsed_docs.items():
            d += 1
            req.write("\n@section {}_{} {}\n".format(k.replace(" ", "_"), d, k))
            for r in v.keys():
                comp = v[r]
                req.write("\n@subsection {}_{} {}\n\n".format(r.replace(" ", "_"), d, r))
                for c in comp:
                    req.write("@subsubsection {} {}: {}\n".format(c['rid'], c['rid'], c['name']))
                    req.write("@details {}\n\n".format(c['req']))
                    req.write("@param TYPE {}\n\n".format(c['type']))

        req.write(FOOTER)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Create doxygen requirements page')
    parser.add_argument('--csv', help='CSV file to parse')
    parser.add_argument('--json', help='JSON file to parse')
    parser.add_argument('--output', help='Output file to write to')

    args = parser.parse_args()

    if args.json:
        filename = args.json
        write_dox(parse_strictdoc_json(filename), args.output)





