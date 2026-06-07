#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

"""
PR Test Advisor: Analyzes a Zephyr pull request and recommends tests.

Fetches a GitHub PR, maps changed files to twister test areas and pytest
suites using the existing tags.yaml / MAINTAINERS.yml infrastructure, then
optionally uses an LLM agent to rate change complexity and provide a
natural-language test recommendation rationale.

Supported LLM providers (selected via --provider or ADVISOR_PROVIDER):

  openai     - OpenAI models (gpt-4o, o3-mini, ...) and any OpenAI-compatible
               endpoint (Azure, Ollama, llama.cpp, vLLM, ...)
               Requires: pip install openai
               Env:      OPENAI_API_KEY, OPENAI_BASE_URL (optional)

  anthropic  - Anthropic Claude models (claude-opus-4, claude-sonnet-4-5, ...)
               Requires: pip install anthropic
               Env:      ANTHROPIC_API_KEY

  litellm    - Universal proxy; works with 100+ providers via a single interface.
               Pass any litellm model string, e.g.:
                 anthropic/claude-3-5-sonnet-20241022
                 gemini/gemini-1.5-pro
                 ollama/llama3
               Requires: pip install litellm
               Env:      provider-specific (OPENAI_API_KEY, ANTHROPIC_API_KEY, ...)

  auto       - (default) Infer provider from model name and available packages.
               claude-* -> anthropic; gpt-*/o1-*/o3-*/o4-* -> openai;
               anything containing '/' -> litellm; fallback: first available.

Usage (heuristic only, no API key needed):
    ./scripts/ci/pr_test_advisor.py --pr 12345

Usage (OpenAI):
    OPENAI_API_KEY=sk-... ./scripts/ci/pr_test_advisor.py --pr 12345

Usage (Claude):
    ANTHROPIC_API_KEY=sk-ant-... \\
        ./scripts/ci/pr_test_advisor.py --pr 12345 \\
        --provider anthropic --model claude-opus-4

Usage (Ollama / local model via litellm):
    ./scripts/ci/pr_test_advisor.py --pr 12345 \\
        --provider litellm --model ollama/qwen2.5-coder

Usage (OpenAI-compatible custom endpoint, e.g. vLLM):
    OPENAI_API_KEY=none OPENAI_BASE_URL=http://localhost:8000/v1 \\
        ./scripts/ci/pr_test_advisor.py --pr 12345 --model my-model

Environment variables:
    GITHUB_TOKEN      - GitHub API token (rate limits / private repos)
    ADVISOR_PROVIDER  - Default provider (overridden by --provider)
    OPENAI_API_KEY    - OpenAI / compatible key
    OPENAI_BASE_URL   - Optional base URL for OpenAI-compatible endpoints
    ANTHROPIC_API_KEY - Anthropic API key

Requirements:
    pip install PyGithub pyyaml
    pip install openai      # for openai provider
    pip install anthropic   # for anthropic provider
    pip install litellm     # for litellm provider

Output:
    Prints a structured report to stdout, optionally saves JSON to --output.
"""

import argparse
import fnmatch
import json
import logging
import os
import re
import sys
from pathlib import Path

import yaml

try:
    from yaml import CSafeLoader as SafeLoader
except ImportError:
    from yaml import SafeLoader

try:
    from github import Github
    HAS_PYGITHUB = True
except ImportError:
    HAS_PYGITHUB = False

try:
    import openai
    HAS_OPENAI = True
except ImportError:
    HAS_OPENAI = False

try:
    import anthropic
    HAS_ANTHROPIC = True
except ImportError:
    HAS_ANTHROPIC = False

try:
    import litellm
    HAS_LITELLM = True
except ImportError:
    HAS_LITELLM = False

logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.WARNING)
log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

ZEPHYR_BASE = Path(os.environ.get('ZEPHYR_BASE', Path(__file__).resolve().parents[2]))

TAGS_YAML = ZEPHYR_BASE / 'scripts' / 'ci' / 'tags.yaml'
MAINTAINERS_YAML = ZEPHYR_BASE / 'MAINTAINERS.yml'
TWISTER_IGNORE = ZEPHYR_BASE / 'scripts' / 'ci' / 'twister_ignore.txt'

GITHUB_ORG = 'zephyrproject-rtos'
GITHUB_REPO = 'zephyr'

# Complexity levels (ascending)
COMPLEXITY_LEVELS = ['trivial', 'low', 'medium', 'high', 'critical']

# ---------------------------------------------------------------------------
# Mapping: file glob patterns -> pytest test directories (relative to repo)
# ---------------------------------------------------------------------------

