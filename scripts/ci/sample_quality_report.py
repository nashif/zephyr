#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

"""
Sample Quality Report: Analyzes all Zephyr samples under samples/ and
produces a self-contained HTML report evaluating each sample against the
criteria defined in samples/sample_definition_and_criteria.rst.

Each sample is evaluated on:
  - Correctness: does it violate the no-testing rule (no ztest/zassert)?
  - Twister integration: sample.yaml present, integration_platforms set,
    harness configured, not marked build_only for every test case?
  - Documentation: README.rst present with required sections?
  - Code quality: source complexity appropriate for a sample?
  - Purpose clarity: what does the sample demonstrate?

Supported LLM providers (selected via --provider or SAMPLE_ADVISOR_PROVIDER):

  openai     - OpenAI models (gpt-4o, o3-mini, ...)
               Requires: pip install openai
               Env:      OPENAI_API_KEY, OPENAI_BASE_URL (optional)

  anthropic  - Anthropic Claude models (claude-opus-4, claude-sonnet-4-5, ...)
               Requires: pip install anthropic
               Env:      ANTHROPIC_API_KEY

  litellm    - Universal proxy; 100+ providers via a single interface.
               Requires: pip install litellm
               Env:      provider-specific

  openrouter - OpenRouter.ai aggregator: 200+ models via one API key.
               Requires: pip install openai
               Env:      OPENROUTER_API_KEY

  auto       - (default) Infer from model name and available packages.

Usage (heuristic only, no API key needed):
    ./scripts/ci/sample_quality_report.py --output report.html

Usage (with caching — fast on subsequent runs):
    ./scripts/ci/sample_quality_report.py \\
        --cache sample_quality_cache.json --output report.html

Usage (analyze specific subsystem):
    ./scripts/ci/sample_quality_report.py --subsystem bluetooth

Usage (with LLM analysis, limited to samples with issues):
    OPENROUTER_API_KEY=sk-or-... \\
        ./scripts/ci/sample_quality_report.py \\
        --model anthropic/claude-opus-4 \\
        --max-llm 50 \\
        --llm-non-compliant-only \\
        --cache sample_quality_cache.json \\
        --output report.html

Usage (analyze a single sample for debugging):
    ./scripts/ci/sample_quality_report.py \\
        --sample samples/hello_world --no-llm

Environment variables:
    SAMPLE_ADVISOR_PROVIDER  Default LLM provider (overridden by --provider)
    OPENAI_API_KEY           OpenAI / compatible key
    OPENAI_BASE_URL          Optional base URL for OpenAI-compatible endpoints
    ANTHROPIC_API_KEY        Anthropic API key
    OPENROUTER_API_KEY       OpenRouter.ai API key
    OPENROUTER_SITE_URL      Optional: site URL shown in or.ai dashboard
    OPENROUTER_SITE_NAME     Optional: app name shown in or.ai dashboard

Requirements:
    pip install pyyaml
    pip install openai      # for openai and openrouter providers
    pip install anthropic   # for anthropic provider
    pip install litellm     # for litellm provider

Output:
    Generates a self-contained HTML report (--output, default:
    sample_quality_report.html) and optionally a raw JSON file (--json-out).
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
SAMPLES_DIR = ZEPHYR_BASE / 'samples'

OPENROUTER_BASE_URL = 'https://openrouter.ai/api/v1'

_ANTHROPIC_PREFIXES = ('claude-',)
_OPENAI_PREFIXES = ('gpt-', 'o1-', 'o3-', 'o4-', 'text-davinci')

DEFAULT_OUTPUT = 'sample_quality_report.html'
DEFAULT_CACHE = 'sample_quality_cache.json'

# Cache version — increment when the cache schema changes incompatibly.
CACHE_VERSION = 1

# ---------------------------------------------------------------------------
# Git helpers
# ---------------------------------------------------------------------------

def _build_sample_sha_map():
    """
    Build a {rel_path: tree_sha} mapping for every directory under samples/
    in a single git call.

    Uses ``git ls-tree -r -d HEAD -- samples/`` which outputs one line per
    directory object::

        <mode> tree <sha>\t<path>

    The tree SHA for a directory changes whenever any file inside it changes,
    so it serves as a cheap content fingerprint without traversing commit
    history.

    Returns an empty dict when git is unavailable or the tree cannot be read.
    """
    try:
        result = subprocess.run(
            ['git', '-C', str(ZEPHYR_BASE), 'ls-tree', '-r', '-d',
             'HEAD', '--', 'samples/'],
            capture_output=True, text=True, timeout=15,
        )
        sha_map = {}
        for line in result.stdout.splitlines():
            line = line.strip()
            if not line:
                continue
            # format: "<mode> tree <sha>\t<path>"
            try:
                meta, path = line.split('\t', 1)
                sha = meta.split()[2]
                sha_map[path] = sha
            except (ValueError, IndexError):
                continue
        return sha_map
    except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
        return {}


# ---------------------------------------------------------------------------
# Cache management
# ---------------------------------------------------------------------------

def load_cache(cache_path):
    """
    Load the sample quality cache from a JSON file.

    Cache format::

        {
            "version": 1,
            "entries": {
                "samples/hello_world": {
                    "git_sha":     "<last-commit-sha-for-path>",
                    "analyzed_at": "<ISO datetime>",
                    "sample":      { ... serialisable sample metadata ... },
                    "heuristic":   { ... heuristic result ... },
                    "verdict":     "compliant",
                    "llm": {
                        "<model-name>": { ... llm result ... }
                    }
                }, ...
            }
        }

    Returns a dict {path: entry} or empty dict on any error.
    """
    if not cache_path or not Path(cache_path).exists():
        return {}
    try:
        with open(cache_path, encoding='utf-8') as fh:
            data = json.load(fh)
        if not isinstance(data, dict):
            return {}
        if data.get('version') != CACHE_VERSION:
            log.debug('Cache version mismatch (%s); discarding.',
                      data.get('version'))
            return {}
        entries = data.get('entries', {})
        log.debug('Loaded cache from %s (%d entries)', cache_path, len(entries))
        return entries
    except Exception as exc:
        log.warning('Could not load cache %s: %s', cache_path, exc)
        return {}


def save_cache(cache_path, entries):
    """
    Persist the sample quality cache to a JSON file atomically.

    Writes to a .tmp file first then renames to avoid corruption on
    interrupted runs.
    """
    if not cache_path:
        return
    try:
        data = {'version': CACHE_VERSION, 'entries': entries}
        tmp = cache_path + '.tmp'
        with open(tmp, 'w', encoding='utf-8') as fh:
            json.dump(data, fh, indent=2, default=str)
        os.replace(tmp, cache_path)
        log.debug('Cache saved to %s (%d entries)', cache_path, len(entries))
    except Exception as exc:
        log.warning('Could not save cache %s: %s', cache_path, exc)


def _cache_entry_from_analysis(analysis, git_sha):
    """
    Build a serialisable cache entry from a completed analysis dict.

    The full source file contents are excluded to keep the cache compact;
    only the fields needed to reconstruct the display are stored.
    """
    sd = analysis['sample']
    # Keep display-relevant fields; drop bulky raw source arrays
    sample_meta = {
        'path':             sd['path'],
        'name':             sd.get('name', ''),
        'description':      sd.get('description', ''),
        'subsystem':        sd['subsystem'],
        'yaml_name':        sd.get('yaml_name'),
        'yaml_data':        sd.get('yaml_data', {}),
        'loc':              sd['loc'],
        'source_count':     sd['source_count'],
        'has_readme':       sd['has_readme'],
        'has_prj_conf':     sd['has_prj_conf'],
        'has_cmake':        sd['has_cmake'],
        # Keep only the first 700 chars for the HTML README excerpt
        'readme_content':   sd.get('readme_content', '')[:700],
        'prj_conf_content': sd.get('prj_conf_content', '')[:200],
        # Mark that sources are not available (needed by LLM builder)
        'sources':          [],
    }
    # LLM results keyed by model name so different models are cached separately
    llm_by_model = {}
    llm_result = analysis.get('llm')
    llm_model  = analysis.get('_llm_model', '')
    if llm_result and llm_model:
        llm_by_model[llm_model] = llm_result

    return {
        'git_sha':     git_sha,
        'analyzed_at': datetime.datetime.now().isoformat(),
        'sample':      sample_meta,
        'heuristic':   analysis['heuristic'],
        'verdict':     analysis['verdict'],
        'llm':         llm_by_model,
    }


def _analysis_from_cache(entry, llm_model=None):
    """
    Reconstruct an analysis dict from a cache entry.

    llm_model: if provided, look up cached LLM results for that model.
    Returns the analysis dict with _from_cache=True.
    """
    llm_result = None
    if llm_model:
        llm_result = entry.get('llm', {}).get(llm_model)

    # Determine final verdict (may need upgrade if LLM is now available)
    verdict = entry['verdict']
    if llm_result and llm_result.get('verdict') in VERDICTS:
        llm_v_idx = VERDICTS.index(llm_result['verdict'])
        h_v_idx   = VERDICTS.index(entry['heuristic']['verdict'])
        if llm_v_idx > h_v_idx:
            verdict = llm_result['verdict']

    return {
        'sample':      entry['sample'],
        'heuristic':   entry['heuristic'],
        'llm':         llm_result,
        'verdict':     verdict,
        '_from_cache': True,
        '_llm_model':  llm_model or '',
    }


# ---------------------------------------------------------------------------
# Verdict levels (ascending severity of issues)
# ---------------------------------------------------------------------------

VERDICTS = ['compliant', 'minor-issues', 'major-issues', 'non-compliant']

VERDICT_COLORS = {
    'compliant':     '#43a047',
    'minor-issues':  '#f57c00',
    'major-issues':  '#e53935',
    'non-compliant': '#b71c1c',
}

VERDICT_LABELS = {
    'compliant':     'Compliant',
    'minor-issues':  'Minor Issues',
    'major-issues':  'Major Issues',
    'non-compliant': 'Non-Compliant',
}

# ---------------------------------------------------------------------------
# Issue types: (severity, human label)
# Severity: critical -> non-compliant; major -> major-issues; minor -> minor-issues
# ---------------------------------------------------------------------------

ISSUE_DEFS = {
    'uses_ztest': (
        'critical',
        'Uses Ztest framework (CONFIG_ZTEST or #include <ztest.h>) — '
        'forbidden in samples per criterion 1',
    ),
    'uses_zassert': (
        'critical',
        'Uses zassert_* macros — forbidden in samples per criterion 1',
    ),
    'no_sample_yaml': (
        'major',
        'No sample.yaml, tests.yaml, or testcase.yaml found',
    ),
    'wrong_yaml_name': (
        'major',
        'Uses testcase.yaml instead of sample.yaml — wrong naming for a sample',
    ),
    'no_readme': (
        'major',
        'No README.rst found — required by criterion 4',
    ),
    'readme_no_overview': (
        'minor',
        'README lacks an Overview or introductory section',
    ),
    'readme_no_building': (
        'minor',
        'README lacks Building & Running instructions',
    ),
    'readme_no_output': (
        'minor',
        'README lacks Sample Output section (recommended when output is expected)',
    ),
    'no_prj_conf': (
        'major',
        'No prj.conf found — base configuration must be in prj.conf per criterion 2',
    ),
    'build_only_all': (
        'major',
        'All test cases are marked build_only — samples must be able to run, not just build',
    ),
    'no_integration_platforms': (
        'minor',
        'No integration_platforms defined — recommended to limit CI scope',
    ),
    'no_harness': (
        'minor',
        'No harness configured — cannot verify sample produces expected output',
    ),
    'skip_all': (
        'major',
        'All test cases have skip: true — sample cannot be validated by CI',
    ),
    'very_large_source': (
        'minor',
        'Source code exceeds 1 000 lines — may be too complex for a reference sample',
    ),
    'no_cmake': (
        'major',
        'No CMakeLists.txt found',
    ),
    'missing_sample_section': (
        'minor',
        'sample.yaml lacks a top-level "sample:" section with name/description',
    ),
}

SEVERITY_ORDER = ['critical', 'major', 'minor']

# ---------------------------------------------------------------------------
# Subsystem mapping: path prefix -> label
# ---------------------------------------------------------------------------

PATH_TO_SUBSYSTEM = [
    ('samples/bluetooth/',              'Bluetooth'),
    ('samples/net/',                    'Networking'),
    ('samples/kernel/',                 'Kernel'),
    ('samples/drivers/',                'Drivers'),
    ('samples/subsys/bluetooth/',       'Subsys / Bluetooth'),
    ('samples/subsys/net/',             'Subsys / Networking'),
    ('samples/subsys/fs/',              'Subsys / Filesystem'),
    ('samples/subsys/usb/',             'Subsys / USB'),
    ('samples/subsys/mgmt/',            'Subsys / Mgmt'),
    ('samples/subsys/shell/',           'Subsys / Shell'),
    ('samples/subsys/logging/',         'Subsys / Logging'),
    ('samples/subsys/',                 'Subsystems'),
    ('samples/boards/',                 'Boards'),
    ('samples/basic/',                  'Basic'),
    ('samples/cpp/',                    'C++'),
    ('samples/arch/',                   'Architecture'),
    ('samples/modules/',                'Modules'),
    ('samples/psa/',                    'PSA'),
    ('samples/tfm_integration/',        'TF-M Integration'),
    ('samples/sensor/',                 'Sensor'),
    ('samples/shields/',                'Shields'),
    ('samples/userspace/',              'Userspace'),
    ('samples/sysbuild/',               'Sysbuild'),
    ('samples/data_structures/',        'Data Structures'),
    ('samples/application_development/', 'Application Dev'),
    ('samples/regulator/',              'Regulator'),
    ('samples/philosophers/',           'Kernel'),
    ('samples/hello_world/',            'Basic'),
    ('samples/synchronization/',        'Kernel'),
]

KNOWN_SUBSYSTEMS = sorted(set(label for _, label in PATH_TO_SUBSYSTEM))


# ---------------------------------------------------------------------------
# Sample discovery
# ---------------------------------------------------------------------------

def _is_sample_dir(path):
    """
    Return True if path contains a sample YAML descriptor.
    A sample directory is identified by the presence of sample.yaml,
    tests.yaml, or testcase.yaml.
    """
    for name in ('sample.yaml', 'tests.yaml', 'testcase.yaml'):
        if (path / name).exists():
            return True
    return False


def discover_samples(samples_dir=SAMPLES_DIR, subsystem_filter=None,
                     single_sample=None):
    """
    Walk the samples/ tree and return a list of Path objects, each pointing
    to a sample directory (one that contains a YAML descriptor).

    subsystem_filter: if set, restrict to samples whose path contains this
    string (case-insensitive).

    single_sample: if set, must be a path string; only that sample is returned.
    """
    if single_sample:
        p = Path(single_sample)
        if not p.is_absolute():
            p = ZEPHYR_BASE / p
        if _is_sample_dir(p):
            return [p]
        log.warning('Path %s is not a sample directory (no YAML descriptor)', p)
        return []

    found = []
    for dirpath in sorted(samples_dir.rglob('*')):
        if not dirpath.is_dir():
            continue
        if _is_sample_dir(dirpath):
            found.append(dirpath)

    if subsystem_filter:
        low = subsystem_filter.lower()
        found = [p for p in found if low in str(p).lower()]

    return found


# ---------------------------------------------------------------------------
# File reading helpers
# ---------------------------------------------------------------------------

def _read_text(path, max_bytes=65536):
    """Read a text file safely; return empty string on error."""
    try:
        return path.read_text(encoding='utf-8', errors='replace')[:max_bytes]
    except OSError:
        return ''


def _load_yaml(path):
    """Load YAML safely; return empty dict on error."""
    if not HAS_YAML:
        return {}
    try:
        with open(path, encoding='utf-8') as fh:
            return yaml.load(fh, Loader=SafeLoader) or {}
    except Exception as exc:
        log.debug('Could not load %s: %s', path, exc)
        return {}


def collect_source_files(sample_dir):
    """
    Return a list of (relative_path, content) tuples for all .c and .h
    files under the sample directory.  Content is capped at 8 KB per file.
    """
    sources = []
    for ext in ('*.c', '*.h', '*.cpp', '*.cxx'):
        for p in sorted(sample_dir.rglob(ext)):
            content = _read_text(p, max_bytes=8192)
            rel = str(p.relative_to(ZEPHYR_BASE))
            sources.append((rel, content))
    return sources


def count_loc(sources):
    """Count non-empty, non-comment source lines across all source files."""
    total = 0
    for _, content in sources:
        for line in content.splitlines():
            stripped = line.strip()
            if stripped and not stripped.startswith('//') and not stripped.startswith('*'):
                total += 1
    return total


# ---------------------------------------------------------------------------
# Heuristic checks
# ---------------------------------------------------------------------------

_ZTEST_INCLUDE_RE = re.compile(r'#\s*include\s+[<"]ztest(?:_test)?\.h[>"]')
_ZTEST_CONFIG_RE = re.compile(r'CONFIG_ZTEST\s*=\s*y', re.IGNORECASE)
_ZASSERT_RE = re.compile(r'\bzassert_\w+\s*\(')
_ZTEST_SUITE_RE = re.compile(r'\bZTEST_SUITE\b|\bZTEST\b|\bztest_run_test_suite\b')

_README_OVERVIEW_RE = re.compile(
    r'^(Overview|Introduction|Description|About)\s*\n[*#=-]+',
    re.MULTILINE | re.IGNORECASE,
)
_README_BUILDING_RE = re.compile(
    r'^(Building|Build(ing)?\s+(and\s+)?Runn?ing|How\s+to\s+(build|run))\s*\n[*#=-]+',
    re.MULTILINE | re.IGNORECASE,
)
_README_OUTPUT_RE = re.compile(
    r'^(Sample\s+Output|Expected\s+Output|Output|Console\s+Output)\s*\n[*#=-]+',
    re.MULTILINE | re.IGNORECASE,
)


def _check_source_for_ztest(sources):
    """
    Inspect source files for ztest imports and zassert calls.
    Returns (uses_ztest: bool, uses_zassert: bool, offending_files: list).
    """
    uses_ztest = False
    uses_zassert = False
    offenders = []

    for rel, content in sources:
        file_zt = bool(
            _ZTEST_INCLUDE_RE.search(content) or _ZTEST_SUITE_RE.search(content)
        )
        file_za = bool(_ZASSERT_RE.search(content))
        if file_zt or file_za:
            offenders.append(rel)
        uses_ztest = uses_ztest or file_zt
        uses_zassert = uses_zassert or file_za

    return uses_ztest, uses_zassert, offenders


def _check_yaml_compliance(yaml_data, yaml_name):
    """
    Check the YAML descriptor for compliance issues.

    Returns a dict of issue keys mapped to detail strings.
    """
    issues = {}

    if yaml_name == 'testcase.yaml':
        issues['wrong_yaml_name'] = (
            'testcase.yaml should be renamed to sample.yaml for samples'
        )

    tests = yaml_data.get('tests') or {}
    if not isinstance(tests, dict):
        tests = {}

    if not tests:
        return issues

    # Check if the top-level 'sample:' section exists
    if 'sample' not in yaml_data:
        issues['missing_sample_section'] = (
            'No top-level "sample:" block with name/description found'
        )

    # Check build_only across all test cases
    all_build_only = all(
        tc.get('build_only', False)
        for tc in tests.values()
        if isinstance(tc, dict)
    )
    if all_build_only and len(tests) > 0:
        issues['build_only_all'] = (
            f'All {len(tests)} test case(s) are build_only'
        )

    # Check skip: true across all test cases
    all_skip = all(
        tc.get('skip', False)
        for tc in tests.values()
        if isinstance(tc, dict)
    )
    if all_skip and len(tests) > 0:
        issues['skip_all'] = f'All {len(tests)} test case(s) have skip: true'

    # Check integration_platforms (at common or individual test level)
    common = yaml_data.get('common') or {}
    has_integration = bool(common.get('integration_platforms'))
    if not has_integration:
        for tc in tests.values():
            if isinstance(tc, dict) and tc.get('integration_platforms'):
                has_integration = True
                break
    if not has_integration:
        issues['no_integration_platforms'] = (
            'No integration_platforms defined in common or any test case'
        )

    # Check harness (at common or individual test level)
    has_harness = bool(common.get('harness'))
    if not has_harness:
        for tc in tests.values():
            if isinstance(tc, dict) and tc.get('harness'):
                has_harness = True
                break
    if not has_harness:
        # build_only-only samples legitimately have no harness
        if not all_build_only:
            issues['no_harness'] = (
                'No harness set; sample output cannot be verified by twister'
            )

    return issues


def _check_readme(readme_content):
    """
    Check README content for required sections.
    Returns a dict of issue keys mapped to detail strings.
    """
    issues = {}

    if not readme_content.strip():
        return issues  # No README at all is handled at a higher level

    # Zephyr-style sample docs use * and # underlines for RST headings
    has_overview = bool(_README_OVERVIEW_RE.search(readme_content))
    has_building = bool(_README_BUILDING_RE.search(readme_content))
    has_output = bool(_README_OUTPUT_RE.search(readme_content))

    if not has_overview:
        # Also accept any RST section heading near the top
        lines = readme_content.splitlines()
        first_sections = [
            l.strip() for l in lines[:30]
            if re.match(r'^[*#=\-^"]{4,}$', l.strip())
        ]
        if not first_sections:
            issues['readme_no_overview'] = (
                'README does not appear to have an Overview/Introduction section'
            )

    if not has_building:
        if 'build' not in readme_content.lower() and 'run' not in readme_content.lower():
            issues['readme_no_building'] = (
                'README does not describe how to build or run the sample'
            )

    if not has_output:
        # Only flag missing output if the README mentions "output" or "console"
        body_lower = readme_content.lower()
        if 'output' in body_lower or 'console' in body_lower or 'print' in body_lower:
            if not has_output:
                issues['readme_no_output'] = (
                    'README mentions output but lacks a dedicated Sample Output section'
                )

    return issues


def _extract_purpose_from_readme(readme_content):
    """
    Extract a brief purpose description from the README:
    the first non-heading, non-directive paragraph of text.
    """
    if not readme_content:
        return ''

    lines = readme_content.splitlines()
    in_directive = False
    text_lines = []

    for line in lines:
        stripped = line.strip()
        # Skip RST directives and their content
        if stripped.startswith('.. '):
            in_directive = True
            continue
        if in_directive:
            if stripped == '' or line.startswith(' ') or line.startswith('\t'):
                continue
            in_directive = False
        # Skip heading underlines
        if re.match(r'^[*#=\-^"]{4,}$', stripped):
            if text_lines:
                break
            continue
        # Skip heading text (lines before underlines) — heuristic: short line
        if stripped and len(stripped) < 80 and not text_lines:
            # peek: next non-empty line is underline?
            pass
        if stripped:
            text_lines.append(stripped)
            if len(' '.join(text_lines)) > 300:
                break
        elif text_lines:
            break

    purpose = ' '.join(text_lines)[:400]
    return purpose


def _infer_subsystem(sample_path):
    """Infer a subsystem label from the sample's path."""
    rel = str(sample_path.relative_to(ZEPHYR_BASE)).replace('\\', '/')
    for prefix, label in PATH_TO_SUBSYSTEM:
        if rel.startswith(prefix):
            return label
    return 'Other'


