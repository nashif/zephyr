#!/usr/bin/env python3

import csv
import sys

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

grouped = dict()

with open(filename) as fp:
        cr = csv.DictReader(fp)
        for row in cr:
            #print(row)
            group = row['group']
            if row['normative'] == "FALSE":
                continue
            if not grouped.get(group, None):
                grouped[group] = []

            grouped[group].append({'id': "{}".format(row['uid']), 'req': row['text'], 'name': row['header']})



with open("requirements.dox", "w") as req:
    req.write(HEADER)

    for r in grouped.keys():
        comp = grouped[r]
        req.write("\n@section {}\n\n".format(r))
        for c in comp:
            req.write("@subsection {} {}: {}\n{}\n\n\n".format(c['id'], c['id'], c['name'], c['req']))

    req.write(FOOTER)