PYTEST_MAPPINGS = [
    {
        'patterns': [
            'scripts/pylib/twister/**',
            'scripts/twister',
            'scripts/tests/twister/**',
        ],
        'pytest_paths': ['scripts/tests/twister', 'scripts/tests/twister_blackbox'],
        'description': 'Twister unit and blackbox tests',
    },
    {
        'patterns': [
            'cmake/**',
            'CMakeLists.txt',
            'scripts/cmake/**',
            'share/sysbuild/**',
            'share/zephyr-package/**',
        ],
        'pytest_paths': ['scripts/tests/build', 'scripts/tests/build_helpers'],
        'description': 'CMake / build system tests',
    },
    {
        'patterns': [
            'scripts/dts/**',
            'dts/**',
            'include/zephyr/dt-bindings/**',
        ],
        'pytest_paths': ['scripts/tests/build'],
        'description': 'Devicetree / DTS tests',
    },
    {
        'patterns': [
            'scripts/ci/**',
        ],
        'pytest_paths': ['scripts/tests/twister', 'scripts/tests/twister_blackbox'],
        'description': 'CI script tests',
    },
]

# ---------------------------------------------------------------------------
# Subsystem heuristic: directory prefix -> subsystem label
# ---------------------------------------------------------------------------

DIR_TO_SUBSYSTEM = {
    'arch/': 'Architecture',
    'boards/': 'Boards / BSP',
    'cmake/': 'Build system',
    'doc/': 'Documentation',
    'drivers/': 'Drivers',
    'dts/': 'Devicetree',
    'include/': 'Public API / Headers',
    'kernel/': 'Kernel',
    'lib/': 'Library',
    'modules/': 'Modules',
    'samples/': 'Samples',
    'scripts/': 'Scripts / Tooling',
    'soc/': 'SoC',
    'subsys/': 'Subsystems',
    'tests/': 'Tests',
    'west.yml': 'Manifest / West',
    'CMakeLists.txt': 'Build system',
    'Kconfig': 'Kconfig',
    'MAINTAINERS.yml': 'Maintenance',
}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _load_yaml_safe(path):
    with open(path) as fh:
        return yaml.load(fh, Loader=SafeLoader)


def _get_match_fn(globs, regexes):
    """Build a compiled regex matcher from glob patterns and regex strings."""
    if not (globs or regexes):
        return None
    regex = ''
    if globs:
        glob_regexes = []
        for g in globs:
            gr = g.replace('.', '\\.').replace('*', '[^/]*').replace('?', '[^/]')
            if not g.endswith('/'):
                gr += '$'
            glob_regexes.append(gr)
        regex += '^(?:{})'.format('|'.join(glob_regexes))
    if regexes:
        if regex:
            regex += '|'
        regex += '|'.join(regexes)
    return re.compile(regex).search


def _load_twister_ignore(path):
    """Return a list of non-comment, non-empty ignore patterns."""
    try:
        with open(path) as fh:
            lines = fh.read().splitlines()
        return [l for l in lines if l and not l.startswith('#')]
    except FileNotFoundError:
        return []


# ---------------------------------------------------------------------------
# Tag/area resolution (mirrors tags.yaml logic from test_plan.py)
# ---------------------------------------------------------------------------

class TagMatcher:
    """Matches file paths against tags.yaml entries."""

    def __init__(self, tags_yaml_path=TAGS_YAML):
        self._tags = {}
        if not os.path.exists(tags_yaml_path):
            return
        raw = _load_yaml_safe(tags_yaml_path)
        for name, cfg in raw.items():
            self._tags[name] = {
                'match': _get_match_fn(cfg.get('files'), cfg.get('files-regex')),
                'exclude': _get_match_fn(
                    cfg.get('files-exclude'), cfg.get('files-regex-exclude')
                ),
            }

    def matched_tags(self, files):
        """Return set of tag names that are touched by the given file list."""
        touched = set()
        for f in files:
            for name, t in self._tags.items():
                if t['match'] and t['match'](f):
                    if not (t['exclude'] and t['exclude'](f)):
                        touched.add(name)
        return touched

    def all_tags(self):
        return set(self._tags.keys())

    def excluded_tags(self, files):
        """Return tags that should be *excluded* (i.e., were not touched)."""
        return self.all_tags() - self.matched_tags(files)


# ---------------------------------------------------------------------------
# Maintainer / area resolution from MAINTAINERS.yml
# ---------------------------------------------------------------------------

