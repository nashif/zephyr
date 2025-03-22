#!/usr/bin/env python3


import json
import sys
from collections import defaultdict

def load_database(database_path):
    with open(database_path, 'r') as file:
        return json.load(file)

def find_best_coverage(database, changed_files):
    coverage = defaultdict(lambda: defaultdict(int))

    for file in changed_files:
        if file in database:
            for entry in database[file]:
                testsuite_id = entry["testsuite_id"]
                platform = entry["platform"]
                coverage[testsuite_id][platform] += 1

    best_coverage = []
    for testsuite_id, platforms in coverage.items():
        for platform, count in platforms.items():
            best_coverage.append((testsuite_id, platform, count))

    best_coverage.sort(key=lambda x: x[2], reverse=True)
    return best_coverage

def main(database_path, changed_files):
    database = load_database(database_path)
    best_coverage = find_best_coverage(database, changed_files)

    if best_coverage:
        print("Best coverage testsuites and platforms:")
        for testsuite_id, platform, count in best_coverage:
            print(f"Testsuite: {testsuite_id}, Platform: {platform}, Coverage: {count}")
    else:
        print("No matching testsuites found for the provided files.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python find_best_coverage.py <path_to_database> <file1> <file2> ...")
        sys.exit(1)

    database_path = sys.argv[1]
    changed_files = sys.argv[2:]
    main(database_path, changed_files)