#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

"""
Issue Triage Tool: Analyzes open Zephyr GitHub bug issues for triage.

Fetches open issues of type "Bug" (or a custom issue type) from the Zephyr
GitHub repository, performs heuristic and optional LLM-based analysis
of each issue, then generates a self-contained HTML report with per-issue
triage recommendations.

Analysis dimensions:
  - Priority assessment  : is the current priority label appropriate?
  - Duplicate detection  : cluster similar issues, flag likely duplicates
  - Criticality scoring  : severity signals from title and body text
  - Already-fixed check  : search git log for "Fixes #N" commit references
  - Report quality       : distinguish CI test-dump reports from real bugs
  - Platform specificity : hardware-only issues vs general Zephyr bugs
  - Triage verdict       : valid-bug | duplicate | already-fixed |
                           wrong-priority | not-a-bug | platform-specific |
                           test-failure-only | stale | needs-triage

Supported LLM providers (selected via --provider or TRIAGE_PROVIDER env var):

  openai     - OpenAI models (gpt-4o, o3-mini, ...)
               Requires: pip install openai
               Env:      OPENAI_API_KEY, OPENAI_BASE_URL (optional)

  anthropic  - Anthropic Claude models (claude-opus-4, claude-sonnet-4-5, ...)
               Requires: pip install anthropic
               Env:      ANTHROPIC_API_KEY

  litellm    - Universal proxy; 100+ providers via a single interface.
               Requires: pip install litellm
               Env:      provider-specific (OPENAI_API_KEY, etc.)

  openrouter - OpenRouter.ai aggregator: 200+ models via one API key.
               Model strings: anthropic/claude-opus-4, google/gemini-pro, ...
               Requires: pip install openai
               Env:      OPENROUTER_API_KEY
                         OPENROUTER_SITE_URL (optional)
                         OPENROUTER_SITE_NAME (optional)

  auto       - (default) Infer from model name and available packages.
               OPENROUTER_API_KEY set -> openrouter;
               claude-* -> anthropic; gpt-*/o1-*/o3-*/o4-* -> openai;
               anything containing '/' -> litellm.

Usage (heuristic only, no API key needed):
    GITHUB_TOKEN=ghp_xxx ./scripts/ci/issue_triage.py --output report.html

Usage (with OpenRouter LLM):
    GITHUB_TOKEN=ghp_xxx OPENROUTER_API_KEY=sk-or-... \\
        ./scripts/ci/issue_triage.py \\
        --model anthropic/claude-opus-4 \\
        --output report.html

Usage (analyze a single issue for debugging):
    GITHUB_TOKEN=ghp_xxx ./scripts/ci/issue_triage.py --issue 12345 --no-llm

Usage (custom issue type):
    GITHUB_TOKEN=ghp_xxx ./scripts/ci/issue_triage.py --type Enhancement

Environment variables:
    GITHUB_TOKEN          GitHub API token (rate limits / private repos)
    TRIAGE_PROVIDER       Default LLM provider (overridden by --provider)
    OPENAI_API_KEY        OpenAI / compatible key
    OPENAI_BASE_URL       Optional base URL for OpenAI-compatible endpoints
    ANTHROPIC_API_KEY     Anthropic API key
    OPENROUTER_API_KEY    OpenRouter.ai API key
    OPENROUTER_SITE_URL   Optional: site URL shown in or.ai dashboard
    OPENROUTER_SITE_NAME  Optional: app name shown in or.ai dashboard

Requirements:
    pip install PyGithub pyyaml
    pip install openai      # for openai and openrouter providers
    pip install anthropic   # for anthropic provider
    pip install litellm     # for litellm provider

Output:
    Generates a self-contained HTML report (--output, default:
    issue_triage_report.html) and optionally a raw JSON file (--json).
"""

import argparse
import datetime
import html as html_module
import json
import logging
import os
import re
import subprocess
import sys
import urllib.parse
import urllib.request
from pathlib import Path

try:
    import yaml
    try:
        from yaml import CSafeLoader as SafeLoader
    except ImportError:
        from yaml import SafeLoader
    HAS_YAML = True
except ImportError:
    HAS_YAML = False
    SafeLoader = None

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
MAINTAINERS_YAML = ZEPHYR_BASE / 'MAINTAINERS.yml'

GITHUB_ORG = 'zephyrproject-rtos'
GITHUB_REPO = 'zephyr'
OPENROUTER_BASE_URL = 'https://openrouter.ai/api/v1'

_ANTHROPIC_PREFIXES = ('claude-',)
_OPENAI_PREFIXES = ('gpt-', 'o1-', 'o3-', 'o4-', 'text-davinci')

DEFAULT_ISSUE_TYPE = 'Bug'
DEFAULT_MAX_ISSUES = 200
DEFAULT_OUTPUT = 'issue_triage_report.html'

GITHUB_API_BASE = 'https://api.github.com'
GITHUB_API_VERSION = '2022-11-28'

# Canonical priority tiers extracted from common Zephyr label formats
PRIORITY_LABELS = {
    'priority: critical': 'critical',
    'priority: high':     'high',
    'priority: medium':   'medium',
    'priority: low':      'low',
    'p0':                 'critical',
    'p1':                 'high',
    'p2':                 'medium',
    'p3':                 'low',
    'p4':                 'negligible',
}

PRIORITY_ORDER = ['critical', 'high', 'medium', 'low', 'negligible']

VERDICTS = [
    'valid-bug',
    'duplicate',
    'already-fixed',
    'wrong-priority',
    'not-a-bug',
    'platform-specific',
    'test-failure-only',
    'stale',
    'needs-triage',
]

VERDICT_LABELS = {
    'valid-bug':          'Valid Bug',
    'duplicate':          'Duplicate',
    'already-fixed':      'Already Fixed',
    'wrong-priority':     'Wrong Priority',
    'not-a-bug':          'Not a Bug',
    'platform-specific':  'Platform Specific',
    'test-failure-only':  'Test Failure Only',
    'stale':              'Stale',
    'needs-triage':       'Needs Triage',
}

VERDICT_COLORS = {
    'valid-bug':          '#ef5350',
    'duplicate':          '#fb8c00',
    'already-fixed':      '#43a047',
    'wrong-priority':     '#1e88e5',
    'not-a-bug':          '#8e24aa',
    'platform-specific':  '#00acc1',
    'test-failure-only':  '#f4511e',
    'stale':              '#546e7a',
    'needs-triage':       '#795548',
}

CRITICALITY_ORDER = ['critical', 'high', 'medium', 'low', 'negligible']

CRITICALITY_COLORS = {
    'critical':   '#b71c1c',
    'high':       '#e53935',
    'medium':     '#f57c00',
    'low':        '#f9a825',
    'negligible': '#388e3c',
}

# Issue age thresholds for staleness detection
_STALE_AGE_DAYS = 180
_STALE_INACTIVE_DAYS = 90

# ---------------------------------------------------------------------------
# Heuristic pattern compilation
# ---------------------------------------------------------------------------

_TEST_FAILURE_TITLE_RE = re.compile(
    r'(?i)(test\s+fail|build\s+fail|ci\s+fail|twister\s+fail'
    r'|test\s+error|failing\s+tests?|test\s+regression'
    r'|tests?\s+broken|ci\s+broken)',
)

_TEST_FAILURE_BODY_RE = re.compile(
    r'(?i)(tests?/\S+/\S+\s|FAILED:\s*\d+|twister.*error'
    r'|platform\s+\S+\s+(?:fail|pass)|Error\s+rate:\s*\d'
    r'|test\s+case.*FAILED|ztest.*FAIL)',
)

_PLATFORM_SPECIFIC_TITLE_RE = re.compile(
    r'(?i)(only\s+on\s+\S+|fails\s+on\s+\S+|specific\s+to\s+\S+'
    r'|\bboard:\s*\S+|\bplatform:\s*\S+)',
)