class MaintainerResolver:
    """Maps file paths to Zephyr areas/subsystems via MAINTAINERS.yml."""

    def __init__(self, maintainers_yaml=MAINTAINERS_YAML):
        self._areas = {}
        if not os.path.exists(maintainers_yaml):
            return
        raw = _load_yaml_safe(maintainers_yaml)
        for area_name, cfg in raw.items():
            if not isinstance(cfg, dict):
                continue
            self._areas[area_name] = {
                'labels': cfg.get('labels', []),
                'match': _get_match_fn(cfg.get('files'), cfg.get('files-regex')),
                'exclude': _get_match_fn(
                    cfg.get('files-exclude'), cfg.get('files-regex-exclude')
                ),
            }

    def find_areas(self, files):
        """Return list of (area_name, labels) tuples for files."""
        matched = {}
        for f in files:
            for area, info in self._areas.items():
                if info['match'] and info['match'](f):
                    if not (info['exclude'] and info['exclude'](f)):
                        if area not in matched:
                            matched[area] = info['labels']
        return list(matched.items())


# ---------------------------------------------------------------------------
# Heuristic test-path finder (mirrors find_tests in test_plan.py)
# ---------------------------------------------------------------------------

def find_test_paths_for_files(files):
    """Return set of test/sample directories that should be run for files."""
    test_yaml_names = {'sample.yaml', 'testcase.yaml', 'tests.yaml'}
    tests = set()
    for f in files:
        if f.endswith('.rst'):
            continue
        d = Path(f).parent
        found = False
        while not found and d != d.parent:
            if any((ZEPHYR_BASE / d / td).exists() for td in test_yaml_names):
                tests.add(str(d))
                found = True
                break
            d = d.parent
    return tests


# ---------------------------------------------------------------------------
# Pytest test mapping
# ---------------------------------------------------------------------------

def find_pytest_tests(files):
    """Return list of dicts with pytest_paths and description for changed files."""
    results = []
    seen_paths = set()
    for mapping in PYTEST_MAPPINGS:
        for f in files:
            matched = any(fnmatch.fnmatch(f, p) for p in mapping['patterns'])
            if matched:
                new_paths = [
                    p for p in mapping['pytest_paths']
                    if p not in seen_paths and (ZEPHYR_BASE / p).exists()
                ]
                if new_paths:
                    seen_paths.update(new_paths)
                    results.append({
                        'paths': new_paths,
                        'description': mapping['description'],
                    })
                break
    return results


# ---------------------------------------------------------------------------
# Heuristic complexity estimation
# ---------------------------------------------------------------------------

def estimate_complexity(files, diff_stats):
    """
    Heuristic complexity rating without LLM.

    Returns a dict with 'level' (str) and 'reasons' (list of str).
    """
    reasons = []
    score = 0

    total_changes = diff_stats.get('additions', 0) + diff_stats.get('deletions', 0)
    num_files = len(files)

    # Score based on diff size
    if total_changes > 2000:
        score += 4
        reasons.append(f'Large diff: {total_changes} lines changed')
    elif total_changes > 500:
        score += 2
        reasons.append(f'Medium diff: {total_changes} lines changed')
    elif total_changes > 100:
        score += 1
        reasons.append(f'Moderate diff: {total_changes} lines changed')

    # Score based on number of files
    if num_files > 50:
        score += 3
        reasons.append(f'Many files: {num_files} files changed')
    elif num_files > 20:
        score += 2
        reasons.append(f'Multiple files: {num_files} files changed')
    elif num_files > 5:
        score += 1

    # Score based on subsystem breadth
    subsystems = set()
    for f in files:
        for prefix, label in DIR_TO_SUBSYSTEM.items():
            if f.startswith(prefix) or f == prefix:
                subsystems.add(label)
    if len(subsystems) > 4:
        score += 2
        reasons.append(f'Cross-subsystem: touches {len(subsystems)} subsystems')
    elif len(subsystems) > 2:
        score += 1
        reasons.append(f'Multi-subsystem: touches {len(subsystems)} subsystems')

    # Core risk areas
    risky_patterns = [
        ('kernel/', 'Kernel code changed'),
        ('arch/', 'Architecture code changed'),
        ('include/zephyr/', 'Public API header changed'),
        ('cmake/', 'Build system changed'),
        ('Kconfig', 'Kconfig logic changed'),
        ('scripts/pylib/twister/', 'Twister core changed'),
    ]
    for pattern, reason in risky_patterns:
        if any(f.startswith(pattern) or f == pattern for f in files):
            score += 1
            reasons.append(reason)

    # Map score to level
    if score == 0:
        level = 'trivial'
    elif score <= 2:
        level = 'low'
    elif score <= 4:
        level = 'medium'
    elif score <= 7:
        level = 'high'
    else:
        level = 'critical'

    if not reasons:
        reasons.append('Small, focused change')

    return {'level': level, 'score': score, 'reasons': reasons}