# ---------------------------------------------------------------------------
# Full heuristic analysis for a single sample
# ---------------------------------------------------------------------------

def read_sample_data(sample_dir):
    """
    Read all relevant files for a sample and return a metadata dict.
    """
    rel_path = str(sample_dir.relative_to(ZEPHYR_BASE))

    # Detect YAML descriptor
    yaml_name = None
    yaml_data = {}
    for name in ('sample.yaml', 'tests.yaml', 'testcase.yaml'):
        yp = sample_dir / name
        if yp.exists():
            yaml_name = name
            yaml_data = _load_yaml(yp)
            break

    readme_content = _read_text(sample_dir / 'README.rst')
    prj_conf_content = _read_text(sample_dir / 'prj.conf')
    cmake_exists = (sample_dir / 'CMakeLists.txt').exists()
    sources = collect_source_files(sample_dir)
    loc = count_loc(sources)

    # Extract sample name from YAML
    sample_meta = yaml_data.get('sample') or {}
    sample_name = sample_meta.get('name', '')
    sample_description = sample_meta.get('description', '')

    return {
        'path':               rel_path,
        'name':               sample_name,
        'description':        sample_description,
        'subsystem':          _infer_subsystem(sample_dir),
        'yaml_name':          yaml_name,
        'yaml_data':          yaml_data,
        'has_readme':         bool(readme_content.strip()),
        'readme_content':     readme_content,
        'has_prj_conf':       bool(prj_conf_content.strip()),
        'prj_conf_content':   prj_conf_content,
        'has_cmake':          cmake_exists,
        'sources':            sources,
        'source_count':       len(sources),
        'loc':                loc,
    }


