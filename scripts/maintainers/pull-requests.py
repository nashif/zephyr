#!/usr/bin/env python3

# Copyright (c) 2019 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

import sys
import os
import time
import json
import datetime
from github import Github, GithubException
from collections import defaultdict
from Maintainers import Maintainers, MaintainersError, GitError

_logging = 1

def log(s):
    if _logging:
        print(s, file=sys.stdout)


def main():
    dry_run = 0

    m = Maintainers("MAINTAINERS_new.yml")
    token = os.environ.get('GITHUB_TOKEN', None)
    if not token:
        sys.exit('Github token not set in environment, please set the '
                 'GITHUB_TOKEN environment variable and retry.')

    gh = Github(token)
    gh_repo = gh.get_repo("zephyrproject-rtos/zephyr")

    if len(sys.argv) > 1:
        pulls = [gh_repo.get_pull(int(sys.argv[1]))]
    else:
        pulls = gh_repo.get_pulls(state="open", base="master")

    for pr in pulls:
        delta = datetime.datetime.now() - pr.created_at
        if delta.days > 2:
            continue
        if pr.assignee and not dry_run:
            continue
        if pr.draft:
            continue

        log("+++++++++++++++++++++++++")
        log(f"https://github.com/zephyrproject-rtos/zephyr/pull/{pr.number} : {pr.title}")

        labels = set()
        collab = set()
        area_counter = defaultdict(int)
        maint = defaultdict(int)

        num_files = 0
        all_areas = set()
        for f in pr.get_files():
            num_files += 1
            log(f"file: {f.filename}")
            areas = m.path2areas(f.filename)

            if areas:
                all_areas.update(areas)
                for a in areas:
                    area_counter[a.name] += 1
                    labels.update(a.labels)
                    collab.update(a.collaborators)
                    collab.update(a.maintainers)
                    for p in a.maintainers:
                        maint[p] += 1

        ac = dict(sorted(area_counter.items(), key=lambda item: item[1], reverse=True))
        print(f"Area matches: {ac}")
        print(f"labels: {labels}")
        print(f"collab: {collab}")

        sm = dict(sorted(maint.items(), key=lambda item: item[1], reverse=True))

        print(f"Submitted by: {pr.user.login}")
        print(f"candidate maintainers: {sm}")

        prop = 0
        if sm:
            maintainer = list(sm.keys())[0]

            if len(ac) > 1 and list(ac.values())[0] == list(ac.values())[1]:
                log("++ Platform/Drivers takes precedence over subsystem...")
                for aa in ac:
                    if 'Documentation' in aa:
                        log("++ With multiple areas of same weight including docs, take something else other than Documentation as the maintainer")
                        for a in all_areas:
                            if a.name == aa and a.maintainers[0] == maintainer:
                                maintainer = list(sm.keys())[1]
                    elif 'Platform' in aa:
                        log(f"Set maintainer of area {aa}")
                        for a in all_areas:
                            if a.name == aa:
                                if a.maintainers:
                                    maintainer = a.maintainers[0]
                                    break


            # if the submitter is the same as the maintainer, check if we have
            # multiple maintainers
            if pr.user.login == maintainer:
                log("Submitter is same as Assignee, trying to find another assignee...")
                aff = list(ac.keys())[0]
                for a in all_areas:
                    if a.name == aff:
                        if len(a.maintainers) > 1:
                            maintainer = a.maintainers[1]
                        else:
                            log(f"This area has only one maintainer, keeping assignee as {maintainer}")

            prop = (maint[maintainer] / num_files) * 100
            if prop < 20:
                maintainer = "None"
        else:
            maintainer = "None"
        print(f"Picked maintainer: {maintainer} ({prop:.2f}% ownership)")
        print("+++++++++++++++++++++++++")
        print()

        if maintainer == "None":
            continue
            print("No maintainer found...")
            input("Press Enter to continue...")


        # Set labels
        if labels:
            current_labels = pr.get_labels()
            for l in labels:
                log(f"adding label {l}...")
                if not dry_run:
                    pr.add_to_labels(l)

        if collab:
            reviewers = []
            existing_reviewers = set()

            revs = pr.get_reviews()
            for review in revs:
                existing_reviewers.add(review.user)

            rl = pr.get_review_requests()
            page = 0
            for r in rl:
                existing_reviewers |= set(r.get_page(page))
                page += 1

            for c in collab:
                u = gh.get_user(c)
                if pr.user != u and gh_repo.has_in_collaborators(u):
                    if u not in existing_reviewers:
                        reviewers.append(c)

            if reviewers:
                try:
                    print(f"adding reviewers {reviewers}...")
                    if not dry_run:
                        pr.create_review_request(reviewers=reviewers)
                except:
                    log("cant add reviewer")
                    pass

        ms = []
        # assignees
        if maintainer != 'None':
            try:
                u = gh.get_user(maintainer)
                ms.append(u)
            except:
                log(f"Unknown user")

            for mm in ms:
                print(f"Adding assignee {mm}...")
                if not dry_run:
                    pr.add_to_assignees(mm)

        time.sleep(1)


if __name__ == "__main__":
    main()