# ---------------------------------------------------------------------------
# LLM provider abstraction
# ---------------------------------------------------------------------------

# Model-name prefixes used for auto-detection
_ANTHROPIC_PREFIXES = ('claude-',)
_OPENAI_PREFIXES = ('gpt-', 'o1-', 'o3-', 'o4-', 'text-davinci')


def _resolve_provider(model, provider_hint):
    """
    Determine which LLM provider to use.

    Resolution order:
      1. Explicit --provider / ADVISOR_PROVIDER value
      2. Model name prefix heuristic
      3. litellm if installed (handles model strings with '/')
      4. First available library

    Returns one of 'openai', 'anthropic', 'litellm', or None.
    """
    hint = provider_hint or os.environ.get('ADVISOR_PROVIDER', 'auto')

    if hint != 'auto':
        if hint == 'openai' and not HAS_OPENAI:
            log.warning('Provider "openai" requested but openai package is not installed')
            return None
        if hint == 'anthropic' and not HAS_ANTHROPIC:
            log.warning('Provider "anthropic" requested but anthropic package is not installed')
            return None
        if hint == 'litellm' and not HAS_LITELLM:
            log.warning('Provider "litellm" requested but litellm package is not installed')
            return None
        return hint

    # Auto-detect from model name
    if any(model.startswith(p) for p in _ANTHROPIC_PREFIXES):
        if HAS_ANTHROPIC:
            return 'anthropic'
        if HAS_LITELLM:
            return 'litellm'

    if any(model.startswith(p) for p in _OPENAI_PREFIXES):
        if HAS_OPENAI:
            return 'openai'
        if HAS_LITELLM:
            return 'litellm'

    if '/' in model:
        if HAS_LITELLM:
            return 'litellm'

    # Fallback: first available with a configured key
    if HAS_OPENAI and os.environ.get('OPENAI_API_KEY'):
        return 'openai'
    if HAS_ANTHROPIC and os.environ.get('ANTHROPIC_API_KEY'):
        return 'anthropic'
    if HAS_LITELLM:
        return 'litellm'

    return None


# ---------------------------------------------------------------------------
# LLM agent analysis
# ---------------------------------------------------------------------------

LLM_SYSTEM_PROMPT = """\
You are an expert Zephyr RTOS code reviewer and QA engineer. Your task is to
analyze a GitHub pull request to the Zephyr project and produce a structured
JSON test recommendation report.

You have access to the following tools which you MUST call to gather information
before producing your final report:

1. analyze_changed_files(files: list[str]) -> dict
   Returns subsystems, risk areas, and suggested test tags for a file list.

2. get_pr_diff_summary(file_path: str) -> str
   Returns a brief summary of what changed in a specific file (from the PR diff).

3. finalize_report(analysis: dict) -> dict
   Accepts your complete analysis and returns the validated final report.

Produce a final_report JSON object with exactly these fields:
{
  "summary": "<2-3 sentence description of what this PR does>",
  "complexity": "<trivial|low|medium|high|critical>",
  "complexity_rationale": "<1-2 sentences explaining the rating>",
  "affected_subsystems": ["<subsystem1>", ...],
  "risk_areas": ["<area of concern>", ...],
  "test_focus": ["<what aspects should be tested>", ...],
  "additional_platforms": ["<platform slug>", ...],
  "notes": "<any special testing notes or caveats>"
}

Guidelines for complexity:
- trivial: docs-only, typo fix, comment update, CI-config only
- low: single driver/subsystem, narrow scope, < 100 lines
- medium: single subsystem but non-trivial, 100-500 lines, or API change
- high: multiple subsystems, kernel/arch changes, public API break, > 500 lines
- critical: cross-cutting refactor, ABI break, security fix, build system overhaul

Only output valid JSON, no markdown fences, no prose outside the JSON object.
"""

LLM_USER_PROMPT_TMPL = """\
Pull Request #{pr_number}: {pr_title}
URL: {pr_url}
Author: {pr_author}
Labels: {pr_labels}

PR Description:
{pr_body}

Changed files ({num_files} files, +{additions}/-{deletions}):
{file_list}

Subsystems detected by heuristic analysis: {heuristic_subsystems}
Twister tags touched: {twister_tags}
Heuristic complexity estimate: {heuristic_complexity}

Please analyze this PR and return the final_report JSON.
"""