def heuristic_analyze(sample_data):
    """
    Run all heuristic checks against a sample's collected data.

    Returns a dict with:
      issues:   {issue_key: detail_string}
      verdict:  'compliant' | 'minor-issues' | 'major-issues' | 'non-compliant'
      purpose:  short description extracted from README
      signals:  list of plain-text signal strings for the report
    """
    issues = {}
    signals = []

    # --- Check 1: no ztest usage ---
    uses_zt, uses_za, zt_offenders = _check_source_for_ztest(sample_data['sources'])

    # Also check prj.conf for CONFIG_ZTEST
    if _ZTEST_CONFIG_RE.search(sample_data['prj_conf_content']):
        uses_zt = True

    if uses_zt:
        issues['uses_ztest'] = (
            'ztest framework found in: '
            + (', '.join(zt_offenders[:3]) or 'prj.conf')
        )
    if uses_za:
        issues['uses_zassert'] = (
            'zassert macros found in: ' + ', '.join(zt_offenders[:3])
        )

    # --- Check 2: YAML descriptor ---
    if not sample_data['yaml_name']:
        issues['no_sample_yaml'] = 'No YAML descriptor found in sample directory'
    else:
        yaml_issues = _check_yaml_compliance(
            sample_data['yaml_data'], sample_data['yaml_name']
        )
        issues.update(yaml_issues)

    # --- Check 3: prj.conf ---
    if not sample_data['has_prj_conf']:
        issues['no_prj_conf'] = 'prj.conf is absent'

    # --- Check 4: CMakeLists.txt ---
    if not sample_data['has_cmake']:
        issues['no_cmake'] = 'CMakeLists.txt is absent'

    # --- Check 5: README ---
    if not sample_data['has_readme']:
        issues['no_readme'] = 'README.rst is absent'
    else:
        readme_issues = _check_readme(sample_data['readme_content'])
        issues.update(readme_issues)

    # --- Check 6: source complexity ---
    if sample_data['loc'] > 1000:
        issues['very_large_source'] = (
            f'Total source LOC = {sample_data["loc"]} '
            '(>1000 may indicate test-like complexity)'
        )

    # --- Determine verdict ---
    has_critical = any(
        ISSUE_DEFS.get(k, ('minor', ''))[0] == 'critical'
        for k in issues
    )
    has_major = any(
        ISSUE_DEFS.get(k, ('minor', ''))[0] == 'major'
        for k in issues
    )
    has_minor = any(
        ISSUE_DEFS.get(k, ('minor', ''))[0] == 'minor'
        for k in issues
    )

    if has_critical:
        verdict = 'non-compliant'
    elif has_major:
        verdict = 'major-issues'
    elif has_minor:
        verdict = 'minor-issues'
    else:
        verdict = 'compliant'

    # --- Build signals list ---
    if not issues:
        signals.append('Passes all heuristic checks')
    for key, detail in issues.items():
        sev, label = ISSUE_DEFS.get(key, ('minor', key))
        signals.append(f'[{sev.upper()}] {label}')

    # Positive signals
    if sample_data['yaml_data'].get('sample', {}).get('name'):
        signals.append(
            f'Sample name: "{sample_data["yaml_data"]["sample"]["name"]}"'
        )
    if sample_data['loc'] and sample_data['loc'] <= 300:
        signals.append(f'Compact source: {sample_data["loc"]} LOC (good for a sample)')
    elif sample_data['loc']:
        signals.append(f'Source size: {sample_data["loc"]} LOC')

    # --- Extract purpose ---
    purpose = _extract_purpose_from_readme(sample_data['readme_content'])
    if not purpose and sample_data.get('description'):
        purpose = sample_data['description']
    if not purpose and sample_data['yaml_data'].get('sample', {}).get('description'):
        purpose = sample_data['yaml_data']['sample']['description']
    if not purpose:
        purpose = '(no description found)'

    return {
        'issues':   issues,
        'verdict':  verdict,
        'purpose':  purpose,
        'signals':  signals,
    }


# ---------------------------------------------------------------------------
# LLM provider abstraction
# ---------------------------------------------------------------------------