_BOARD_NAME_RE = re.compile(
    r'(?i)\b(nrf52\w*|nrf91\w*|nrf54\w*|stm32\w*|esp32\w*|rp2040'
    r'|frdm[_-]\w+|nucleo[_-]\w+|arduino\w*|lpcxpresso\w*'
    r'|mimxrt\w*|sam[dle]\w*|bl5340|thingy91|bl654'
    r'|qemu_x86|qemu_cortex\w*|native_sim|imxrt\w*)\b',
)

_QUESTION_RE = re.compile(
    r'(?i)^\s*(how\s+do\s+i|how\s+to|how\s+can\s+i|is\s+it\s+possible'
    r'|can\s+i\s+|should\s+i\s+|question\s*:|feature\s+request\s*:'
    r'|enhancement\s*:|help\s+needed\s*:)',
)

_CRASH_CRITICAL_RE = re.compile(
    r'(?i)\b(crash|hang\b|deadlock|panic|hardfault|memory\s+corruption'
    r'|security\s+vulnerabilit|cve[-\s]\d|buffer\s+overflow'
    r'|use.after.free|stack\s+overflow|heap\s+corruption'
    r'|data\s+loss|bricked|irrecoverable|boot\s+loop|bootloop'
    r'|null\s+pointer\s+deref|kernel\s+panic)\b',
)

_HIGH_RE = re.compile(
    r'(?i)\b(regression|broken\b|doesn.t\s+work|not\s+work\w*'
    r'|infinite\s+loop|stuck\b|freeze\b|unresponsive|wrong\s+output'
    r'|corrupt\w*|undefined\s+behavior|out\s+of\s+memory\b|oom\b'
    r'|assert\s+fail|assertion\s+fail|segfault|sigsegv)\b',
)

_MEDIUM_RE = re.compile(
    r'(?i)\b(warning\b|deprecat\w*|workaround|intermittent|flaky'
    r'|occasionally|sometimes\s+fail|performance\s+issue|timeout\b)\b',
)

_LOW_RE = re.compile(
    r'(?i)\b(typo\b|cosmetic|minor\s+issue|misleading|comment\s+error'
    r'|documentation\s+error|nitpick|style\s+issue|wrong\s+comment)\b',
)

_REPRO_RE = re.compile(
    r'(?i)(steps\s+to\s+repro|to\s+reproduce|reproduction\s+steps'
    r'|expected\s+(?:behavior|result|output)|actual\s+(?:behavior|result|output)'
    r'|observed\s+behavior|current\s+behavior)',
)

# ---------------------------------------------------------------------------
# MAINTAINERS.yml lookup
# ---------------------------------------------------------------------------

def load_maintainers_by_label():
    """
    Build a {label_lower: {area, maintainers}} map from MAINTAINERS.yml.
    Returns an empty dict if the file cannot be loaded.
    """
    if not HAS_YAML or not MAINTAINERS_YAML.exists():
        return {}
    try:
        with open(MAINTAINERS_YAML) as fh:
            data = yaml.load(fh, Loader=SafeLoader)
    except Exception as exc:
        log.debug('Could not load MAINTAINERS.yml: %s', exc)
        return {}
    label_map = {}
    for area_name, area_data in data.items():
        if not isinstance(area_data, dict):
            continue
        maintainers = area_data.get('maintainers', [])
        for label in area_data.get('labels', []):
            label_map[label.lower()] = {
                'area': area_name,
                'maintainers': maintainers,
            }
    return label_map


# ---------------------------------------------------------------------------
# Similarity helpers
# ---------------------------------------------------------------------------

def _title_words(title):
    """Return a normalized word set for Jaccard similarity."""
    words = re.sub(r'[^\w\s]', ' ', title.lower()).split()
    stop = {
        'a', 'an', 'the', 'is', 'in', 'on', 'at', 'to', 'for', 'of',
        'and', 'or', 'not', 'with', 'by', 'are', 'was', 'does', 'do',
        'be', 'has', 'have', 'that', 'this', 'it', 'its', 'when',
        'after', 'from', 'but', 'if', 'as', 'into', 'so', 'than',
    }
    return set(w for w in words if w not in stop and len(w) > 2)


def compute_similarity(title1, title2):
    """Return Jaccard similarity (0.0-1.0) between two issue titles."""
    w1 = _title_words(title1)
    w2 = _title_words(title2)
    union = len(w1 | w2)
    return len(w1 & w2) / union if union else 0.0


def find_similar_issues(issue, all_issues, threshold=0.30, max_results=5):
    """
    Return a list of (number, score, title) tuples for issues whose title
    is similar to the given issue above the threshold.
    Sorted by descending score.
    """
    own_num = issue['number']
    own_title = issue['title']
    candidates = []
    for other in all_issues:
        if other['number'] == own_num:
            continue
        score = compute_similarity(own_title, other['title'])
        if score >= threshold:
            candidates.append((other['number'], round(score, 2), other['title']))
    candidates.sort(key=lambda x: -x[1])
    return candidates[:max_results]


# ---------------------------------------------------------------------------
# Already-fixed check (git log search)
# ---------------------------------------------------------------------------

def check_already_fixed_git(issue_number):
    """
    Search the git log for commits that reference fixing this issue.
    Looks for "Fixes #N" and "Closes #N" (case-insensitive variants).
    Returns a list of oneline commit strings.
    """
    commits = []
    seen = set()
    patterns = [
        f'Fixes #{issue_number}',
        f'Closes #{issue_number}',
        f'fixes #{issue_number}',
        f'closes #{issue_number}',
    ]
    cmd = ['git', '-C', str(ZEPHYR_BASE), 'log', '--oneline']
    for p in patterns:
        cmd += ['--grep', p]
    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=15,
        )
        for line in result.stdout.splitlines():
            line = line.strip()
            if line and line not in seen:
                seen.add(line)
                commits.append(line)
    except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
        pass
    return commits


# ---------------------------------------------------------------------------
# Heuristic analysis
# ---------------------------------------------------------------------------

def _extract_priority(labels):
    """Extract canonical priority string from a list of label names."""
    for label in labels:
        key = label.lower().strip()
        if key in PRIORITY_LABELS:
            return PRIORITY_LABELS[key]
    return None


def _extract_area_labels(labels):
    """Return labels that start with 'area:' (case-insensitive)."""
    return [l for l in labels if l.lower().startswith('area:')]


def _heuristic_criticality(issue):
    """
    Estimate criticality from title/body keywords.
    Returns one of: critical | high | medium | low | negligible
    """
    title = (issue.get('title') or '').lower()
    body = (issue.get('body') or '').lower()
    text = title + ' ' + body[:2000]

    if _CRASH_CRITICAL_RE.search(text):
        return 'critical'
    if _HIGH_RE.search(text):
        return 'high'
    if _MEDIUM_RE.search(text):
        return 'medium'
    if _LOW_RE.search(text):
        return 'low'
    return 'medium'


def _report_quality(issue):
    """
    Assess the quality of the bug report: good | minimal | poor.
    - good    : has reproduction steps or expected/actual behavior
    - minimal : some description but missing key structure
    - poor    : less than 50 chars or no body
    """
    body = issue.get('body') or ''
    if len(body.strip()) < 50:
        return 'poor'
    if _REPRO_RE.search(body):
        return 'good'
    if len(body.strip()) < 200:
        return 'minimal'
    return 'minimal'