def _build_user_content(pr_data, heuristic_result):
    """Build the user message string from PR data and heuristic results."""
    files = pr_data['files']
    file_list = '\n'.join(f'  {f}' for f in sorted(files))
    return LLM_USER_PROMPT_TMPL.format(
        pr_number=pr_data['number'],
        pr_title=pr_data['title'],
        pr_url=pr_data['url'],
        pr_author=pr_data['author'],
        pr_labels=', '.join(pr_data.get('labels', [])) or '(none)',
        pr_body=(pr_data.get('body') or '(no description)')[:3000],
        num_files=len(files),
        additions=pr_data['diff_stats'].get('additions', '?'),
        deletions=pr_data['diff_stats'].get('deletions', '?'),
        file_list=file_list,
        heuristic_subsystems=', '.join(heuristic_result['subsystems']) or '(unknown)',
        twister_tags=', '.join(sorted(heuristic_result['touched_tags'])) or '(none)',
        heuristic_complexity=heuristic_result['complexity']['level'],
    )


def _call_openai(model, system, user):
    """Call an OpenAI or OpenAI-compatible endpoint. Returns raw text."""
    api_key = os.environ.get('OPENAI_API_KEY')
    if not api_key:
        raise RuntimeError('OPENAI_API_KEY is not set')
    base_url = os.environ.get('OPENAI_BASE_URL')
    kwargs = {'api_key': api_key}
    if base_url:
        kwargs['base_url'] = base_url
    client = openai.OpenAI(**kwargs)
    response = client.chat.completions.create(
        model=model,
        messages=[
            {'role': 'system', 'content': system},
            {'role': 'user', 'content': user},
        ],
        response_format={'type': 'json_object'},
        temperature=0.2,
        max_tokens=1024,
    )
    return response.choices[0].message.content


def _call_anthropic(model, system, user):
    """Call an Anthropic Claude model. Returns raw text."""
    api_key = os.environ.get('ANTHROPIC_API_KEY')
    if not api_key:
        raise RuntimeError('ANTHROPIC_API_KEY is not set')
    client = anthropic.Anthropic(api_key=api_key)
    response = client.messages.create(
        model=model,
        max_tokens=1024,
        system=system,
        messages=[{'role': 'user', 'content': user}],
        temperature=0.2,
    )
    return response.content[0].text


def _call_litellm(model, system, user):
    """Call any model supported by litellm. Returns raw text."""
    response = litellm.completion(
        model=model,
        messages=[
            {'role': 'system', 'content': system},
            {'role': 'user', 'content': user},
        ],
        temperature=0.2,
        max_tokens=1024,
    )
    return response.choices[0].message.content


def llm_analyze(pr_data, heuristic_result, provider=None):
    """
    Call an LLM to analyze the PR using the selected provider.

    Dispatches to the appropriate backend based on provider auto-detection
    or explicit --provider / ADVISOR_PROVIDER selection.

    Returns the parsed JSON dict, or None on failure.
    """
    model = pr_data.get('llm_model', 'gpt-4o-mini')
    resolved = _resolve_provider(model, provider)

    if resolved is None:
        return None

    user_content = _build_user_content(pr_data, heuristic_result)

    _BACKENDS = {
        'openai': _call_openai,
        'anthropic': _call_anthropic,
        'litellm': _call_litellm,
    }

    backend_fn = _BACKENDS.get(resolved)
    if backend_fn is None:
        log.warning('Unknown provider: %s', resolved)
        return None

    log.info('Using LLM provider: %s  model: %s', resolved, model)
    pr_data['llm_provider'] = resolved
    try:
        raw = backend_fn(model, LLM_SYSTEM_PROMPT, user_content)
        return json.loads(raw)
    except Exception as exc:
        log.warning('LLM analysis failed (%s): %s', resolved, exc)
        return None


# ---------------------------------------------------------------------------
# GitHub PR fetcher
# ---------------------------------------------------------------------------