def _resolve_provider(model, provider_hint):
    """
    Determine which LLM provider to use.  Mirrors the resolution logic in
    issue_triage.py and pr_test_advisor.py.
    """
    hint = provider_hint or os.environ.get('SAMPLE_ADVISOR_PROVIDER', 'auto')

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
    Extract a JSON object from raw LLM output.
    Tries: direct parse, strip code fences, regex search for {...}.
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
    """Call an OpenAI or OpenAI-compatible endpoint.  Returns raw text."""
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
            {'role': 'user',   'content': user},
        ],
        response_format={'type': 'json_object'},
        temperature=0.2,
        max_tokens=1500,
    )
    return response.choices[0].message.content


def _call_anthropic(model, system, user):
    """Call an Anthropic Claude model.  Returns raw text."""
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
    """Call any model supported by litellm.  Returns raw text."""
    response = litellm.completion(
        model=model,
        messages=[
            {'role': 'system', 'content': system},
            {'role': 'user',   'content': user},
        ],
        temperature=0.2,
        max_tokens=1500,
    )
    return response.choices[0].message.content


def _call_openrouter(model, system, user):
    """
    Call OpenRouter.ai using the OpenAI-compatible endpoint.
    response_format is intentionally omitted to support all proxied backends.
    """
    api_key = os.environ.get('OPENROUTER_API_KEY')
    if not api_key:
        raise RuntimeError('OPENROUTER_API_KEY is not set')
    extra_headers = {}
    site_url = os.environ.get('OPENROUTER_SITE_URL', '')
    site_name = os.environ.get('OPENROUTER_SITE_NAME', 'Zephyr Sample Quality Report')
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
            {'role': 'user',   'content': user},
        ],
        temperature=0.2,
        max_tokens=1500,
    )
    content = response.choices[0].message.content
    finish_reason = response.choices[0].finish_reason
    if not content or not content.strip():
        raise json.JSONDecodeError(
            f'Empty response from OpenRouter (finish_reason={finish_reason!r})',
            '', 0,
        )
    return content


# ---------------------------------------------------------------------------
# LLM system and user prompts
# ---------------------------------------------------------------------------

LLM_SYSTEM_PROMPT = """\
You are an expert Zephyr RTOS developer and documentation reviewer.
Analyze the Zephyr sample described below and respond with ONLY a JSON object.

IMPORTANT: Your entire response must be a single valid JSON object.
Do NOT wrap it in ```json fences. Do NOT add any text before or after.

The JSON must have exactly these fields:
{
  "purpose": "<1-2 sentences describing what feature/subsystem this sample demonstrates>",
  "usefulness": "<excellent|good|adequate|poor>",
  "usefulness_rationale": "<1-2 sentences: does the sample clearly illustrate non-trivial usage?>",
  "doc_quality": "<excellent|good|adequate|poor|missing>",
  "doc_quality_rationale": "<1-2 sentences: is the README helpful, accurate, complete?>",
  "code_quality": "<excellent|good|adequate|poor>",
  "code_quality_rationale": "<1-2 sentences: is the code clean, readable, free of test anti-patterns?>",
  "is_actually_a_test": <true|false>,
  "test_rationale": "<if true, explain why this looks like a test rather than a sample; else empty string>",
  "has_testing_overhead": <true|false>,
  "overhead_rationale": "<if true, describe what makes it overly complex for a sample; else empty string>",
  "additional_issues": ["<issue description>", ...],
  "recommendations": ["<actionable recommendation>", ...],
  "verdict": "<compliant|minor-issues|major-issues|non-compliant>"
}

Evaluation criteria (from samples/sample_definition_and_criteria.rst):
1. Samples must NOT use the Ztest framework or zassert macros.
2. Samples must have a sample.yaml descriptor and be runnable by twister.
3. Samples should demonstrate real, non-trivial feature usage — not unit tests or edge cases.
4. Samples must have a README.rst with Overview, Building & Running, and Sample Output.
5. Code should be concise and readable as a reference; not a production application.

Verdict mapping:
  compliant     - meets all criteria well
  minor-issues  - mostly fine but 1-2 small documentation or configuration gaps
  major-issues  - missing key requirements (no README, build_only, etc.)
  non-compliant - violates fundamental rules (uses ztest, no YAML, etc.)
"""

LLM_USER_PROMPT_TMPL = """\
Sample path: {path}
Sample name: {name}
Subsystem: {subsystem}
Source files: {source_count} file(s), {loc} LOC
YAML descriptor: {yaml_name}

Heuristic issues found:
{heuristic_issues}

README.rst (first 2500 chars):
{readme}

sample.yaml / tests.yaml content:
{yaml_content}

prj.conf content:
{prj_conf}

Main source file (first 3000 chars):
{main_source}

Please analyze this sample and return the JSON.
"""


def _build_user_prompt(sample_data, heuristic):
    """Build the LLM user message from collected sample data."""
    issues_text = '\n'.join(
        f'  [{ISSUE_DEFS.get(k, ("minor", k))[0].upper()}] {v}'
        for k, v in heuristic['issues'].items()
    ) or '  (no heuristic issues found)'

    # Pick the most substantial source file as "main source"
    main_source = ''
    if sample_data['sources']:
        biggest = max(sample_data['sources'], key=lambda x: len(x[1]))
        main_source = biggest[1][:3000]

    yaml_content = ''
    if sample_data['yaml_name']:
        yp = ZEPHYR_BASE / sample_data['path'] / sample_data['yaml_name']
        yaml_content = _read_text(yp, max_bytes=3000)

    return LLM_USER_PROMPT_TMPL.format(
        path=sample_data['path'],
        name=sample_data.get('name') or '(unnamed)',
        subsystem=sample_data['subsystem'],
        source_count=sample_data['source_count'],
        loc=sample_data['loc'],
        yaml_name=sample_data['yaml_name'] or '(none)',
        heuristic_issues=issues_text,
        readme=sample_data['readme_content'][:2500] or '(no README)',
        yaml_content=yaml_content or '(not found)',
        prj_conf=sample_data['prj_conf_content'][:1000] or '(not found)',
        main_source=main_source or '(no source files)',
    )


def llm_analyze_sample(sample_data, heuristic, model, provider=None):
    """
    Call an LLM to analyze a single sample.

    Returns the parsed JSON dict, or None on failure.
    """
    resolved = _resolve_provider(model, provider)
    if resolved is None:
        return None

    user_content = _build_user_prompt(sample_data, heuristic)

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

    log.debug('LLM analysis: %s  provider=%s  model=%s',
              sample_data['path'], resolved, model)
    try:
        raw = backend_fn(model, LLM_SYSTEM_PROMPT, user_content)
        log.debug('Raw LLM response: %s', raw[:500])
        return _extract_json(raw)
    except json.JSONDecodeError as exc:
        log.warning('LLM JSON parse failed for %s: %s', sample_data['path'], exc)
        return None
    except Exception as exc:
        log.warning('LLM call failed for %s: %s', sample_data['path'], exc)
        return None


# ---------------------------------------------------------------------------
# Combined analysis pipeline
# ---------------------------------------------------------------------------

def analyze_sample(sample_dir, model='gpt-4o-mini', provider=None,
                   run_llm=False):
    """
    Run heuristic (and optionally LLM) analysis on a single sample directory.

    Returns a comprehensive analysis dict.
    """
    sample_data = read_sample_data(sample_dir)
    heuristic = heuristic_analyze(sample_data)

    llm_result = None
    if run_llm:
        llm_result = llm_analyze_sample(sample_data, heuristic, model, provider)

    # Merge verdict: take the more severe of heuristic and LLM
    final_verdict = heuristic['verdict']
    if llm_result and llm_result.get('verdict') in VERDICTS:
        llm_v_idx = VERDICTS.index(llm_result['verdict'])
        h_v_idx = VERDICTS.index(heuristic['verdict'])
        if llm_v_idx > h_v_idx:
            final_verdict = llm_result['verdict']

    return {
        'sample':      sample_data,
        'heuristic':   heuristic,
        'llm':         llm_result,
        'verdict':     final_verdict,
        '_from_cache': False,
        '_llm_model':  model if run_llm and llm_result else '',
    }


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
.container{max-width:1500px;margin:0 auto;padding:16px 24px}
.stats-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));
  gap:12px;margin:16px 0}