def heuristic_analyze_issue(issue, all_issues, maintainers_by_label):
    """
    Perform heuristic triage analysis on a single issue.

    Returns a dict with: criticality, verdict, signals, flags, similar_issues,
    git_commits, report_quality, maintainers.
    """
    title = issue.get('title') or ''
    body = issue.get('body') or ''
    age_days = issue.get('age_days', 0)
    inactive_days = issue.get('inactive_days', 0)

    signals = []

    # Structural flags
    is_test_failure = bool(
        _TEST_FAILURE_TITLE_RE.search(title)
        or (
            _TEST_FAILURE_BODY_RE.search(body)
            and len(body.split('\n')) > 8
            and len(body.strip()) > 300
        )
    )

    board_matches = _BOARD_NAME_RE.findall(title + ' ' + body[:300])
    is_platform_specific = bool(
        _PLATFORM_SPECIFIC_TITLE_RE.search(title)
        or len(board_matches) >= 2
    )

    is_question = bool(_QUESTION_RE.search(title))
    is_stale = (age_days > _STALE_AGE_DAYS and inactive_days > _STALE_INACTIVE_DAYS)

    # Git-based already-fixed check
    git_commits = check_already_fixed_git(issue['number'])
    is_fixed = bool(git_commits)

    # Title similarity (duplicate candidates)
    similar_issues = find_similar_issues(issue, all_issues)

    # Report quality
    quality = _report_quality(issue)

    # Criticality
    criticality = _heuristic_criticality(issue)

    # Priority assessment
    current_priority = issue.get('current_priority')
    priority_wrong = False
    if current_priority:
        cp_idx = PRIORITY_ORDER.index(current_priority) if current_priority in PRIORITY_ORDER else 2
        cr_idx = PRIORITY_ORDER.index(criticality) if criticality in PRIORITY_ORDER else 2
        # More than 2 tiers off -> flag as wrong
        if abs(cp_idx - cr_idx) >= 2:
            priority_wrong = True
            signals.append(
                f'Priority mismatch: labeled {current_priority!r} but'
                f' estimated criticality is {criticality!r}'
            )

    # Verdict determination (ordered by specificity)
    if is_fixed:
        verdict = 'already-fixed'
        signals.append(f'{len(git_commits)} git commit(s) reference fixing this issue')
    elif is_question:
        verdict = 'not-a-bug'
        signals.append('Title suggests a question or feature request rather than a bug')
    elif is_test_failure:
        verdict = 'test-failure-only'
        signals.append('Matches CI/test failure dump pattern (no root-cause analysis)')
    elif similar_issues and similar_issues[0][1] >= 0.60:
        verdict = 'duplicate'
        num, score, stitle = similar_issues[0]
        signals.append(
            f'High title similarity ({score:.0%}) with issue #{num}: {stitle[:60]}'
        )
    elif is_stale:
        verdict = 'stale'
        signals.append(
            f'No activity for {inactive_days} days (issue is {age_days} days old)'
        )
    elif priority_wrong:
        verdict = 'wrong-priority'
    elif is_platform_specific:
        verdict = 'platform-specific'
        if board_matches:
            signals.append(
                f'References platform-specific hardware: {", ".join(set(board_matches[:3]))}'
            )
    elif quality == 'poor':
        verdict = 'needs-triage'
        signals.append('Poor report quality; insufficient information to triage')
    else:
        verdict = 'needs-triage'
        signals.append('No strong heuristic signals; requires manual review')

    if not signals:
        signals.append('No strong heuristic signals detected')

    # Maintainer lookup from area labels
    maintainers = []
    seen_m = set()
    for area_label in issue.get('area_labels', []):
        info = maintainers_by_label.get(area_label.lower())
        if info:
            for m in info.get('maintainers', []):
                if m not in seen_m:
                    seen_m.add(m)
                    maintainers.append(m)

    return {
        'criticality': criticality,
        'verdict': verdict,
        'signals': signals,
        'is_test_failure': is_test_failure,
        'is_platform_specific': is_platform_specific,
        'is_question': is_question,
        'is_stale': is_stale,
        'report_quality': quality,
        'similar_issues': similar_issues,
        'git_commits': git_commits,
        'priority_wrong': priority_wrong,
        'maintainers': maintainers,
    }


# ---------------------------------------------------------------------------
# LLM provider abstraction (mirrors pr_test_advisor.py)
# ---------------------------------------------------------------------------

def _resolve_provider(model, provider_hint):
    """
    Determine which LLM provider to use.

    Resolution order:
      1. Explicit --provider / TRIAGE_PROVIDER value
      2. OPENROUTER_API_KEY present -> openrouter
      3. Model name prefix heuristic
      4. litellm if installed
      5. First available library with a configured key
    """
    hint = provider_hint or os.environ.get('TRIAGE_PROVIDER', 'auto')

    if hint != 'auto':
        if hint in ('openai', 'openrouter') and not HAS_OPENAI:
            log.warning('Provider "%s" requires the openai package', hint)
            return None
        if hint == 'anthropic' and not HAS_ANTHROPIC:
            log.warning('Provider "anthropic" requires the anthropic package')
            return None
        if hint == 'litellm' and not HAS_LITELLM:
            log.warning('Provider "litellm" requires the litellm package')
            return None
        return hint

    if os.environ.get('OPENROUTER_API_KEY') and HAS_OPENAI:
        return 'openrouter'

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

    if HAS_OPENAI and os.environ.get('OPENAI_API_KEY'):
        return 'openai'
    if HAS_ANTHROPIC and os.environ.get('ANTHROPIC_API_KEY'):
        return 'anthropic'
    if HAS_LITELLM:
        return 'litellm'

    return None


def _extract_json(raw):
    """
    Extract a JSON object from raw LLM text using three fallback strategies:
      1. Direct parse
      2. Strip markdown code fences
      3. Regex search for the first {...} block
    """
    if not raw or not raw.strip():
        raise json.JSONDecodeError('Empty response from LLM', '', 0)

    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        pass

    stripped = re.sub(r'^```(?:json)?\s*', '', raw.strip(), flags=re.IGNORECASE)
    stripped = re.sub(r'```\s*$', '', stripped.strip())
    try:
        return json.loads(stripped)
    except json.JSONDecodeError:
        pass

    match = re.search(r'\{.*\}', raw, re.DOTALL)
    if match:
        try:
            return json.loads(match.group(0))
        except json.JSONDecodeError:
            pass

    raise json.JSONDecodeError('No JSON object found in LLM response', raw, 0)


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
        max_tokens=1500,
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
        max_tokens=1500,
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
        max_tokens=1500,
    )
    return response.choices[0].message.content


def _call_openrouter(model, system, user):
    """
    Call OpenRouter.ai using the OpenAI-compatible endpoint.
    response_format is intentionally omitted; JSON is extracted by
    _extract_json() to support all proxied backends.
    """
    api_key = os.environ.get('OPENROUTER_API_KEY')
    if not api_key:
        raise RuntimeError('OPENROUTER_API_KEY is not set')
    extra_headers = {}
    site_url = os.environ.get('OPENROUTER_SITE_URL', '')
    site_name = os.environ.get('OPENROUTER_SITE_NAME', 'Zephyr Issue Triage')
    if site_url:
        extra_headers['HTTP-Referer'] = site_url
    extra_headers['X-Title'] = site_name
    client = openai.OpenAI(
        api_key=api_key,
        base_url=OPENROUTER_BASE_URL,
        default_headers=extra_headers,
    )
    response = client.chat.completions.create(
        model=model,
        messages=[
            {'role': 'system', 'content': system},
            {'role': 'user', 'content': user},
        ],
        temperature=0.2,
        max_tokens=1500,
    )
    content = response.choices[0].message.content
    finish_reason = response.choices[0].finish_reason
    if not content or not content.strip():
        raise json.JSONDecodeError(
            f'Empty response from OpenRouter (finish_reason={finish_reason!r})', '', 0
        )
    return content


# ---------------------------------------------------------------------------
# LLM system prompt and per-issue analysis
# ---------------------------------------------------------------------------