def fetch_pr(pr_number, github_token=None, org=GITHUB_ORG, repo=GITHUB_REPO):
    """
    Fetch PR metadata and file list from GitHub.

    Returns a dict with title, body, files, diff_stats, labels, etc.
    Falls back to a minimal stub if PyGithub is not available.
    """
    if not HAS_PYGITHUB:
        raise SystemExit(
            'PyGithub is required: pip install PyGithub'
        )

    token = github_token or os.environ.get('GITHUB_TOKEN')
    gh = Github(token) if token else Github()

    try:
        gh_repo = gh.get_repo(f'{org}/{repo}')
        pr = gh_repo.get_pull(pr_number)
    except Exception as exc:
        raise SystemExit(f'Failed to fetch PR #{pr_number}: {exc}') from exc

    files = [f.filename for f in pr.get_files()]
    diff_stats = {
        'additions': pr.additions,
        'deletions': pr.deletions,
        'changed_files': pr.changed_files,
    }

    return {
        'number': pr.number,
        'title': pr.title,
        'url': pr.html_url,
        'author': pr.user.login,
        'body': pr.body or '',
        'state': pr.state,
        'labels': [lbl.name for lbl in pr.labels],
        'files': files,
        'diff_stats': diff_stats,
        'base_branch': pr.base.ref,
        'draft': pr.draft,
    }


# ---------------------------------------------------------------------------
# Core analysis
# ---------------------------------------------------------------------------

def analyze_pr(pr_data, llm_model='gpt-4o-mini', llm_provider=None):
    """
    Run heuristic + optional LLM analysis on fetched PR data.

    Returns a comprehensive analysis dict.
    """
    files = pr_data['files']

    # --- Heuristic subsystem detection ---
    subsystems = set()
    for f in files:
        for prefix, label in DIR_TO_SUBSYSTEM.items():
            if f.startswith(prefix) or f == prefix:
                subsystems.add(label)

    # --- Tag analysis ---
    tag_matcher = TagMatcher()
    touched_tags = tag_matcher.matched_tags(files)
    excluded_tags = tag_matcher.excluded_tags(files)

    # --- Maintainer area detection ---
    maintainer_resolver = MaintainerResolver()
    areas = maintainer_resolver.find_areas(files)
    area_labels = sorted({lbl for _, lbls in areas for lbl in lbls})

    # --- Complexity heuristic ---
    complexity = estimate_complexity(files, pr_data['diff_stats'])

    heuristic_result = {
        'subsystems': sorted(subsystems),
        'touched_tags': touched_tags,
        'excluded_tags': excluded_tags,
        'area_labels': area_labels,
        'complexity': complexity,
    }

    # --- Test path detection ---
    test_paths = find_test_paths_for_files(files)
    pytest_tests = find_pytest_tests(files)

    # --- Ignore-file check ---
    ignore_patterns = _load_twister_ignore(TWISTER_IGNORE)
    unresolved = []
    for f in files:
        if not any(fnmatch.fnmatch(f, p) for p in ignore_patterns):
            if not any(f.startswith(tp) for tp in test_paths):
                unresolved.append(f)

    # --- LLM analysis (optional) ---
    pr_data['llm_model'] = llm_model
    llm_result = llm_analyze(pr_data, heuristic_result, provider=llm_provider)

    return {
        'pr': pr_data,
        'heuristic': heuristic_result,
        'test_paths': sorted(test_paths),
        'pytest_tests': pytest_tests,
        'unresolved_files': unresolved,
        'llm': llm_result,
    }


# ---------------------------------------------------------------------------
# Report generation
# ---------------------------------------------------------------------------

def _twister_cmd(tag_options, test_path_options, full_run, platforms=None):
    """Build a representative twister command string."""
    parts = ['./scripts/twister']

    if full_run:
        parts.append('--integration')
        if tag_options:
            for tag in sorted(tag_options):
                parts += ['-e', tag]
    else:
        for path in sorted(test_path_options):
            parts += ['-T', path]
        for tag in sorted(tag_options):
            parts += ['-e', tag]

    if platforms:
        for p in platforms:
            parts += ['-p', p]

    return ' '.join(parts)


