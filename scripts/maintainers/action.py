#!/usr/bin/env python3

# Copyright (c) 2019 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

import sys
import os
import json
from Maintainers import Maintainers, MaintainersError, GitError
from github import Github, GithubException

_logging = 1

def log(s):
    if _logging:
        print(s, file=sys.stdout)

def main():
    # Retrieve main env vars
    action = os.environ.get('GITHUB_ACTION', None)
    workflow = os.environ.get('GITHUB_WORKFLOW', None)
    org_repo = os.environ.get('GITHUB_REPOSITORY', None) or "nashif/zephyr"

    log(f'Running action {action} from workflow {workflow} in {org_repo}')

    evt_name = os.environ.get('GITHUB_EVENT_NAME', None)
    evt_path = os.environ.get('GITHUB_EVENT_PATH', None)
    workspace = os.environ.get('GITHUB_WORKSPACE', None)

    token = os.environ.get('GITHUB_TOKEN', None)
    if not token:
        sys.exit('Github token not set in environment, please set the '
                 'GITHUB_TOKEN environment variable and retry.')

    if not ("pull_request" in evt_name):
        sys.exit(f'Invalid event {evt_name}')

    if evt_path and os.path.exists(evt_path):
        with open(evt_path, 'r') as f:
            evt = json.load(f)
    else:
        evt = {"pull_request": {'number': 5}}

    pr = evt['pull_request']

    gh = Github(token)
    tk_usr = gh.get_user()
    gh_repo = gh.get_repo(org_repo)
    gh_pr = gh_repo.get_pull(int(pr['number']))

    m = Maintainers("MAINTAINERS_new.yml")
    labels = []
    collab = []
    maintainers = []
    for f in gh_pr.get_files():
        log(f"file: {f.filename}")
        areas = m.path2areas(f.filename)

        if areas:
            for a in areas:
                labels += a.labels
                collab += a.collaborators
                collab += a.maintainers
                maintainers += a.maintainers

    print(f"labels: {labels}")
    print(f"collab: {collab}")
    print(f"maintainers: {maintainers}")

    # Set labels
    if labels:
        for l in labels:
            gh_pr.add_to_labels(l)

    if collab:
        reviewers = []
        for c in collab:
            u = gh.get_user(c)
            if gh_pr.user != u and gh_repo.has_in_collaborators(u):
                reviewers.append(c)

        if reviewers:
            try:
                gh_pr.create_review_request(reviewers=reviewers)
            except:
                log("cant add reviewer")
                pass

    ms = []
    if maintainers:
        for m in maintainers:
            try:
                u = gh.get_user(m)
                ms.append(u)
            except:
                log(f"Unknown user {u}")
                pass
        for m in ms:
            gh_pr.add_to_assignees(m)



if __name__ == "__main__":
    main()