ISSUE_TRIAGE_SYSTEM_PROMPT = """\
You are an expert Zephyr RTOS maintainer performing bug triage.
Analyze the GitHub issue described below and respond with ONLY a JSON object.

IMPORTANT: Your entire response must be a single valid JSON object.
Do NOT wrap it in ```json fences. Do NOT add any text before or after.

The JSON object must have exactly these fields:
{
  "summary": "<one concise sentence describing what the bug is>",
  "criticality": "<critical|high|medium|low|negligible>",
  "priority_assessment": "<correct|too-high|too-low|not-set>",
  "priority_suggested": "<critical|high|medium|low|none>",
  "verdict": "<valid-bug|duplicate|already-fixed|not-a-bug|platform-specific|test-failure-only|wrong-priority|stale|needs-triage>",
  "verdict_rationale": "<one sentence explaining the verdict>",
  "is_platform_specific": <true|false>,
  "affected_subsystem": "<subsystem name, e.g. Bluetooth, Networking, Kernel, Drivers/SPI>",
  "duplicate_candidate": <null or integer issue number>,
  "action_items": ["<specific action 1>", "<specific action 2>"],
  "notes": "<additional triage notes or empty string>"
}

Verdict definitions:
- valid-bug: well-described bug that needs fixing, not yet fixed
- duplicate: very similar to one of the listed candidate issues
- already-fixed: evidence the bug is already fixed (git commits listed or recently merged PR)
- not-a-bug: user error, question, feature request, or working as designed
- platform-specific: only affects specific hardware/platform, not a general Zephyr issue
- test-failure-only: CI/test failure report with no root cause; likely flaky test or setup issue
- wrong-priority: valid bug but priority label does not match actual severity
- stale: no activity for a long time; may no longer be relevant
- needs-triage: insufficient information to make a determination

Criticality definitions:
- critical: crash, hang, data loss, security vulnerability, system failure
- high: major functionality broken, regression, affects many users
- medium: partial functionality broken, workaround exists
- low: minor issue, cosmetic, rare edge case
- negligible: trivial, documentation-only, cosmetic

Priority assessment:
- correct: assigned priority matches estimated criticality
- too-high: issue is less severe than assigned priority suggests
- too-low: issue is more severe than assigned priority (needs escalation)
- not-set: no priority label is assigned
"""


def _build_issue_prompt(issue, similar_issues, git_commits):
    """Build the user-side content for LLM analysis of a single issue."""
    lines = [
        f"Issue: #{issue['number']}",
        f"Title: {issue['title']}",
        f"URL: {issue['url']}",
        f"Author: {issue['author']}",
        f"Created: {issue['created_at']} ({issue['age_days']} days ago)",
        f"Last updated: {issue['updated_at']} ({issue['inactive_days']} days inactive)",
        f"Comments: {issue['comment_count']}",
        f"Labels: {', '.join(issue['labels']) if issue['labels'] else 'none'}",
        f"Current priority: {issue['current_priority'] or 'not set'}",
        f"Area labels: {', '.join(issue['area_labels']) if issue['area_labels'] else 'none'}",
        '',
        'Description:',
        (issue.get('body') or '(no description provided)')[:3000],
    ]

    if git_commits:
        lines += ['', 'Git commits referencing this issue (possible fixes):']
        for c in git_commits[:5]:
            lines.append(f'  {c}')

    if similar_issues:
        lines += ['', 'Issues with similar titles (possible duplicates):']
        for num, score, title in similar_issues[:5]:
            lines.append(f'  #{num} ({score:.0%} similar): {title}')

    return '\n'.join(lines)


def llm_analyze_issue(issue, similar_issues, git_commits, model, provider):
    """
    Call the LLM to triage a single issue.

    Returns a parsed dict or None on failure.
    """
    resolved = _resolve_provider(model, provider)
    if resolved is None:
        return None

    system = ISSUE_TRIAGE_SYSTEM_PROMPT
    user = _build_issue_prompt(issue, similar_issues, git_commits)

    _BACKENDS = {
        'openai':      _call_openai,
        'anthropic':   _call_anthropic,
        'litellm':     _call_litellm,
        'openrouter':  _call_openrouter,
    }

    backend_fn = _BACKENDS.get(resolved)
    if backend_fn is None:
        log.warning('Unknown provider: %s', resolved)
        return None

    log.info('LLM analyzing issue #%d via %s (%s)', issue['number'], resolved, model)
    try:
        raw = backend_fn(model, system, user)
        log.debug('Raw LLM response for #%d: %s', issue['number'], raw)
        return _extract_json(raw)
    except json.JSONDecodeError as exc:
        log.warning('LLM failed for issue #%d (%s): %s', issue['number'], resolved, exc)
        if exc.doc and exc.doc.strip():
            log.warning(
                'Raw response was:\n%s',
                exc.doc[:2000] + ('...' if len(exc.doc) > 2000 else ''),
            )
        else:
            log.warning(
                'Model returned empty response for #%d. '
                'Try a different model with --model.',
                issue['number'],
            )
        return None
    except Exception as exc:
        log.warning('LLM failed for issue #%d (%s): %s', issue['number'], resolved, exc)
        return None


# ---------------------------------------------------------------------------
# GitHub REST API helpers
# ---------------------------------------------------------------------------

def _gh_api_request(path, token, params=None):
    """
    Perform a single GET request against the GitHub REST API.

    Returns the parsed JSON response body.
    Raises SystemExit on HTTP errors.
    """
    base = f'{GITHUB_API_BASE}{path}'
    if params:
        base = f'{base}?{urllib.parse.urlencode(params)}'
    headers = {
        'Accept':               'application/vnd.github+json',
        'X-GitHub-Api-Version': GITHUB_API_VERSION,
        'User-Agent':           'Zephyr-Issue-Triage/1.0',
    }
    if token:
        headers['Authorization'] = f'Bearer {token}'
    req = urllib.request.Request(base, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode('utf-8'))
    except urllib.error.HTTPError as exc:
        body = ''
        try:
            body = exc.read().decode('utf-8', errors='replace')[:200]
        except Exception:
            pass
        raise SystemExit(
            f'GitHub API {exc.code} for {path}: {body}'
        ) from exc
    except Exception as exc:
        raise SystemExit(f'GitHub API request failed: {exc}') from exc


def _gh_api_paginate(path, token, params, max_items):
    """
    Paginate through a GitHub REST API list endpoint.

    Yields raw issue dicts (not PRs) until max_items is reached or
    all pages are exhausted.
    """
    page = 1
    fetched = 0
    while fetched < max_items:
        page_params = dict(params)
        page_params['per_page'] = 100
        page_params['page'] = page
        items = _gh_api_request(path, token, page_params)
        if not items:
            break
        for item in items:
            if item.get('pull_request'):
                continue
            yield item
            fetched += 1
            if fetched >= max_items:
                break
        if len(items) < 100:
            break
        page += 1


def _normalize_issue(raw, now):
    """
    Normalize a raw GitHub REST API issue dict into the internal format.
    """
    def _parse_dt(s):
        if not s:
            return now
        s = s.rstrip('Z')
        try:
            dt = datetime.datetime.fromisoformat(s)
        except ValueError:
            return now
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=datetime.timezone.utc)
        return dt

    created_at = _parse_dt(raw.get('created_at'))
    updated_at = _parse_dt(raw.get('updated_at'))
    age_days = (now - created_at).days
    inactive_days = (now - updated_at).days
    label_names = [lbl['name'] for lbl in raw.get('labels', [])]

    issue_type_obj = raw.get('type') or {}
    issue_type_name = issue_type_obj.get('name', '') if isinstance(issue_type_obj, dict) else ''

    return {
        'number':        raw['number'],
        'title':         raw.get('title') or '',
        'url':           raw.get('html_url', ''),
        'author':        (raw.get('user') or {}).get('login', ''),
        'created_at':    created_at.strftime('%Y-%m-%d'),
        'updated_at':    updated_at.strftime('%Y-%m-%d'),
        'age_days':      age_days,
        'inactive_days': inactive_days,
        'labels':        label_names,
        'issue_type':    issue_type_name,
        'current_priority': _extract_priority(label_names),
        'area_labels':   _extract_area_labels(label_names),
        'body':          (raw.get('body') or '')[:4000],
        'comment_count': raw.get('comments', 0),
        'assignees':     [
            (a.get('login') or '') for a in (raw.get('assignees') or [])
        ],
    }


# ---------------------------------------------------------------------------
# GitHub issue fetcher
# ---------------------------------------------------------------------------