def generate_report(analysis, verbose=False):
    """
    Render the analysis as a human-readable text report.

    Returns the report as a string.
    """
    pr = analysis['pr']
    h = analysis['heuristic']
    llm = analysis['llm']
    complexity = h['complexity']
    c_level = llm.get('complexity', complexity['level']) if llm else complexity['level']

    lines = []
    sep = '=' * 72
    lines.append(sep)
    lines.append(f"PR Test Advisor Report — #{pr['number']}: {pr['title']}")
    lines.append(sep)
    lines.append(f"  URL:    {pr['url']}")
    lines.append(f"  Author: {pr['author']}")
    lines.append(f"  State:  {pr['state']}{' [DRAFT]' if pr['draft'] else ''}")
    lines.append(f"  Base:   {pr['base_branch']}")
    labels = ', '.join(pr['labels']) if pr['labels'] else '(none)'
    lines.append(f"  Labels: {labels}")
    stats = pr['diff_stats']
    lines.append(
        f"  Diff:   {stats['changed_files']} files, "
        f"+{stats['additions']}/-{stats['deletions']}"
    )
    lines.append('')

    # --- Summary / AI analysis ---
    lines.append('SUMMARY')
    lines.append('-' * 40)
    if llm and llm.get('summary'):
        lines.append(llm['summary'])
    else:
        lines.append(
            '(No LLM summary \u2014 install openai/anthropic/litellm and set an API key to enable AI analysis)'
        )
        lines.append('')
        lines.append('Affected subsystems (heuristic):')
        for s in h['subsystems']:
            lines.append(f'  • {s}')
    lines.append('')

    # --- Complexity ---
    lines.append('CHANGE COMPLEXITY')
    lines.append('-' * 40)
    lines.append(f"  Rating: {c_level.upper()}")
    if llm and llm.get('complexity_rationale'):
        lines.append(f"  Rationale: {llm['complexity_rationale']}")
    else:
        lines.append(f"  Heuristic score: {complexity['score']}")
        lines.append('  Reasons:')
        for reason in complexity['reasons']:
            lines.append(f'    • {reason}')
    lines.append('')

    # --- Subsystems ---
    lines.append('AFFECTED SUBSYSTEMS / LABELS')
    lines.append('-' * 40)
    if llm and llm.get('affected_subsystems'):
        for s in llm['affected_subsystems']:
            lines.append(f'  • {s}')
    else:
        for s in h['subsystems']:
            lines.append(f'  • {s}')
    if h['area_labels']:
        lines.append('')
        lines.append('  GitHub labels:')
        for lbl in h['area_labels']:
            lines.append(f'    [{lbl}]')
    lines.append('')

    # --- Risk areas ---
    if llm and llm.get('risk_areas'):
        lines.append('RISK AREAS')
        lines.append('-' * 40)
        for r in llm['risk_areas']:
            lines.append(f'  ⚠  {r}')
        lines.append('')

    # --- Twister tags touched ---
    lines.append('TWISTER TAG COVERAGE')
    lines.append('-' * 40)
    if h['touched_tags']:
        lines.append('  Tags TOUCHED by this PR (include in test run):')
        for t in sorted(h['touched_tags']):
            lines.append(f'    + {t}')
    else:
        lines.append('  No specific tags matched.')
    if h['excluded_tags']:
        lines.append('  Tags NOT touched (can be excluded with -e):')
        for t in sorted(h['excluded_tags']):
            lines.append(f'    - {t}')
    lines.append('')

    # --- Twister test paths ---
    lines.append('TWISTER: DIRECT TEST PATHS')
    lines.append('-' * 40)
    if analysis['test_paths']:
        lines.append('  These test/sample directories are directly affected:')
        for path in analysis['test_paths']:
            lines.append(f'    {path}')
    else:
        lines.append('  No directly modified test/sample directories found.')
    lines.append('')

    # --- Twister command ---
    lines.append('TWISTER: SUGGESTED COMMANDS')
    lines.append('-' * 40)

    needs_full = (
        not analysis['test_paths']
        and not h['touched_tags']
        and bool(analysis['unresolved_files'])
    )

    extra_platforms = []
    if llm and llm.get('additional_platforms'):
        extra_platforms = llm['additional_platforms']

    if analysis['test_paths'] and not needs_full:
        cmd = _twister_cmd(
            h['excluded_tags'],
            analysis['test_paths'],
            full_run=False,
            platforms=extra_platforms or None,
        )
        lines.append('  Targeted run (changed tests only):')
        lines.append(f'    {cmd}')
        lines.append('')

    # Suggest tag-filtered integration run
    if h['excluded_tags']:
        cmd_integration = _twister_cmd(
            h['excluded_tags'],
            [],
            full_run=True,
            platforms=extra_platforms or None,
        )
        lines.append('  Integration run (with tag exclusions for unaffected areas):')
        lines.append(f'    {cmd_integration}')
        lines.append('')

    # Full run recommendation
    if needs_full or c_level in ('high', 'critical'):
        lines.append('  ⚠  Full twister run recommended due to complexity or cross-subsystem changes:')
        lines.append('    ./scripts/twister --integration')
        lines.append('')

    # --- Pytest ---
    lines.append('PYTEST: RECOMMENDED TEST SUITES')
    lines.append('-' * 40)
    if analysis['pytest_tests']:
        for entry in analysis['pytest_tests']:
            lines.append(f"  {entry['description']}:")
            for p in entry['paths']:
                lines.append(f"    pytest {p}/")
    else:
        lines.append('  No matching pytest suites for the changed files.')
    lines.append('')

    # --- LLM test focus ---
    if llm and llm.get('test_focus'):
        lines.append('AI-RECOMMENDED TEST FOCUS')
        lines.append('-' * 40)
        for focus in llm['test_focus']:
            lines.append(f'  • {focus}')
        lines.append('')

    # --- AI notes ---
    if llm and llm.get('notes'):
        lines.append('TESTING NOTES')
        lines.append('-' * 40)
        lines.append(f"  {llm['notes']}")
        lines.append('')

    # --- Changed files (verbose) ---
    if verbose:
        lines.append('CHANGED FILES')
        lines.append('-' * 40)
        for f in sorted(pr['files']):
            lines.append(f'  {f}')
        lines.append('')

        if analysis['unresolved_files']:
            lines.append('UNRESOLVED FILES (may trigger full twister)')
            lines.append('-' * 40)
            for f in sorted(analysis['unresolved_files']):
                lines.append(f'  {f}')
            lines.append('')

    lines.append(sep)
    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Argument parsing and entry point