.stat-card{background:#fff;border-radius:8px;padding:14px 16px;
  box-shadow:0 1px 3px rgba(0,0,0,.12);border-left:4px solid #ccc}
.stat-card .val{font-size:1.8rem;font-weight:700;line-height:1}
.stat-card .lbl{font-size:.75rem;color:#666;margin-top:4px;text-transform:uppercase;
  letter-spacing:.5px}
.filters{background:#fff;border-radius:8px;padding:14px 20px;margin-bottom:12px;
  box-shadow:0 1px 3px rgba(0,0,0,.1);display:flex;flex-wrap:wrap;gap:12px;
  align-items:center}
.filters label{font-size:.85rem;color:#555;display:flex;align-items:center;gap:6px}
.filters select,.filters input[type=text]{border:1px solid #ccc;border-radius:4px;
  padding:4px 8px;font-size:.85rem;outline:none}
.filters select:focus,.filters input[type=text]:focus{border-color:#1976d2}
.filters button{padding:5px 14px;border:1px solid #ccc;border-radius:4px;
  background:#fff;cursor:pointer;font-size:.85rem;color:#555}
.filters button:hover{background:#f5f5f5}
.filter-count{margin-left:auto;font-size:.85rem;color:#666}
.tbl-wrap{background:#fff;border-radius:8px;overflow:hidden;
  box-shadow:0 1px 3px rgba(0,0,0,.1)}
table{width:100%;border-collapse:collapse}
thead th{background:#263238;color:#eceff1;padding:10px 12px;text-align:left;
  font-size:.8rem;text-transform:uppercase;letter-spacing:.5px;
  cursor:pointer;white-space:nowrap;user-select:none}
thead th:hover{background:#37474f}
thead th.sorted-asc::after{content:' \u25b2'}
thead th.sorted-desc::after{content:' \u25bc'}
tbody tr.sample-row{cursor:pointer;transition:background .15s}
tbody tr.sample-row:hover{background:#e3f2fd}
tbody tr.sample-row.expanded{background:#e8eaf6}
tbody td{padding:8px 12px;border-bottom:1px solid #eceff1;vertical-align:top}
tbody tr.detail-row td{padding:0;background:#f5f7fa;border-bottom:2px solid #c5cae9}
.detail-inner{padding:16px 20px;display:grid;
  grid-template-columns:1fr 1fr;gap:16px;font-size:.85rem}
.detail-inner h4{font-size:.8rem;text-transform:uppercase;color:#666;
  margin-bottom:6px;letter-spacing:.5px}
.detail-box{background:#fff;border:1px solid #e0e0e0;border-radius:6px;
  padding:12px 14px}
.detail-full{grid-column:1/-1}
.badge{display:inline-block;padding:2px 10px;border-radius:12px;
  font-size:.75rem;font-weight:600;color:#fff;white-space:nowrap}
.issue-chip{display:inline-block;padding:2px 8px;border-radius:4px;
  font-size:.72rem;font-weight:600;color:#fff;margin:2px}
.tag-llm{font-size:.7rem;background:#e8f5e9;color:#1b5e20;
  border:1px solid #a5d6a7;border-radius:10px;padding:1px 7px;
  font-weight:600;vertical-align:middle}
.tag-heuristic{font-size:.7rem;background:#fff3e0;color:#e65100;
  border:1px solid #ffcc80;border-radius:10px;padding:1px 7px;
  font-weight:600;vertical-align:middle}
.issue-row-critical{background:#ffebee;border-left:3px solid #c62828;
  padding:4px 8px;margin:3px 0;border-radius:0 4px 4px 0;font-size:.82rem}
.issue-row-major{background:#fff3e0;border-left:3px solid #e65100;
  padding:4px 8px;margin:3px 0;border-radius:0 4px 4px 0;font-size:.82rem}
.issue-row-minor{background:#f3f3f3;border-left:3px solid #9e9e9e;
  padding:4px 8px;margin:3px 0;border-radius:0 4px 4px 0;font-size:.82rem}
.no-results{text-align:center;padding:40px;color:#999;font-size:1rem}
.section-title{font-size:1rem;font-weight:600;color:#37474f;
  margin:16px 0 8px}
.quality-badge{display:inline-block;padding:2px 8px;border-radius:4px;
  font-size:.72rem;font-weight:600;color:#fff}
.purpose-text{font-style:italic;color:#555;font-size:.82rem;
  margin:4px 0;max-height:3.6em;overflow:hidden}
.loc-bar-wrap{width:80px;height:8px;background:#e0e0e0;border-radius:4px;
  display:inline-block;vertical-align:middle}
.loc-bar{height:8px;border-radius:4px;background:#42a5f5}
.tag-cached{font-size:.7rem;background:#e3f2fd;color:#0d47a1;
  border:1px solid #90caf9;border-radius:10px;padding:1px 7px;
  font-weight:600;vertical-align:middle}
@media(max-width:900px){.detail-inner{grid-template-columns:1fr}}
"""

_JS = """\
(function(){
var rows=[],filtered=[];
function init(){
  rows=Array.from(document.querySelectorAll('tbody tr.sample-row'));
  filtered=rows.slice();
  updateCount();
  document.querySelectorAll('thead th[data-col]').forEach(function(th){
    th.addEventListener('click',function(){sortBy(th)});
  });
  ['fv','fs_name','fsub','fai'].forEach(function(id){
    var el=document.getElementById(id);
    if(el)el.addEventListener(el.tagName==='SELECT'?'change':'input',applyFilters);
  });
  document.getElementById('btn-clear').addEventListener('click',clearFilters);
  rows.forEach(function(row){
    row.addEventListener('click',function(){toggleDetail(row)});
  });
}
function toggleDetail(row){
  var did=row.dataset.detail;
  var dr=document.getElementById(did);
  if(!dr)return;
  var open=dr.style.display!==''&&dr.style.display!=='none';
  if(open){dr.style.display='none';row.classList.remove('expanded');}
  else{dr.style.display='';row.classList.add('expanded');}
}
function applyFilters(){
  var v=document.getElementById('fv').value;
  var s=(document.getElementById('fs_name').value||'').toLowerCase();
  var sub=document.getElementById('fsub').value;
  var ai=document.getElementById('fai').value;
  filtered=[];
  rows.forEach(function(row){
    var mv=!v||row.dataset.verdict===v;
    var ms=!s||(row.dataset.path||'').indexOf(s)>=0||(row.dataset.name||'').indexOf(s)>=0;
    var msub=!sub||row.dataset.subsystem===sub;
    var mai=!ai||(ai==='yes'&&row.dataset.hasllm==='1')||(ai==='no'&&row.dataset.hasllm!=='1');
    var show=mv&&ms&&msub&&mai;
    row.style.display=show?'':'none';
    var dr=document.getElementById(row.dataset.detail);
    if(dr&&!show){dr.style.display='none';}
    if(!show)row.classList.remove('expanded');
    if(show)filtered.push(row);
  });
  updateCount();
}
function clearFilters(){
  document.getElementById('fv').value='';
  document.getElementById('fs_name').value='';
  document.getElementById('fsub').value='';
  document.getElementById('fai').value='';
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
  if(el)el.textContent='Showing '+filtered.length+' of '+rows.length+' samples';
}
document.addEventListener('DOMContentLoaded',init);
})();
"""


def _verdict_badge(verdict):
    color = VERDICT_COLORS.get(verdict, '#9e9e9e')
    label = VERDICT_LABELS.get(verdict, verdict)
    esc = html_module.escape
    return (
        f'<span class="badge" style="background:{color}">'
        f'{esc(label)}</span>'
    )


def _quality_badge(quality, label=None):
    colors = {
        'excellent': '#43a047',
        'good':      '#66bb6a',
        'adequate':  '#f57c00',
        'poor':      '#e53935',
        'missing':   '#b71c1c',
    }
    color = colors.get(quality or '', '#9e9e9e')
    lbl = label or (quality or 'n/a').capitalize()
    return (
        f'<span class="quality-badge" style="background:{color}">'
        f'{html_module.escape(lbl)}</span>'
    )


def _loc_bar(loc, max_loc=1000):
    pct = min(100, int(loc / max_loc * 100))
    return (
        f'<span class="loc-bar-wrap">'
        f'<span class="loc-bar" style="width:{pct}%"></span>'
        f'</span> {loc}'
    )


def _detail_html(analysis, idx):
    """Render the expandable detail panel for a single sample."""
    esc = html_module.escape
    h = analysis['heuristic']
    llm = analysis.get('llm')
    sd = analysis['sample']

    parts = ['<div class="detail-inner">']

    # ------------------------------------------------------------------ #
    # Left: heuristic checks                                              #
    # ------------------------------------------------------------------ #
    parts.append('<div class="detail-box">')
    from_cache = analysis.get('_from_cache', False)
    if llm:
        src_tag = '<span class="tag-llm">AI + Heuristic</span>'
    elif from_cache:
        src_tag = '<span class="tag-cached">Cached</span>'
    else:
        src_tag = '<span class="tag-heuristic">Heuristic</span>'
    parts.append(f'<h4>Compliance checks {src_tag}</h4>')

    if h['issues']:
        for key, detail in sorted(
            h['issues'].items(),
            key=lambda kv: SEVERITY_ORDER.index(
                ISSUE_DEFS.get(kv[0], ('minor', ''))[0]
            ),
        ):
            sev = ISSUE_DEFS.get(key, ('minor', ''))[0]
            cls = f'issue-row-{sev}'
            label = ISSUE_DEFS.get(key, ('minor', key))[1]
            parts.append(
                f'<div class="{cls}">'
                f'<strong>[{sev.upper()}]</strong> {esc(label)}'
                f'<br><small style="color:#666">{esc(detail)}</small>'
                f'</div>'
            )
    else:
        parts.append(
            '<p style="color:#2e7d32;font-weight:600">'
            '&#x2714; Passes all heuristic checks'
            '</p>'
        )

    # Source stats
    parts.append(
        f'<p style="margin-top:10px;color:#555">'
        f'<strong>Source:</strong> {sd["source_count"]} file(s), '
        f'{sd["loc"]} LOC'
        f'</p>'
    )

    # Tags / YAML info
    yaml_name = sd.get('yaml_name') or '(none)'
    has_prj = 'yes' if sd['has_prj_conf'] else '<span style="color:#c62828">NO</span>'
    has_cmake = 'yes' if sd['has_cmake'] else '<span style="color:#c62828">NO</span>'
    parts.append(
        f'<p style="margin-top:4px;color:#555">'
        f'<strong>YAML:</strong> {esc(yaml_name)} &nbsp; '
        f'<strong>prj.conf:</strong> {has_prj} &nbsp; '
        f'<strong>CMakeLists:</strong> {has_cmake}'
        f'</p>'
    )

    parts.append('</div>')  # end left box

    # ------------------------------------------------------------------ #
    # Right: AI/heuristic assessment                                      #
    # ------------------------------------------------------------------ #
    parts.append('<div class="detail-box">')
    parts.append('<h4>Quality assessment</h4>')

    if llm:
        purpose = llm.get('purpose') or h.get('purpose', '')
        if purpose:
            parts.append(
                f'<p style="margin-bottom:8px"><em>{esc(purpose)}</em></p>'
            )

        usefulness = llm.get('usefulness', '')
        use_rat = llm.get('usefulness_rationale', '')
        if usefulness:
            parts.append(
                f'<p><strong>Usefulness:</strong> '
                f'{_quality_badge(usefulness)} '
                f'<span style="font-size:.8rem;color:#555">{esc(use_rat)}</span></p>'
            )

        doc_q = llm.get('doc_quality', '')
        doc_rat = llm.get('doc_quality_rationale', '')
        if doc_q:
            parts.append(
                f'<p style="margin-top:6px"><strong>Doc quality:</strong> '
                f'{_quality_badge(doc_q)} '
                f'<span style="font-size:.8rem;color:#555">{esc(doc_rat)}</span></p>'
            )

        code_q = llm.get('code_quality', '')
        code_rat = llm.get('code_quality_rationale', '')
        if code_q:
            parts.append(
                f'<p style="margin-top:6px"><strong>Code quality:</strong> '
                f'{_quality_badge(code_q)} '
                f'<span style="font-size:.8rem;color:#555">{esc(code_rat)}</span></p>'
            )

        if llm.get('is_actually_a_test'):
            parts.append(
                f'<p style="margin-top:8px;color:#c62828;font-weight:600">'
                f'&#x26A0; Looks like a test, not a sample: '
                f'{esc(llm.get("test_rationale", ""))}'
                f'</p>'
            )

        if llm.get('has_testing_overhead'):
            parts.append(
                f'<p style="margin-top:6px;color:#e65100">'
                f'&#x26A0; Testing overhead: '
                f'{esc(llm.get("overhead_rationale", ""))}'
                f'</p>'
            )

        if llm.get('recommendations'):
            parts.append(
                '<p style="margin-top:8px"><strong>Recommendations:</strong></p>'
            )
            for rec in llm['recommendations'][:5]:
                parts.append(
                    f'<div class="issue-row-minor">{esc(rec)}</div>'
                )

        if llm.get('additional_issues'):
            parts.append(
                '<p style="margin-top:8px"><strong>Additional AI-detected issues:</strong></p>'
            )
            for issue in llm['additional_issues'][:5]:
                parts.append(
                    f'<div class="issue-row-major">{esc(issue)}</div>'
                )
    else:
        # Heuristic-only: show purpose and signals
        purpose = h.get('purpose', '')
        if purpose and purpose != '(no description found)':
            parts.append(
                f'<p style="margin-bottom:8px"><em>{esc(purpose)}</em></p>'
            )
        parts.append('<ul style="padding-left:16px">')
        for sig in h.get('signals', []):
            if not sig.startswith('['):
                parts.append(f'<li style="color:#555">{esc(sig)}</li>')
        parts.append('</ul>')
        parts.append(
            '<p style="margin-top:10px;color:#888;font-size:.8rem">'
            'Enable LLM analysis (--max-llm) for detailed quality assessment.'
            '</p>'
        )

    parts.append('</div>')  # end right box

    # ------------------------------------------------------------------ #
    # Full-width: README excerpt                                          #
    # ------------------------------------------------------------------ #
    parts.append('<div class="detail-box detail-full">')
    parts.append(
        f'<h4>Sample path: '
        f'<code style="font-weight:400">{esc(sd["path"])}</code></h4>'
    )

    readme = sd.get('readme_content', '').strip()
    if readme:
        snippet = esc(readme[:600]) + ('...' if len(readme) > 600 else '')
        parts.append(
            f'<pre style="white-space:pre-wrap;font-size:.78rem;'
            f'background:#f9f9f9;padding:8px;border-radius:4px;'
            f'border:1px solid #e0e0e0;max-height:150px;overflow-y:auto;'
            f'margin-top:8px">{snippet}</pre>'
        )
    else:
        parts.append(
            '<p style="color:#c62828;margin-top:6px">'
            '&#x26A0; No README.rst found</p>'
        )

    # Link to GitHub
    rel_path = sd['path'].replace('\\', '/')
    gh_url = (
        f'https://github.com/zephyrproject-rtos/zephyr/tree/main/{rel_path}'
    )
    parts.append(
        f'<p style="margin-top:8px">'
        f'<a href="{esc(gh_url)}" target="_blank">View on GitHub</a>'
        f'</p>'
    )

    parts.append('</div>')  # end full-width box

    parts.append('</div>')  # end detail-inner

    return ''.join(parts)


def generate_html_report(all_analyses, args):
    """
    Generate a fully self-contained HTML report for all analyzed samples.
    Returns the HTML as a string.
    """
    esc = html_module.escape
    now_str = datetime.datetime.now().strftime('%Y-%m-%d %H:%M')
    total = len(all_analyses)

    # Statistics
    verdict_counts = {v: 0 for v in VERDICTS}
    issue_counts = {k: 0 for k in ISSUE_DEFS}
    subsystem_counts = {}
    llm_count = 0

    for a in all_analyses:
        v = a['verdict']
        if v in verdict_counts:
            verdict_counts[v] += 1
        for k in a['heuristic']['issues']:
            if k in issue_counts:
                issue_counts[k] += 1
        sub = a['sample']['subsystem']
        subsystem_counts[sub] = subsystem_counts.get(sub, 0) + 1
        if a.get('llm'):
            llm_count += 1

    # Sort subsystems by count descending
    top_subsystems = sorted(subsystem_counts.items(), key=lambda x: -x[1])

    parts = [
        '<!DOCTYPE html>',
        '<html lang="en">',
        '<head>',
        '<meta charset="UTF-8">',
        '<meta name="viewport" content="width=device-width,initial-scale=1.0">',
        f'<title>Zephyr Sample Quality Report — {now_str}</title>',
        f'<style>{_CSS}</style>',
        '</head>',
        '<body>',
        '<header>',
        '<h1>Zephyr Sample Quality Report</h1>',
        f'<p>Generated: {esc(now_str)}'
        f' &nbsp;|&nbsp; {total} samples analyzed'
        f' &nbsp;|&nbsp; AI analysis: <strong>'
        f'{"enabled (" + esc(getattr(args, "model", "")) + ")" if llm_count > 0 else "disabled"}'
        f'</strong>'
        + (f' &nbsp;|&nbsp; Subsystem filter: <strong>{esc(args.subsystem)}</strong>'
           if getattr(args, 'subsystem', None) else '')
        + '</p>',
        '</header>',
        '<div class="container">',
    ]

    # Verdict summary
    parts.append('<div class="section-title">Summary by Verdict</div>')
    parts.append('<div class="stats-grid">')
    parts.append(
        f'<div class="stat-card" style="border-color:#1a237e">'
        f'<div class="val">{total}</div>'
        f'<div class="lbl">Total Samples</div></div>'
    )
    for v in VERDICTS:
        cnt = verdict_counts[v]
        color = VERDICT_COLORS.get(v, '#9e9e9e')
        pct = f'{cnt / total * 100:.0f}%' if total else '0%'
        parts.append(
            f'<div class="stat-card" style="border-color:{color}">'
            f'<div class="val" style="color:{color}">{cnt}</div>'
            f'<div class="lbl">{esc(VERDICT_LABELS[v])} ({pct})</div></div>'
        )
    parts.append('</div>')

    # Issues frequency table
    top_issues = sorted(issue_counts.items(), key=lambda x: -x[1])
    top_issues = [(k, c) for k, c in top_issues if c > 0]
    if top_issues:
        parts.append('<div class="section-title">Most Common Issues</div>')
        parts.append(
            '<div style="background:#fff;border-radius:8px;padding:14px 20px;'
            'box-shadow:0 1px 3px rgba(0,0,0,.1);margin-bottom:12px">'
        )
        parts.append(
            '<table style="width:100%;border-collapse:collapse">'
            '<thead><tr>'
            '<th style="text-align:left;padding:6px 12px;background:#263238;color:#eceff1;font-size:.8rem">Issue</th>'
            '<th style="text-align:left;padding:6px 12px;background:#263238;color:#eceff1;font-size:.8rem">Severity</th>'
            '<th style="text-align:right;padding:6px 12px;background:#263238;color:#eceff1;font-size:.8rem">Count</th>'
            '<th style="text-align:right;padding:6px 12px;background:#263238;color:#eceff1;font-size:.8rem">% of Samples</th>'
            '</tr></thead>'
            '<tbody>'
        )
        sev_colors = {'critical': '#b71c1c', 'major': '#e65100', 'minor': '#757575'}
        for key, cnt in top_issues[:20]:
            sev, label = ISSUE_DEFS.get(key, ('minor', key))
            sc = sev_colors.get(sev, '#9e9e9e')
            pct = f'{cnt / total * 100:.1f}%' if total else '0%'
            parts.append(
                f'<tr style="border-bottom:1px solid #eceff1">'
                f'<td style="padding:6px 12px;font-size:.83rem">{esc(label)}</td>'
                f'<td style="padding:6px 12px">'
                f'<span style="color:{sc};font-weight:600;font-size:.78rem">'
                f'{esc(sev.upper())}</span></td>'
                f'<td style="padding:6px 12px;text-align:right;font-weight:600">{cnt}</td>'
                f'<td style="padding:6px 12px;text-align:right;color:#666">{pct}</td>'
                f'</tr>'
            )
        parts.append('</tbody></table></div>')

    # Top subsystems
    parts.append('<div class="section-title">Samples by Subsystem</div>')
    parts.append('<div class="stats-grid">')
    for sub, cnt in top_subsystems[:16]:
        parts.append(
            f'<div class="stat-card" style="border-color:#5c6bc0">'
            f'<div class="val" style="color:#3949ab">{cnt}</div>'
            f'<div class="lbl">{esc(sub)}</div></div>'
        )
    parts.append('</div>')

    # Filter controls
    def _opts(values, empty_label='All'):
        opts = [f'<option value="">{esc(empty_label)}</option>']
        for v in values:
            opts.append(f'<option value="{esc(v)}">{esc(v)}</option>')
        return ''.join(opts)

    subsystem_list = sorted(subsystem_counts.keys())

    parts.append(
        '<div class="filters">'
        '<label>Verdict: <select id="fv">'
        + _opts(VERDICTS)
        + '</select></label>'
        '<label>Subsystem: <select id="fsub">'
        + _opts(subsystem_list)
        + '</select></label>'
        '<label>AI analyzed: <select id="fai">'
        '<option value="">All</option>'
        '<option value="yes">AI yes</option>'
        '<option value="no">AI no</option>'
        '</select></label>'
        '<label>Search: <input type="text" id="fs_name" placeholder="path or name" style="width:200px"></label>'
        '<button id="btn-clear">Clear</button>'
        '<span class="filter-count" id="filter-count"></span>'
        '</div>'
    )

    # Main table
    parts.append('<div class="tbl-wrap"><table>')
    parts.append(
        '<thead><tr>'
        '<th data-col="path">Sample</th>'
        '<th>Subsystem</th>'
        '<th data-col="loc" style="width:110px">LOC</th>'
        '<th style="width:80px">YAML</th>'
        '<th style="width:60px">README</th>'
        '<th style="width:120px">Issues</th>'
        '<th data-col="verdict" style="width:140px">Verdict</th>'
        '</tr></thead>'
    )
    parts.append('<tbody>')

    for idx, analysis in enumerate(all_analyses):
        sd = analysis['sample']
        h = analysis['heuristic']
        llm = analysis.get('llm')
        verdict = analysis['verdict']
        detail_id = f'detail-{idx}'

        v_color = VERDICT_COLORS.get(verdict, '#9e9e9e')

        # Issue chips
        issue_chips = []
        for key in sorted(h['issues'], key=lambda k: SEVERITY_ORDER.index(
            ISSUE_DEFS.get(k, ('minor', ''))[0]
        )):
            sev = ISSUE_DEFS.get(key, ('minor', ''))[0]
            chip_colors = {'critical': '#b71c1c', 'major': '#e65100', 'minor': '#757575'}
            cc = chip_colors.get(sev, '#9e9e9e')
            # Short label for the chip
            short = {
                'uses_ztest':             'ztest',
                'uses_zassert':           'zassert',
                'no_sample_yaml':         'no-yaml',
                'wrong_yaml_name':        'wrong-yaml',
                'no_readme':              'no-readme',
                'readme_no_overview':     'no-overview',
                'readme_no_building':     'no-building',
                'readme_no_output':       'no-output',
                'no_prj_conf':            'no-prj.conf',
                'build_only_all':         'build-only',
                'no_integration_platforms': 'no-integ-plat',
                'no_harness':             'no-harness',
                'skip_all':               'skip-all',
                'very_large_source':      'large-src',
                'no_cmake':               'no-cmake',
                'missing_sample_section': 'no-sample-sec',
            }.get(key, key[:12])
            issue_chips.append(
                f'<span class="issue-chip" style="background:{cc}">'
                f'{esc(short)}</span>'
            )
        issues_html = ''.join(issue_chips) or (
            '<span style="color:#2e7d32;font-size:.78rem">&#x2714; none</span>'
        )

        row_attrs = (
            f'data-path="{esc(sd["path"].lower())}"'
            f' data-name="{esc((sd.get("name") or "").lower())}"'
            f' data-verdict="{esc(verdict)}"'
            f' data-subsystem="{esc(sd["subsystem"])}"'
            f' data-loc="{sd["loc"]}"'
            f' data-hasllm="{"1" if llm else "0"}"'
            f' data-detail="{detail_id}"'
        )

        parts.append(
            f'<tr class="sample-row" {row_attrs}'
            f' style="border-left:4px solid {v_color}">'
        )

        # Sample path / name
        name_disp = sd.get('name') or Path(sd['path']).name
        path_short = sd['path'].replace('samples/', '', 1)
        ai_tag = ' <span class="tag-llm">AI</span>' if llm else ''
        parts.append(
            f'<td>'
            f'<div style="font-weight:600;font-size:.85rem">{esc(name_disp)}{ai_tag}</div>'
            f'<div style="font-size:.75rem;color:#888">{esc(path_short)}</div>'
            f'</td>'
        )
        # Subsystem
        parts.append(
            f'<td style="font-size:.82rem;color:#37474f">{esc(sd["subsystem"])}</td>'
        )
        # LOC
        parts.append(
            f'<td style="font-size:.82rem">{_loc_bar(sd["loc"])}</td>'
        )
        # YAML
        yaml_color = '#2e7d32' if sd['yaml_name'] == 'sample.yaml' else (
            '#f57c00' if sd['yaml_name'] else '#c62828'
        )
        yaml_disp = sd.get('yaml_name') or 'missing'
        parts.append(
            f'<td style="font-size:.78rem;color:{yaml_color}">'
            f'{esc(yaml_disp)}</td>'
        )
        # README
        readme_disp = (
            '<span style="color:#2e7d32">&#x2714;</span>'
            if sd['has_readme'] else
            '<span style="color:#c62828">&#x2718;</span>'
        )
        parts.append(f'<td style="text-align:center">{readme_disp}</td>')
        # Issues
        parts.append(f'<td style="line-height:1.8">{issues_html}</td>')
        # Verdict
        parts.append(f'<td>{_verdict_badge(verdict)}</td>')

        parts.append('</tr>')

        # Detail row
        detail_content = _detail_html(analysis, idx)
        parts.append(
            f'<tr class="detail-row" id="{detail_id}" style="display:none">'
            f'<td colspan="7">{detail_content}</td>'
            f'</tr>'
        )

    if not all_analyses:
        parts.append(
            '<tr><td colspan="7" class="no-results">No samples found</td></tr>'
        )

    parts.append('</tbody></table></div>')

    # Footer
    parts.append(
        '<p style="margin:20px 0;color:#999;font-size:.8rem;text-align:center">'
        f'Zephyr Sample Quality Report &mdash; {esc(now_str)}'
        f' &mdash; {total} samples'
        f' &mdash; AI: {"on" if llm_count > 0 else "off"}'
        '</p>'
    )

    parts.append('</div>')
    parts.append(f'<script>{_JS}</script>')
    parts.append('</body></html>')

    return '\n'.join(parts)


# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

def parse_args():
    parser = argparse.ArgumentParser(
        prog='sample_quality_report.py',
        description=(
            'Analyze Zephyr samples against the sample definition criteria '
            'and generate a self-contained HTML report'
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            'Examples:\n'
            '  # Heuristic-only analysis of all samples:\n'
            '  ./scripts/ci/sample_quality_report.py --output report.html\n\n'
            '  # Repeated runs: use git-SHA cache for speed:\n'
            '  ./scripts/ci/sample_quality_report.py \\\n'
            '      --cache sample_quality_cache.json --output report.html\n\n'
            '  # Analyze only the bluetooth subsystem:\n'
            '  ./scripts/ci/sample_quality_report.py --subsystem bluetooth\n\n'
            '  # AI analysis of non-compliant samples (up to 50), with cache:\n'
            '  OPENROUTER_API_KEY=sk-or-... \\\n'
            '      ./scripts/ci/sample_quality_report.py \\\n'
            '      --model anthropic/claude-opus-4 \\\n'
            '      --max-llm 50 --llm-non-compliant-only \\\n'
            '      --cache sample_quality_cache.json \\\n'
            '      --output report.html\n\n'
            '  # Analyze a single sample directory:\n'
            '  ./scripts/ci/sample_quality_report.py \\\n'
            '      --sample samples/hello_world --no-llm\n'
        ),
    )
    parser.add_argument(
        '--output',
        default=DEFAULT_OUTPUT,
        metavar='FILE',
        help=f'Output HTML file (default: {DEFAULT_OUTPUT})',
    )
    parser.add_argument(
        '--json-out',
        default=None,
        metavar='FILE',
        help='Also save full JSON analysis to FILE',
    )
    parser.add_argument(
        '--subsystem',
        default=None,
        metavar='SUBSYSTEM',
        help=(
            'Limit analysis to samples whose path contains this string '
            '(case-insensitive).  Examples: bluetooth, net, kernel'
        ),
    )
    parser.add_argument(
        '--sample',
        default=None,
        metavar='PATH',
        help='Analyze a single sample directory (relative or absolute path)',
    )
    parser.add_argument(
        '--model',
        default='gpt-4o-mini',
        metavar='MODEL',
        help=(
            'LLM model name (default: gpt-4o-mini). '
            'Examples: claude-opus-4, ollama/llama3, anthropic/claude-opus-4'
        ),
    )
    parser.add_argument(
        '--provider',
        default=None,
        choices=['auto', 'openai', 'anthropic', 'litellm', 'openrouter'],
        metavar='PROVIDER',
        help=(
            'LLM provider: auto (default), openai, anthropic, litellm, openrouter. '
            'Overrides SAMPLE_ADVISOR_PROVIDER env var.'
        ),
    )
    parser.add_argument(
        '--max-llm',
        type=int,
        default=0,
        metavar='N',
        help=(
            'Maximum number of samples to analyze with LLM (default: 0 = no LLM). '
            'Samples are prioritized by severity of heuristic issues.'
        ),
    )
    parser.add_argument(
        '--llm-non-compliant-only',
        action='store_true',
        help='Only run LLM analysis on samples that have heuristic issues',
    )
    parser.add_argument(
        '--no-llm',
        action='store_true',
        help='Disable LLM analysis entirely (overrides --max-llm)',
    )
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Print per-sample progress to stderr',
    )
    parser.add_argument(
        '--cache',
        default=None,
        metavar='FILE',
        help=(
            'Path to the cache JSON file (default: no cache). '
            'On the first run the cache is populated; subsequent runs '
            'reuse cached results for samples whose last git commit SHA '
            'has not changed.  Use "%(default)s" to disable caching even '
            'when the environment sets a default.'
        ),
    )
    parser.add_argument(
        '--no-cache',
        action='store_true',
        help='Ignore and do not update any cache file',
    )
    parser.add_argument(
        '--debug',
        action='store_true',
        help='Enable DEBUG logging (shows raw LLM responses and details)',
    )
    return parser.parse_args()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def _any_llm_available():
    """Return True if at least one LLM library is installed with a key set."""
    if HAS_OPENAI and (os.environ.get('OPENAI_API_KEY') or
                       os.environ.get('OPENROUTER_API_KEY')):
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

    # Resolve cache path
    cache_path = None if args.no_cache else args.cache

    # Discover samples
    print('Discovering samples...', file=sys.stderr)
    sample_dirs = discover_samples(
        subsystem_filter=args.subsystem,
        single_sample=args.sample,
    )

    if not sample_dirs:
        print('No samples found.', file=sys.stderr)
        return 1

    print(f'Found {len(sample_dirs)} samples.', file=sys.stderr)

    # Load cache
    cache_entries = load_cache(cache_path)
    cache_hits = 0
    cache_misses = 0

    # Build git tree-SHA map in one call (used for cache invalidation)
    print('Building git tree SHA map...', file=sys.stderr)
    sha_map = _build_sample_sha_map()
    if sha_map:
        log.debug('git SHA map: %d directory entries', len(sha_map))
    else:
        log.debug('git SHA map unavailable; cache invalidation disabled')

    # Determine which samples should have LLM analysis
    use_llm = not args.no_llm and args.max_llm > 0 and _any_llm_available()

    if args.max_llm > 0 and not args.no_llm and not _any_llm_available():
        print(
            'Warning: --max-llm requested but no LLM library/key available.',
            file=sys.stderr,
        )
        use_llm = False

    # First pass: heuristic analysis of all samples (with cache)
    print('Running heuristic analysis...', file=sys.stderr)
    all_analyses = []
    for i, sample_dir in enumerate(sample_dirs):
        rel_path = str(sample_dir.relative_to(ZEPHYR_BASE))
        if args.verbose:
            print(f'  [{i + 1}/{len(sample_dirs)}] {rel_path}', file=sys.stderr)

        # Fetch tree SHA from pre-built map (free — no subprocess per sample)
        git_sha = sha_map.get(rel_path, '')
        cached = cache_entries.get(rel_path)

        if (cached and git_sha
                and cached.get('git_sha') == git_sha
                and 'heuristic' in cached):
            # Cache hit: restore heuristic result without re-reading files
            analysis = _analysis_from_cache(cached)
            analysis['_git_sha'] = git_sha
            cache_hits += 1
            log.debug('Cache hit: %s (sha %s)', rel_path, git_sha[:12])
        else:
            # Cache miss: run full heuristic analysis
            analysis = analyze_sample(sample_dir, run_llm=False)
            analysis['_git_sha'] = git_sha
            # Store heuristic result in cache (LLM added later)
            cache_entries[rel_path] = _cache_entry_from_analysis(
                analysis, git_sha
            )
            cache_misses += 1

        all_analyses.append(analysis)

    if cache_path:
        hits_pct = f'{cache_hits / len(sample_dirs) * 100:.0f}%' if sample_dirs else '0%'
        print(
            f'Cache: {cache_hits} hits ({hits_pct}), '
            f'{cache_misses} misses',
            file=sys.stderr,
        )
        # Persist after heuristic pass so partial results survive interrupts
        save_cache(cache_path, cache_entries)

    # Sort by verdict severity (worst first) then path
    verdict_order = {v: i for i, v in enumerate(reversed(VERDICTS))}
    all_analyses.sort(
        key=lambda a: (
            -verdict_order.get(a['verdict'], 0),
            a['sample']['path'],
        )
    )

    # Second pass: LLM analysis for selected samples
    if use_llm:
        # Select candidates (skip those that already have a cached LLM result
        # for this model and whose git SHA has not changed)
        candidates = []
        for a in all_analyses:
            if args.llm_non_compliant_only and a['verdict'] == 'compliant':
                continue
            # Skip samples that already have a cached LLM result for this
            # model and whose git tree SHA has not changed.
            # Check the cache directly (covers both cache-hit and freshly
            # analyzed samples whose entry was written this run).
            rel_path = a['sample']['path']
            cached = cache_entries.get(rel_path, {})
            git_sha = a.get('_git_sha', '')
            if (cached.get('git_sha') == git_sha
                    and args.model in cached.get('llm', {})):
                # Inject cached LLM result into the in-memory analysis
                llm_r = cached['llm'][args.model]
                a['llm'] = llm_r
                a['_llm_model'] = args.model
                # Re-merge verdict
                if llm_r.get('verdict') in VERDICTS:
                    llm_idx = VERDICTS.index(llm_r['verdict'])
                    h_idx = VERDICTS.index(a['heuristic']['verdict'])
                    if llm_idx > h_idx:
                        a['verdict'] = llm_r['verdict']
                log.debug('LLM cache hit: %s (model %s)', rel_path, args.model)
                continue
            candidates.append(a)
            if len(candidates) >= args.max_llm:
                break

        if candidates:
            print(
                f'Running LLM analysis on {len(candidates)} samples '
                f'(model: {args.model})...',
                file=sys.stderr,
            )
        llm_done = 0
        for analysis in candidates:
            path = analysis['sample']['path']
            if args.verbose:
                print(f'  LLM [{llm_done + 1}/{len(candidates)}] {path}',
                      file=sys.stderr)
            sample_dir = ZEPHYR_BASE / path
            sd = analysis['sample']
            h = analysis['heuristic']

            # Re-read source files if they were dropped from the cache
            if not sd.get('sources') and not analysis.get('_from_cache'):
                pass  # sources already present from fresh analysis
            elif not sd.get('sources'):
                # Restore sources from disk for LLM prompt building
                sd['sources'] = collect_source_files(sample_dir)
                if not sd.get('prj_conf_content') or len(sd['prj_conf_content']) < 50:
                    sd['prj_conf_content'] = _read_text(
                        sample_dir / 'prj.conf'
                    )
                if not sd.get('readme_content') or len(sd['readme_content']) < 100:
                    sd['readme_content'] = _read_text(
                        sample_dir / 'README.rst'
                    )

            llm_result = llm_analyze_sample(sd, h, args.model, args.provider)
            analysis['llm'] = llm_result
            analysis['_llm_model'] = args.model
            if llm_result:
                # Upgrade verdict if LLM rates it worse
                llm_v = llm_result.get('verdict', '')
                if llm_v in VERDICTS:
                    llm_idx = VERDICTS.index(llm_v)
                    h_idx = VERDICTS.index(analysis['verdict'])
                    if llm_idx > h_idx:
                        analysis['verdict'] = llm_v
                # Update cache entry with LLM result
                if cache_path:
                    rel_path = path
                    git_sha = sha_map.get(rel_path, analysis.get('_git_sha', ''))
                    entry = cache_entries.get(rel_path, {})
                    if not entry:
                        entry = _cache_entry_from_analysis(analysis, git_sha)
                        cache_entries[rel_path] = entry
                    if 'llm' not in entry:
                        entry['llm'] = {}
                    entry['llm'][args.model] = llm_result
                    # Save incrementally so partial LLM runs are not lost
                    save_cache(cache_path, cache_entries)
            llm_done += 1
            if args.verbose:
                print(f'  OK ({llm_done}/{len(candidates)})', file=sys.stderr)

    # Generate HTML report
    print('Generating HTML report...', file=sys.stderr)
    html = generate_html_report(all_analyses, args)

    with open(args.output, 'w', encoding='utf-8') as fh:
        fh.write(html)
    print(f'Report written to {args.output}', file=sys.stderr)

    # Optionally save JSON
    if args.json_out:
        # Serialize without non-JSON-safe content (large source files)
        json_data = []
        for a in all_analyses:
            entry = {
                'path':        a['sample']['path'],
                'name':        a['sample'].get('name', ''),
                'subsystem':   a['sample']['subsystem'],
                'yaml_name':   a['sample'].get('yaml_name'),
                'loc':         a['sample']['loc'],
                'verdict':     a['verdict'],
                'issues':      a['heuristic']['issues'],
                'purpose':     a['heuristic']['purpose'],
                'llm':         a.get('llm'),
                'from_cache':  a.get('_from_cache', False),
                'git_sha':     a.get('_git_sha', ''),
            }
            json_data.append(entry)
        with open(args.json_out, 'w', encoding='utf-8') as fh:
            json.dump(json_data, fh, indent=2, default=str)
        print(f'JSON data written to {args.json_out}', file=sys.stderr)

    # Print summary
    verdict_counts = {v: 0 for v in VERDICTS}
    for a in all_analyses:
        verdict_counts[a['verdict']] = verdict_counts.get(a['verdict'], 0) + 1

    print('\nSummary:', file=sys.stderr)
    for v in VERDICTS:
        cnt = verdict_counts[v]
        pct = f'{cnt / len(all_analyses) * 100:.1f}%' if all_analyses else '0%'
        print(f'  {VERDICT_LABELS[v]:<20} {cnt:>5}  ({pct})', file=sys.stderr)

    if cache_path:
        print(
            f'\nCache: {cache_path}  '
            f'({len(cache_entries)} entries stored)',
            file=sys.stderr,
        )

    if not _any_llm_available() and not args.no_llm and args.max_llm == 0:
        print(
            '\nNote: Run with --max-llm N to enable AI-powered quality assessment.',
            file=sys.stderr,
        )

    return 0


if __name__ == '__main__':
    sys.exit(main())