def fetch_issues(
    github_token=None,
    org=GITHUB_ORG,
    repo=GITHUB_REPO,
    issue_type=DEFAULT_ISSUE_TYPE,
    max_issues=DEFAULT_MAX_ISSUES,
    days=0,
    issue_number=None,
):
    """
    Fetch open issues of the given issue type from GitHub.

    Uses the GitHub REST API directly so that the 'type' query parameter
    (GitHub issue types feature) is honoured — PyGithub does not expose
    this parameter yet.

    If issue_number is provided, only that single issue is fetched and
    returned regardless of its type.
    If days > 0, only issues updated within the last N days are returned.
    Returns a list of issue dicts with normalized fields.
    """
    token = github_token or os.environ.get('GITHUB_TOKEN')
    now = datetime.datetime.now(datetime.timezone.utc)
    cutoff = now - datetime.timedelta(days=days) if days > 0 else None
    path = f'/repos/{org}/{repo}/issues'
    results = []

    if issue_number is not None:
        raw = _gh_api_request(f'{path}/{issue_number}', token)
        if raw.get('pull_request'):
            raise SystemExit(f'#{issue_number} is a pull request, not an issue')
        results.append(_normalize_issue(raw, now))
        return results

    params = {
        'state':     'open',
        'type':      issue_type,
        'sort':      'updated',
        'direction': 'desc',
    }

    for raw in _gh_api_paginate(path, token, params, max_issues):
        updated_at_str = raw.get('updated_at', '')
        if cutoff and updated_at_str:
            updated_dt = datetime.datetime.fromisoformat(
                updated_at_str.rstrip('Z')
            ).replace(tzinfo=datetime.timezone.utc)
            if updated_dt < cutoff:
                break
        results.append(_normalize_issue(raw, now))

    return results


# ---------------------------------------------------------------------------
# Full triage orchestration
# ---------------------------------------------------------------------------

def _merge_verdict(heuristic, llm_result):
    """
    Merge heuristic and LLM results into a final verdict dict.
    LLM result overrides heuristic when available.
    """
    if llm_result:
        return {
            'criticality':       llm_result.get('criticality', heuristic['criticality']),
            'verdict':           llm_result.get('verdict', heuristic['verdict']),
            'priority_suggested': llm_result.get('priority_suggested', 'none'),
            'affected_subsystem': llm_result.get('affected_subsystem', ''),
            'action_items':      llm_result.get('action_items', []),
            'duplicate_of':      llm_result.get('duplicate_candidate'),
            'source':            'llm',
        }
    # Heuristic-only
    action_items = []
    verdict = heuristic['verdict']
    if verdict == 'already-fixed':
        action_items.append('Verify fix applies and close the issue')
    elif verdict == 'duplicate':
        sims = heuristic.get('similar_issues', [])
        if sims:
            action_items.append(f'Compare with issue #{sims[0][0]} and close if duplicate')
    elif verdict == 'test-failure-only':
        action_items.append('Investigate root cause of test failures; close if setup issue')
    elif verdict == 'stale':
        action_items.append('Ping reporter; close if no response within 14 days')
    elif verdict == 'platform-specific':
        action_items.append('Assign to platform maintainer for investigation')
    elif verdict == 'wrong-priority':
        action_items.append('Update priority label to match actual severity')
    elif verdict == 'not-a-bug':
        action_items.append('Close as not-a-bug / redirect to mailing list or Slack')
    else:
        action_items.append('Review manually and assign to relevant maintainer')

    return {
        'criticality':        heuristic['criticality'],
        'verdict':            verdict,
        'priority_suggested': 'none',
        'affected_subsystem': '',
        'action_items':       action_items,
        'duplicate_of':       None,
        'source':             'heuristic',
    }


def triage_issues(
    issues,
    llm_model='gpt-4o-mini',
    llm_provider=None,
    no_llm=False,
):
    """
    Run full triage (heuristic + optional LLM) on a list of issue dicts.

    Returns a list of fully-analyzed issue dicts.
    """
    maintainers_by_label = load_maintainers_by_label()
    provider = None if no_llm else llm_provider

    results = []
    total = len(issues)

    for idx, issue in enumerate(issues, 1):
        num = issue['number']
        print(
            f'\r  [{idx}/{total}] Analyzing issue #{num} ...          ',
            end='',
            flush=True,
            file=sys.stderr,
        )

        heuristic = heuristic_analyze_issue(issue, issues, maintainers_by_label)

        llm_result = None
        if provider is not None:
            llm_result = llm_analyze_issue(
                issue,
                heuristic['similar_issues'],
                heuristic['git_commits'],
                model=llm_model,
                provider=provider,
            )

        final = _merge_verdict(heuristic, llm_result)

        results.append({
            **issue,
            'heuristic': heuristic,
            'llm':       llm_result,
            'final':     final,
        })

    print('', file=sys.stderr)
    return results


# ---------------------------------------------------------------------------
# HTML report generation
# ---------------------------------------------------------------------------

_CSS = """\
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
  background:#f0f2f5;color:#212121;font-size:14px;line-height:1.5}
a{color:#1976d2;text-decoration:none}a:hover{text-decoration:underline}
header{background:#1a237e;color:#fff;padding:20px 32px}
header h1{font-size:1.6rem;font-weight:700;margin-bottom:4px}
header p{opacity:.8;font-size:.9rem}
.container{max-width:1400px;margin:0 auto;padding:16px 24px}
.stats-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));
  gap:12px;margin:16px 0}
.stat-card{background:#fff;border-radius:8px;padding:14px 16px;
  box-shadow:0 1px 3px rgba(0,0,0,.12);border-left:4px solid #ccc}
.stat-card .val{font-size:1.8rem;font-weight:700;line-height:1}
.stat-card .lbl{font-size:.75rem;color:#666;margin-top:4px;text-transform:uppercase;
  letter-spacing:.5px}
.filters{background:#fff;border-radius:8px;padding:14px 20px;
  margin-bottom:12px;box-shadow:0 1px 3px rgba(0,0,0,.1);
  display:flex;flex-wrap:wrap;gap:12px;align-items:center}
.filters label{font-size:.85rem;color:#555;display:flex;align-items:center;gap:6px}
.filters select,.filters input[type=text]{border:1px solid #ccc;border-radius:4px;
  padding:4px 8px;font-size:.85rem;outline:none}
.filters select:focus,.filters input[type=text]:focus{border-color:#1976d2}
.filters button{padding:5px 14px;border:1px solid #ccc;border-radius:4px;
  background:#fff;cursor:pointer;font-size:.85rem;color:#555}
.filters button:hover{background:#f5f5f5}
.filters .filter-count{margin-left:auto;font-size:.85rem;color:#666}
.tbl-wrap{background:#fff;border-radius:8px;overflow:hidden;
  box-shadow:0 1px 3px rgba(0,0,0,.1)}
table{width:100%;border-collapse:collapse}
thead th{background:#263238;color:#eceff1;padding:10px 12px;text-align:left;
  font-size:.8rem;text-transform:uppercase;letter-spacing:.5px;
  cursor:pointer;white-space:nowrap;user-select:none}
thead th:hover{background:#37474f}
thead th.sorted-asc::after{content:' \u25b2'}
thead th.sorted-desc::after{content:' \u25bc'}
tbody tr.issue-row{cursor:pointer;transition:background .15s}
tbody tr.issue-row:hover{background:#e3f2fd}
tbody tr.issue-row.expanded{background:#e8eaf6}
tbody td{padding:8px 12px;border-bottom:1px solid #eceff1;vertical-align:top}
tbody tr.detail-row td{padding:0;background:#f5f7fa;border-bottom:2px solid #c5cae9}
.detail-inner{padding:16px 20px;display:grid;
  grid-template-columns:1fr 1fr;gap:16px;font-size:.85rem}
.detail-inner h4{font-size:.8rem;text-transform:uppercase;color:#666;
  margin-bottom:6px;letter-spacing:.5px}
.detail-inner ul{padding-left:16px}
.detail-inner li{margin-bottom:3px}
.detail-box{background:#fff;border:1px solid #e0e0e0;border-radius:6px;
  padding:12px 14px}
.detail-full{grid-column:1/-1}
.badge{display:inline-block;padding:2px 10px;border-radius:12px;
  font-size:.75rem;font-weight:600;color:#fff;white-space:nowrap}
.crit-badge{display:inline-block;padding:2px 8px;border-radius:4px;
  font-size:.75rem;font-weight:600;color:#fff}
.quality-good{color:#2e7d32}
.quality-minimal{color:#ef6c00}
.quality-poor{color:#c62828}
.label-chip{display:inline-block;padding:1px 8px;border-radius:10px;
  font-size:.72rem;background:#e0e0e0;color:#424242;margin:1px}
.action-item{background:#e8f5e9;border-left:3px solid #43a047;
  padding:4px 8px;margin:3px 0;border-radius:0 4px 4px 0;font-size:.82rem}
.git-commit{font-family:monospace;font-size:.78rem;background:#f3e5f5;
  padding:2px 8px;border-radius:4px;margin:2px 0;display:block;color:#4a148c}
.similar-issue{font-size:.82rem;color:#1565c0;margin:2px 0}
.no-results{text-align:center;padding:40px;color:#999;font-size:1rem}
.section-title{font-size:1rem;font-weight:600;color:#37474f;
  margin:16px 0 8px;display:flex;align-items:center;gap:8px}
.tag-llm{font-size:.7rem;background:#e8f5e9;color:#1b5e20;
  border:1px solid #a5d6a7;border-radius:10px;padding:1px 7px;
  font-weight:600;vertical-align:middle}
.tag-heuristic{font-size:.7rem;background:#fff3e0;color:#e65100;
  border:1px solid #ffcc80;border-radius:10px;padding:1px 7px;
  font-weight:600;vertical-align:middle}
@media(max-width:900px){.detail-inner{grid-template-columns:1fr}}
"""