# ---------------------------------------------------------------------------

def parse_args():
    parser = argparse.ArgumentParser(
        description='Analyze a Zephyr PR and recommend tests to run.',
        allow_abbrev=False,
    )
    parser.add_argument(
        '--pr',
        type=int,
        required=True,
        metavar='NUMBER',
        help='GitHub pull request number to analyze',
    )
    parser.add_argument(
        '--token',
        default=None,
        metavar='TOKEN',
        help='GitHub API token (overrides GITHUB_TOKEN env var)',
    )
    parser.add_argument(
        '--org',
        default=GITHUB_ORG,
        metavar='ORG',
        help=f'GitHub organization (default: {GITHUB_ORG})',
    )
    parser.add_argument(
        '--repo',
        default=GITHUB_REPO,
        metavar='REPO',
        help=f'GitHub repository name (default: {GITHUB_REPO})',
    )
    parser.add_argument(
        '--model',
        default='gpt-4o-mini',
        metavar='MODEL',
        help='LLM model name (default: gpt-4o-mini). Examples: '
             'claude-opus-4, ollama/llama3, gemini/gemini-1.5-pro',
    )
    parser.add_argument(
        '--provider',
        default=None,
        choices=['auto', 'openai', 'anthropic', 'litellm'],
        metavar='PROVIDER',
        help='LLM provider: auto (default), openai, anthropic, litellm. '
             'Overrides ADVISOR_PROVIDER env var.',
    )
    parser.add_argument(
        '--output',
        default=None,
        metavar='FILE',
        help='Save full JSON analysis to FILE',
    )
    parser.add_argument(
        '--json',
        action='store_true',
        help='Print JSON output instead of the human-readable report',
    )
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Include full file list in the report',
    )
    parser.add_argument(
        '--no-llm',
        action='store_true',
        help='Disable LLM analysis entirely',
    )
    return parser.parse_args()


def _any_llm_available():
    """Return True if at least one LLM library is installed with a key set."""
    if HAS_OPENAI and os.environ.get('OPENAI_API_KEY'):
        return True
    if HAS_ANTHROPIC and os.environ.get('ANTHROPIC_API_KEY'):
        return True
    if HAS_LITELLM:
        return True
    return False


def main():
    args = parse_args()

    provider = None if args.no_llm else args.provider

    print(f'Fetching PR #{args.pr} from {args.org}/{args.repo} ...', file=sys.stderr)
    pr_data = fetch_pr(args.pr, github_token=args.token, org=args.org, repo=args.repo)

    print('Running analysis ...', file=sys.stderr)
    analysis = analyze_pr(pr_data, llm_model=args.model, llm_provider=provider)

    if analysis['llm']:
        print(
            f"LLM analysis complete (provider: {pr_data.get('llm_provider', 'auto')})",
            file=sys.stderr,
        )
    elif args.no_llm:
        print('LLM analysis disabled (--no-llm).', file=sys.stderr)
    elif not _any_llm_available():
        print(
            'LLM analysis skipped — install openai/anthropic/litellm and set an API key.',
            file=sys.stderr,
        )
    else:
        print('LLM analysis failed (see warnings above).', file=sys.stderr)

    if args.json:
        print(json.dumps(analysis, indent=2, default=str))
    else:
        print(generate_report(analysis, verbose=args.verbose))

    if args.output:
        with open(args.output, 'w') as fh:
            json.dump(analysis, fh, indent=2, default=str)
        print(f'JSON analysis saved to {args.output}', file=sys.stderr)

    return 0


if __name__ == '__main__':
    sys.exit(main())