_JS = """\
(function(){
var rows=[], filtered=[];
function init(){
  rows=Array.from(document.querySelectorAll('tbody tr.issue-row'));
  filtered=rows.slice();
  updateCount();
  document.querySelectorAll('thead th[data-col]').forEach(function(th){
    th.addEventListener('click',function(){sortBy(th)});
  });
  document.getElementById('fv').addEventListener('change',applyFilters);
  document.getElementById('fc').addEventListener('change',applyFilters);
  document.getElementById('fp').addEventListener('change',applyFilters);
  document.getElementById('fs').addEventListener('input',applyFilters);
  document.getElementById('btn-clear').addEventListener('click',clearFilters);
  rows.forEach(function(row){
    row.addEventListener('click',function(){toggleDetail(row)});
  });
}
function toggleDetail(row){
  var did=row.dataset.detail;
  var dr=document.getElementById(did);
  if(!dr)return;
  var open=dr.style.display!=='none'&&dr.style.display!=='';
  if(open){dr.style.display='none';row.classList.remove('expanded');}
  else{dr.style.display='';row.classList.add('expanded');}
}
function applyFilters(){
  var v=document.getElementById('fv').value;
  var c=document.getElementById('fc').value;
  var p=document.getElementById('fp').value;
  var s=document.getElementById('fs').value.toLowerCase();
  filtered=[];
  rows.forEach(function(row){
    var mv=!v||row.dataset.verdict===v;
    var mc=!c||row.dataset.crit===c;
    var mp=!p||row.dataset.prio===p;
    var ms=!s||(row.dataset.title||'').indexOf(s)>=0
               ||(row.dataset.num||'').indexOf(s)>=0;
    var show=mv&&mc&&mp&&ms;
    row.style.display=show?'':'none';
    var dr=document.getElementById(row.dataset.detail);
    if(dr)dr.style.display='none';
    if(show)row.classList.remove('expanded');
    if(show)filtered.push(row);
  });
  updateCount();
}
function clearFilters(){
  document.getElementById('fv').value='';
  document.getElementById('fc').value='';
  document.getElementById('fp').value='';
  document.getElementById('fs').value='';
  applyFilters();
}
var sortCol=null,sortDir=1;
function sortBy(th){
  var col=th.dataset.col;
  if(sortCol===col){sortDir=-sortDir;}else{sortCol=col;sortDir=1;}
  document.querySelectorAll('thead th').forEach(function(h){
    h.classList.remove('sorted-asc','sorted-desc');
  });
  th.classList.add(sortDir>0?'sorted-asc':'sorted-desc');
  var tbody=document.querySelector('tbody');
  var pairs=filtered.map(function(row){
    return {row:row,val:row.dataset[col]||''};
  });
  pairs.sort(function(a,b){
    var n1=parseFloat(a.val),n2=parseFloat(b.val);
    if(!isNaN(n1)&&!isNaN(n2))return sortDir*(n1-n2);
    return sortDir*a.val.localeCompare(b.val);
  });
  pairs.forEach(function(p){
    tbody.appendChild(p.row);
    var dr=document.getElementById(p.row.dataset.detail);
    if(dr)tbody.appendChild(dr);
  });
}
function updateCount(){
  var el=document.getElementById('filter-count');
  if(el)el.textContent='Showing '+filtered.length+' of '+rows.length+' issues';
}
document.addEventListener('DOMContentLoaded',init);
})();
"""


def _verdict_badge(verdict):
    color = VERDICT_COLORS.get(verdict, '#9e9e9e')
    label = VERDICT_LABELS.get(verdict, verdict)
    return (
        f'<span class="badge" style="background:{color}">'
        f'{html_module.escape(label)}</span>'
    )


def _crit_badge(criticality):
    color = CRITICALITY_COLORS.get(criticality or '', '#9e9e9e')
    label = (criticality or 'unknown').capitalize()
    return (
        f'<span class="crit-badge" style="background:{color}">'
        f'{html_module.escape(label)}</span>'
    )


def _priority_badge(priority):
    if not priority:
        return '<span style="color:#aaa">not set</span>'
    color = CRITICALITY_COLORS.get(priority, '#9e9e9e')
    return (
        f'<span class="crit-badge" style="background:{color}">'
        f'{html_module.escape(priority)}</span>'
    )


def _detail_row(analysis, idx):
    """Render the expandable detail row for a single issue."""
    h = analysis['heuristic']
    llm = analysis.get('llm')
    final = analysis['final']
    esc = html_module.escape

    parts = ['<div class="detail-inner">']

    # Left: Heuristic signals + action items
    parts.append('<div class="detail-box">')
    parts.append('<h4>Heuristic signals</h4><ul>')
    for sig in h.get('signals', []):
        parts.append(f'<li>{esc(sig)}</li>')
    parts.append('</ul>')

    quality = h.get('report_quality', '')
    qclass = {'good': 'quality-good', 'minimal': 'quality-minimal', 'poor': 'quality-poor'}.get(
        quality, ''
    )
    parts.append(f'<p style="margin-top:8px">Report quality: '
                 f'<span class="{qclass}">{esc(quality)}</span></p>')

    if h.get('git_commits'):
        parts.append('<p style="margin-top:8px"><strong>Possible fix commits:</strong></p>')
        for c in h['git_commits'][:5]:
            parts.append(f'<code class="git-commit">{esc(c)}</code>')

    if h.get('similar_issues'):
        parts.append('<p style="margin-top:8px"><strong>Similar issues:</strong></p>')
        for num, score, title in h['similar_issues'][:4]:
            parts.append(
                f'<div class="similar-issue">'
                f'<a href="https://github.com/{GITHUB_ORG}/{GITHUB_REPO}/issues/{num}"'
                f' target="_blank">#{num}</a>'
                f' ({score:.0%}) {esc(title[:70])}</div>'
            )

    parts.append('</div>')

    # Right: LLM analysis or action items
    parts.append('<div class="detail-box">')
    source_tag = (
        '<span class="tag-llm">AI</span>'
        if llm else
        '<span class="tag-heuristic">Heuristic</span>'
    )
    parts.append(f'<h4>Triage result {source_tag}</h4>')

    if llm:
        summary = llm.get('summary', '')
        if summary:
            parts.append(f'<p style="margin-bottom:8px"><em>{esc(summary)}</em></p>')
        rationale = llm.get('verdict_rationale', '')
        if rationale:
            parts.append(f'<p style="margin-bottom:8px">{esc(rationale)}</p>')
        subsystem = llm.get('affected_subsystem', '')
        if subsystem:
            parts.append(
                f'<p><strong>Subsystem:</strong> {esc(subsystem)}</p>'
            )
        pa = llm.get('priority_assessment', '')
        ps = llm.get('priority_suggested', '')
        if pa and pa != 'correct':
            parts.append(
                f'<p><strong>Priority:</strong> {esc(pa)}'
                + (f' (suggested: <strong>{esc(ps)}</strong>)' if ps and ps != 'none' else '')
                + '</p>'
            )
        dup = llm.get('duplicate_candidate')
        if dup:
            parts.append(
                f'<p><strong>Possible duplicate of:</strong> '
                f'<a href="https://github.com/{GITHUB_ORG}/{GITHUB_REPO}/issues/{dup}"'
                f' target="_blank">#{dup}</a></p>'
            )
        notes = llm.get('notes', '')
        if notes:
            parts.append(f'<p style="margin-top:6px;color:#555"><em>{esc(notes)}</em></p>')
    else:
        subsystem = final.get('affected_subsystem', '')
        if subsystem:
            parts.append(f'<p><strong>Subsystem:</strong> {esc(subsystem)}</p>')

    action_items = final.get('action_items', [])
    if action_items:
        parts.append('<p style="margin-top:8px"><strong>Action items:</strong></p>')
        for item in action_items:
            parts.append(f'<div class="action-item">{esc(item)}</div>')

    maintainers = h.get('maintainers', [])
    if maintainers:
        parts.append(
            '<p style="margin-top:8px"><strong>Maintainers:</strong> '
            + ', '.join(f'@{esc(m)}' for m in maintainers[:5])
            + '</p>'
        )

    parts.append('</div>')

    # Full row: labels + description snippet
    parts.append('<div class="detail-box detail-full">')
    labels_html = ' '.join(
        f'<span class="label-chip">{esc(l)}</span>'
        for l in analysis.get('labels', [])
    )
    if labels_html:
        parts.append(f'<div style="margin-bottom:8px">{labels_html}</div>')

    body = (analysis.get('body') or '').strip()
    if body:
        snippet = esc(body[:400]) + ('...' if len(body) > 400 else '')
        parts.append(
            f'<pre style="white-space:pre-wrap;font-size:.8rem;'
            f'background:#f9f9f9;padding:8px;border-radius:4px;'
            f'border:1px solid #e0e0e0;max-height:120px;overflow-y:auto">'
            f'{snippet}</pre>'
        )
    parts.append('</div>')
    parts.append('</div>')

    return ''.join(parts)


def generate_html_report(analyses, args):
    """
    Generate a fully self-contained HTML report for all analyzed issues.
    Returns the HTML as a string.
    """
    esc = html_module.escape
    now_str = datetime.datetime.now().strftime('%Y-%m-%d %H:%M')
    total = len(analyses)

    # Count statistics
    verdict_counts = {v: 0 for v in VERDICTS}
    crit_counts = {c: 0 for c in CRITICALITY_ORDER}
    for a in analyses:
        v = a['final']['verdict']
        c = a['final']['criticality']
        if v in verdict_counts:
            verdict_counts[v] += 1
        if c in crit_counts:
            crit_counts[c] += 1

    llm_count = sum(1 for a in analyses if a.get('llm'))

    parts = [
        '<!DOCTYPE html>',
        '<html lang="en">',
        '<head>',
        '<meta charset="UTF-8">',
        '<meta name="viewport" content="width=device-width,initial-scale=1.0">',
        f'<title>Zephyr Issue Triage Report — {now_str}</title>',
        f'<style>{_CSS}</style>',
        '</head>',
        '<body>',
        '<header>',
        '<h1>Zephyr Issue Triage Report</h1>',
        f'<p>Generated: {esc(now_str)}'
        f' | {total} issues analyzed'
        f' | Issue type: <strong>{esc(args.type)}</strong>'
        f' | Repo: <strong>{esc(args.org)}/{esc(args.repo)}</strong>'
        f' | AI analysis: <strong>{"enabled (" + esc(args.model) + ")" if llm_count > 0 else "disabled"}</strong>'
        f'</p>',
        '</header>',
        '<div class="container">',
    ]

    # Stats: by verdict
    parts.append('<div class="section-title">Summary by Verdict</div>')
    parts.append('<div class="stats-grid">')
    # Total card
    parts.append(
        f'<div class="stat-card" style="border-color:#1a237e">'
        f'<div class="val">{total}</div>'
        f'<div class="lbl">Total Issues</div></div>'
    )
    for v in VERDICTS:
        cnt = verdict_counts[v]
        color = VERDICT_COLORS.get(v, '#9e9e9e')
        parts.append(
            f'<div class="stat-card" style="border-color:{color}">'
            f'<div class="val" style="color:{color}">{cnt}</div>'
            f'<div class="lbl">{esc(VERDICT_LABELS[v])}</div></div>'
        )
    parts.append('</div>')

    # Stats: by criticality
    parts.append('<div class="section-title">Summary by Criticality</div>')
    parts.append('<div class="stats-grid">')
    for c in CRITICALITY_ORDER:
        cnt = crit_counts[c]
        color = CRITICALITY_COLORS.get(c, '#9e9e9e')
        parts.append(
            f'<div class="stat-card" style="border-color:{color}">'
            f'<div class="val" style="color:{color}">{cnt}</div>'
            f'<div class="lbl">{esc(c.capitalize())}</div></div>'
        )
    parts.append('</div>')

    # Filters
    def _options(values, current=None):
        opts = ['<option value="">All</option>']
        for v in values:
            sel = ' selected' if v == current else ''
            opts.append(f'<option value="{esc(v)}"{sel}>{esc(v)}</option>')
        return ''.join(opts)

    parts.append(
        '<div class="filters">'
        '<label>Verdict: <select id="fv">'
        + _options(VERDICTS)
        + '</select></label>'
        '<label>Criticality: <select id="fc">'
        + _options(CRITICALITY_ORDER)
        + '</select></label>'
        '<label>Priority: <select id="fp">'
        + _options(PRIORITY_ORDER + ['not-set'])
        + '</select></label>'
        '<label>Search: <input type="text" id="fs" placeholder="title or issue #" style="width:220px"></label>'
        '<button id="btn-clear">Clear</button>'
        '<span class="filter-count" id="filter-count"></span>'
        '</div>'
    )

    # Table
    parts.append('<div class="tbl-wrap"><table>')
    parts.append(
        '<thead><tr>'
        '<th data-col="num" style="width:70px">#</th>'
        '<th>Title</th>'
        '<th data-col="age" style="width:70px">Age</th>'
        '<th data-col="prio" style="width:110px">Priority</th>'
        '<th data-col="crit" style="width:110px">Criticality</th>'
        '<th data-col="verdict" style="width:150px">Verdict</th>'
        '<th style="width:160px">Subsystem</th>'
        '</tr></thead>'
    )
    parts.append('<tbody>')

    for idx, analysis in enumerate(analyses):
        num = analysis['number']
        title = analysis['title']
        url = analysis['url']
        age_days = analysis['age_days']
        prio = analysis['current_priority'] or 'not-set'
        final = analysis['final']
        verdict = final['verdict']
        criticality = final['criticality']
        subsystem = final.get('affected_subsystem', '')
        if not subsystem and analysis.get('area_labels'):
            subsystem = analysis['area_labels'][0].replace('area: ', '').replace('area:', '')

        detail_id = f'detail-{idx}'

        # Row data attributes used by JS filtering and sorting
        row_attrs = (
            f'data-num="{num}"'
            f' data-verdict="{esc(verdict)}"'
            f' data-crit="{esc(criticality)}"'
            f' data-prio="{esc(prio)}"'
            f' data-title="{esc(title.lower())}"'
            f' data-age="{age_days}"'
            f' data-detail="{detail_id}"'
        )

        # Subtle left-border color by criticality
        row_color = CRITICALITY_COLORS.get(criticality, '#9e9e9e')
        parts.append(
            f'<tr class="issue-row" {row_attrs}'
            f' style="border-left:4px solid {row_color}">'
        )
        # Issue number cell
        parts.append(
            f'<td><a href="{esc(url)}" target="_blank">#{num}</a></td>'
        )
        # Title (truncated)
        title_truncated = title[:90] + ('...' if len(title) > 90 else '')
        parts.append(f'<td>{esc(title_truncated)}</td>')
        # Age
        age_str = f'{age_days}d'
        parts.append(f'<td style="white-space:nowrap">{age_str}</td>')
        # Priority
        parts.append(f'<td>{_priority_badge(analysis["current_priority"])}</td>')
        # Criticality
        parts.append(f'<td>{_crit_badge(criticality)}</td>')
        # Verdict
        parts.append(f'<td>{_verdict_badge(verdict)}</td>')
        # Subsystem
        parts.append(f'<td style="font-size:.8rem;color:#555">{esc(subsystem[:40])}</td>')
        parts.append('</tr>')

        # Detail row (hidden by default)
        detail_html = _detail_row(analysis, idx)
        parts.append(
            f'<tr class="detail-row" id="{detail_id}" style="display:none">'
            f'<td colspan="7">{detail_html}</td>'
            f'</tr>'
        )

    if not analyses:
        parts.append(
            '<tr><td colspan="7" class="no-results">No issues found</td></tr>'
        )

    parts.append('</tbody></table></div>')  # end tbl-wrap

    # Footer
    parts.append(
        '<p style="margin:20px 0;color:#999;font-size:.8rem;text-align:center">'
        f'Zephyr Issue Triage Report &mdash; {esc(now_str)}'
        f' &mdash; {total} issues'
        f' &mdash; AI: {"on" if llm_count > 0 else "off"}'
        '</p>'
    )

    parts.append('</div>')  # end container
    parts.append(f'<script>{_JS}</script>')
    parts.append('</body></html>')

    return '\n'.join(parts)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args():
    parser = argparse.ArgumentParser(
        prog='issue_triage.py',
        description='Analyze open Zephyr bug issues for triage and generate an HTML report',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            'Examples:\n'
            '  GITHUB_TOKEN=ghp_xxx ./scripts/ci/issue_triage.py\n'
            '  GITHUB_TOKEN=ghp_xxx OPENROUTER_API_KEY=sk-or-... \\\n'
            '      ./scripts/ci/issue_triage.py --model anthropic/claude-opus-4\n'
            '  ./scripts/ci/issue_triage.py --issue 12345 --no-llm\n'
            '  ./scripts/ci/issue_triage.py --type Enhancement\n'
        ),
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
        help=f'GitHub repository (default: {GITHUB_REPO})',
    )
    parser.add_argument(
        '--type',
        default=DEFAULT_ISSUE_TYPE,
        metavar='TYPE',
        help=f'GitHub issue type to filter on (default: {DEFAULT_ISSUE_TYPE!r}). '
             'This uses the GitHub issue types feature, not labels.',
    )
    parser.add_argument(
        '--max-issues',
        type=int,
        default=DEFAULT_MAX_ISSUES,
        metavar='N',
        help=f'Maximum number of issues to analyze (default: {DEFAULT_MAX_ISSUES})',
    )
    parser.add_argument(
        '--days',
        type=int,
        default=0,
        metavar='N',
        help='Only analyze issues updated in the last N days (0 = no limit)',
    )
    parser.add_argument(
        '--issue',
        type=int,
        default=None,
        metavar='NUMBER',
        help='Analyze a single issue number (for debugging)',
    )
    parser.add_argument(
        '--model',
        default='gpt-4o-mini',
        metavar='MODEL',
        help='LLM model name (default: gpt-4o-mini). Examples: '
             'claude-opus-4, anthropic/claude-opus-4, '
             'google/gemini-pro, ollama/llama3',
    )
    parser.add_argument(
        '--provider',
        default=None,
        choices=['auto', 'openai', 'anthropic', 'litellm', 'openrouter'],
        metavar='PROVIDER',
        help='LLM provider: auto (default), openai, anthropic, litellm, openrouter. '
             'Overrides TRIAGE_PROVIDER env var.',
    )
    parser.add_argument(
        '--output',
        default=DEFAULT_OUTPUT,
        metavar='FILE',
        help=f'HTML report output path (default: {DEFAULT_OUTPUT})',
    )
    parser.add_argument(
        '--json',
        default=None,
        metavar='FILE',
        help='Also save raw JSON analysis to FILE',
    )
    parser.add_argument(
        '--no-llm',
        action='store_true',
        help='Disable LLM analysis entirely (heuristic only)',
    )
    parser.add_argument(
        '--debug',
        action='store_true',
        help='Enable DEBUG logging (shows raw LLM responses)',
    )
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Print per-issue verdict summary to stdout',
    )
    return parser.parse_args()


def _any_llm_available():
    """Return True if at least one LLM library and key are configured."""
    if HAS_OPENAI and os.environ.get('OPENAI_API_KEY'):
        return True
    if HAS_OPENAI and os.environ.get('OPENROUTER_API_KEY'):
        return True
    if HAS_ANTHROPIC and os.environ.get('ANTHROPIC_API_KEY'):
        return True
    if HAS_LITELLM:
        return True
    return False


def main():
    args = parse_args()

    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    # Print a summary of what we're about to do
    target = f'issue #{args.issue}' if args.issue else f'up to {args.max_issues} issues'
    print(
        f'Fetching {target} of type "{args.type}" from '
        f'{args.org}/{args.repo} ...',
        file=sys.stderr,
    )

    issues = fetch_issues(
        github_token=args.token,
        org=args.org,
        repo=args.repo,
        issue_type=args.type,
        max_issues=args.max_issues,
        days=args.days,
        issue_number=args.issue,
    )

    if not issues:
        print('No issues found matching the criteria.', file=sys.stderr)
        sys.exit(0)

    print(f'Found {len(issues)} issue(s). Running triage ...', file=sys.stderr)

    provider = None if args.no_llm else args.provider
    analyses = triage_issues(
        issues,
        llm_model=args.model,
        llm_provider=provider,
        no_llm=args.no_llm,
    )

    llm_count = sum(1 for a in analyses if a.get('llm'))
    if llm_count > 0:
        print(
            f'AI analysis complete: {llm_count}/{len(analyses)} issues '
            f'analyzed via LLM (provider: {args.provider or "auto"}, '
            f'model: {args.model})',
            file=sys.stderr,
        )
    elif args.no_llm:
        print('LLM analysis disabled (--no-llm). Heuristic only.', file=sys.stderr)
    elif not _any_llm_available():
        print(
            'LLM analysis skipped - install openai/anthropic/litellm and set an API key.',
            file=sys.stderr,
        )
    else:
        print('LLM analysis failed (see warnings above).', file=sys.stderr)

    if args.verbose:
        print('\nPer-issue summary:', file=sys.stdout)
        for a in analyses:
            final = a['final']
            print(
                f"  #{a['number']:6d}  "
                f"{final['criticality']:10s}  "
                f"{final['verdict']:20s}  "
                f"{a['title'][:60]}",
            )

    # Generate HTML report
    print(f'Generating HTML report -> {args.output} ...', file=sys.stderr)
    html_content = generate_html_report(analyses, args)
    with open(args.output, 'w', encoding='utf-8') as fh:
        fh.write(html_content)
    print(f'HTML report written to: {args.output}', file=sys.stderr)

    # Optional JSON export
    if args.json:
        with open(args.json, 'w', encoding='utf-8') as fh:
            json.dump(analyses, fh, indent=2, default=str)
        print(f'JSON data written to: {args.json}', file=sys.stderr)

    # Print a quick triage summary
    verdict_counts = {}
    for a in analyses:
        v = a['final']['verdict']
        verdict_counts[v] = verdict_counts.get(v, 0) + 1

    print('\nTriage summary:', file=sys.stderr)
    for v in VERDICTS:
        cnt = verdict_counts.get(v, 0)
        if cnt:
            print(f'  {VERDICT_LABELS[v]:20s}: {cnt}', file=sys.stderr)

    return 0


if __name__ == '__main__':
    sys.exit(main())
