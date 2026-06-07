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

  openrouter - OpenRouter.ai aggregator: 200+ models (Claude, Gemini, Llama,
               Mistral, ...) via a single API key using model strings like
               anthropic/claude-opus-4, google/gemini-pro, meta-llama/llama-3-70b
               Requires: pip install openai
               Env:      OPENROUTER_API_KEY
                         OPENROUTER_SITE_URL (optional, shown in or.ai dashboard)
                         OPENROUTER_SITE_NAME (optional)

  auto       - (default) Infer provider from model name and available packages.
               claude-* -> anthropic, then litellm fallback;
               gpt-*/o1-*/o3-*/o4-* -> openai, then litellm fallback;
               OPENROUTER_API_KEY set -> openrouter;
               anything containing '/' -> litellm; fallback: first available.

Usage (heuristic only, no API key needed):
    ./scripts/ci/pr_test_advisor.py --pr 12345

Usage (OpenAI):
    OPENAI_API_KEY=sk-... ./scripts/ci/pr_test_advisor.py --pr 12345

Usage (Claude via Anthropic):
    ANTHROPIC_API_KEY=sk-ant-... \\
        ./scripts/ci/pr_test_advisor.py --pr 12345 \\
        --provider anthropic --model claude-opus-4

Usage (OpenRouter.ai — any model via one key):
    OPENROUTER_API_KEY=sk-or-... \\
        ./scripts/ci/pr_test_advisor.py --pr 12345 \\
        --provider openrouter --model anthropic/claude-opus-4

Usage (Ollama / local model via litellm):
    ./scripts/ci/pr_test_advisor.py --pr 12345 \\
        --provider litellm --model ollama/qwen2.5-coder

Usage (OpenAI-compatible custom endpoint, e.g. vLLM):
    OPENAI_API_KEY=none OPENAI_BASE_URL=http://localhost:8000/v1 \\
        ./scripts/ci/pr_test_advisor.py --pr 12345 --model my-model

Environment variables:
    GITHUB_TOKEN          - GitHub API token (rate limits / private repos)
    ADVISOR_PROVIDER      - Default provider (overridden by --provider)
    OPENAI_API_KEY        - OpenAI / compatible key
    OPENAI_BASE_URL       - Optional base URL for OpenAI-compatible endpoints
    ANTHROPIC_API_KEY     - Anthropic API key
    OPENROUTER_API_KEY    - OpenRouter.ai API key
    OPENROUTER_SITE_URL   - Optional: your site URL shown in or.ai dashboard
    OPENROUTER_SITE_NAME  - Optional: your app name shown in or.ai dashboard

Requirements:
    pip install PyGithub pyyaml
    pip install openai      # for openai and openrouter providers
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
import subprocess
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
# Source -> test directory mirroring map
# More specific prefixes must appear before more general ones.
# Each entry maps a source prefix to one or more candidate test base dirs.
# ---------------------------------------------------------------------------

SRC_TO_TEST_DIRS = [
    ('subsys/bluetooth/',           ['tests/bluetooth/']),
    ('subsys/net/',                 ['tests/net/', 'tests/subsys/net/']),
    ('subsys/mgmt/mcumgr/',         ['tests/subsys/mgmt/mcumgr/']),
    ('subsys/portability/posix/',   ['tests/posix/']),
    ('subsys/testsuite/',           ['tests/subsys/testsuite/', 'tests/ztest/']),
    ('subsys/',                     ['tests/subsys/']),
    ('drivers/',                    ['tests/drivers/']),
    ('kernel/',                     ['tests/kernel/']),
    ('arch/',                       ['tests/arch/']),
    ('lib/os/',                     ['tests/lib/os/']),
    ('lib/',                        ['tests/lib/']),
    ('crypto/',                     ['tests/crypto/']),
    ('boards/',                     ['tests/boards/']),
    ('modules/hostap/',             ['tests/net/wifi/']),
    ('modules/',                    ['tests/modules/']),
]

# ---------------------------------------------------------------------------
# Subsystem -> representative test platforms.
# More specific prefixes first.  Empty list = hardware-specific, needs
# board detection via Kconfig symbol search.
# ---------------------------------------------------------------------------

SUBSYSTEM_PLATFORMS = [
    # Architecture-specific QEMU targets
    ('arch/arm/core/',              ['qemu_cortex_m3', 'qemu_cortex_m0']),
    ('arch/arm64/',                 ['qemu_cortex_a53']),
    ('arch/x86/',                   ['qemu_x86', 'qemu_x86_64']),
    ('arch/riscv/',                 ['qemu_riscv32', 'qemu_riscv64']),
    ('arch/posix/',                 ['native_sim']),
    ('arch/',                       ['qemu_x86', 'qemu_cortex_m3', 'native_sim']),
    # Kernel - representative cross-arch set
    ('kernel/',                     ['qemu_x86', 'qemu_cortex_m3', 'native_sim']),
    # Networking
    ('subsys/net/',                 ['native_sim', 'qemu_x86']),
    ('drivers/net/',                ['native_sim']),
    ('drivers/ethernet/',           ['native_sim']),
    ('drivers/wifi/',               ['native_sim']),
    ('drivers/ieee802154/',         ['native_sim']),
    ('modules/hostap/',             ['native_sim']),
    # Bluetooth
    ('subsys/bluetooth/',           ['native_sim']),
    ('drivers/bluetooth/',          ['native_sim']),
    # Serial / UART - QEMU exposes a UART
    ('drivers/serial/',             ['qemu_x86', 'native_sim']),
    # GPIO / interrupt controllers
    ('drivers/gpio/',               ['native_sim', 'qemu_x86']),
    ('drivers/interrupt_controller/', ['qemu_x86', 'qemu_cortex_m3']),
    # Storage / flash
    ('drivers/flash/',              ['native_sim']),
    ('drivers/disk/',               ['native_sim']),
    ('subsys/fs/',                  ['native_sim']),
    # USB
    ('drivers/usb/',                ['native_sim']),
    ('subsys/usb/',                 ['native_sim']),
    # CAN
    ('drivers/can/',                ['native_sim']),
    ('subsys/canbus/',              ['native_sim']),
    # Display
    ('drivers/display/',            ['native_sim']),
    # Hardware-specific (no generic emulation available)
    ('drivers/i2c/',                []),
    ('drivers/spi/',                []),
    ('drivers/sensor/',             []),
    ('drivers/adc/',                []),
    ('drivers/dac/',                []),
    ('drivers/pwm/',                []),
    ('drivers/clock_control/',      []),
    ('drivers/pinctrl/',            []),
    ('drivers/dma/',                []),
    ('drivers/',                    []),
    # Subsystems
    ('subsys/portability/posix/',   ['native_sim']),
    ('subsys/',                     ['native_sim', 'qemu_x86']),
    # Libraries / crypto
    ('lib/',                        ['native_sim', 'qemu_x86']),
    ('crypto/',                     ['native_sim', 'qemu_x86']),
    # Tooling / scripts (no twister run)
    ('scripts/',                    []),
    ('doc/',                        []),
    ('cmake/',                      []),
    ('dts/',                        []),
]

# Source directories where missing test coverage is noteworthy
COVERAGE_SOURCE_DIRS = (
    'drivers/',
    'subsys/',
    'kernel/',
    'lib/',
    'arch/',
    'crypto/',
    'modules/',
)


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
# Source -> test directory mirroring
# ---------------------------------------------------------------------------

def mirror_source_to_tests(files):
    """
    Map changed source files to likely test directories using the Zephyr
    directory mirroring convention (drivers/ -> tests/drivers/, etc.).

    For each source file, tries the most specific matching test subdirectory
    first, then falls back to the base test directory for that subsystem.

    Returns a list of dicts:
      { 'test_dir': str, 'source_file': str, 'exists': bool }
    Deduplicates test directories across all input files.
    """
    seen = {}
    for f in files:
        if f.endswith(('.rst', '.md', '.yaml', '.yml', '.conf', '.defconfig')):
            continue
        for src_prefix, test_bases in SRC_TO_TEST_DIRS:
            if not f.startswith(src_prefix):
                continue
            rel = f[len(src_prefix):]
            sub = rel.split('/')[0] if '/' in rel else ''
            for test_base in test_bases:
                # Prefer the deepest existing subdir, then the base
                candidates = []
                if sub:
                    candidates.append(f'{test_base}{sub}/')
                candidates.append(test_base)
                for candidate in candidates:
                    if candidate in seen:
                        break
                    exists = (ZEPHYR_BASE / candidate.rstrip('/')).is_dir()
                    seen[candidate] = {'test_dir': candidate,
                                       'source_file': f,
                                       'exists': exists}
                    if exists:
                        break  # stop at first existing level
            break  # first matching src prefix wins
    return list(seen.values())


# ---------------------------------------------------------------------------
# Driver -> board mapping via Kconfig symbol
# ---------------------------------------------------------------------------

def _driver_kconfig_symbol(driver_file):
    """
    Extract the Kconfig symbol that enables a driver source file by
    inspecting the CMakeLists.txt in the same directory.

    Looks for:
      zephyr_library_sources_ifdef(CONFIG_FOO file.c)

    Returns the symbol string (without CONFIG_ prefix), or None.
    """
    src_name = Path(driver_file).name
    cmake_path = ZEPHYR_BASE / Path(driver_file).parent / 'CMakeLists.txt'
    if not cmake_path.exists():
        return None
    try:
        text = cmake_path.read_text()
    except OSError:
        return None
    m = re.search(
        r'zephyr_library_sources_ifdef\s*\(\s*(CONFIG_\w+)\s+' + re.escape(src_name),
        text,
    )
    return m.group(1).replace('CONFIG_', '') if m else None


def _boards_for_kconfig(symbol, max_boards=10):
    """
    Find board names whose defconfig or DTS includes reference CONFIG_<symbol>.

    Strategy (in order):
    1. grep boards/ defconfigs for CONFIG_<symbol> (any value, catches variants
       like CONFIG_UART_NS16550_ACCESS_WORD_ONLY=y as well as CONFIG_X=y).
    2. If the Kconfig symbol suggests a driver, derive the likely compatible
       string (e.g. UART_NS16550 -> ns16550) and grep DTS files.

    Returns a sorted list of board directory names (capped at max_boards).
    """
    boards_root = ZEPHYR_BASE / 'boards'
    if not boards_root.is_dir():
        return []

    boards = []
    seen = set()

    def _add_from_path(p):
        name = Path(p).parent.name
        if name not in seen:
            seen.add(name)
            boards.append(name)

    # 1. Search defconfigs for CONFIG_<symbol> (any trailing chars)
    config_prefix = f'CONFIG_{symbol}'
    try:
        result = subprocess.run(
            ['grep', '-rl', '--include=*defconfig', config_prefix,
             str(boards_root)],
            capture_output=True, text=True, timeout=15,
        )
        for line in result.stdout.splitlines():
            _add_from_path(line)
            if len(boards) >= max_boards:
                return sorted(boards)
    except (subprocess.TimeoutExpired, OSError, FileNotFoundError):
        pass

    # 2. Search board DTS/DTSI files for a compatible string derived from the
    #    symbol name (e.g. UART_NS16550 -> "ns16550", SPI_NRF -> "nordic,nrf-spi")
    if len(boards) < max_boards:
        # Build a lowercase compatible hint: strip common driver-type prefixes
        compat_hint = symbol.lower()
        for strip in ('uart_', 'spi_', 'i2c_', 'gpio_', 'sensor_',
                      'pwm_', 'adc_', 'dac_', 'can_', 'flash_', 'wifi_',
                      'bt_', 'eth_', 'usb_', 'display_'):
            if compat_hint.startswith(strip):
                compat_hint = compat_hint[len(strip):]
                break
        if len(compat_hint) > 3:
            try:
                result = subprocess.run(
                    ['grep', '-rl', '--include=*.dts', '--include=*.dtsi',
                     compat_hint, str(boards_root)],
                    capture_output=True, text=True, timeout=15,
                )
                for line in result.stdout.splitlines():
                    _add_from_path(line)
                    if len(boards) >= max_boards:
                        break
            except (subprocess.TimeoutExpired, OSError, FileNotFoundError):
                pass

    return sorted(seen)[:max_boards]


def find_driver_boards(files):
    """
    For changed driver source files, find the Kconfig symbol that enables
    each and locate boards that set that symbol in their defconfig.

    Returns a list of dicts:
      { 'file': str, 'symbol': str, 'boards': [str] }
    Only .c files under drivers/ with a discoverable Kconfig symbol are included.
    """
    results = []
    seen_symbols = set()
    for f in files:
        if not (f.startswith('drivers/') and f.endswith('.c')):
            continue
        symbol = _driver_kconfig_symbol(f)
        if not symbol or symbol in seen_symbols:
            continue
        seen_symbols.add(symbol)
        boards = _boards_for_kconfig(symbol)
        results.append({'file': f, 'symbol': symbol, 'boards': boards})
    return results


# ---------------------------------------------------------------------------
# Platform suggestion
# ---------------------------------------------------------------------------

def suggest_platforms(files):
    """
    Return representative test platforms for the subsystems touched.

    Uses SUBSYSTEM_PLATFORMS (ordered, first match wins per file).
    Platforms are deduplicated across all files.

    Returns:
      platforms      - sorted list of platform slugs to add to twister -p
      hw_specific    - sorted list of source prefixes that have no emulation
                       and need board detection instead
    """
    seen_platforms = set()
    all_platforms = []
    hw_specific = set()

    for f in files:
        for prefix, platforms in SUBSYSTEM_PLATFORMS:
            if f.startswith(prefix):
                if platforms:
                    for p in platforms:
                        if p not in seen_platforms:
                            seen_platforms.add(p)
                            all_platforms.append(p)
                else:
                    hw_specific.add(prefix)
                break

    return all_platforms, sorted(hw_specific)


# ---------------------------------------------------------------------------
# Coverage gap detection
# ---------------------------------------------------------------------------

def detect_coverage_gaps(files, mirrored_tests):
    """
    Identify source files that belong to a testable subsystem but have no
    matching test directory in the mirrored test tree.

    Returns a list of file paths considered to have no test coverage.
    """
    covered_prefixes = set()
    for entry in mirrored_tests:
        if entry['exists']:
            covered_prefixes.add(
                next(
                    (p for p, _ in SRC_TO_TEST_DIRS if entry['source_file'].startswith(p)),
                    None,
                )
            )
    covered_prefixes.discard(None)

    gaps = []
    skip_exts = {'.rst', '.md', '.yaml', '.yml', '.conf',
                 '.defconfig', '.cmake', '.txt', '.h'}
    for f in files:
        if not any(f.startswith(d) for d in COVERAGE_SOURCE_DIRS):
            continue
        if Path(f).suffix in skip_exts:
            continue
        if not any(f.startswith(p) for p in covered_prefixes):
            gaps.append(f)
    return gaps


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


OPENROUTER_BASE_URL = 'https://openrouter.ai/api/v1'


def _resolve_provider(model, provider_hint):
    """
    Determine which LLM provider to use.

    Resolution order:
      1. Explicit --provider / ADVISOR_PROVIDER value
      2. OPENROUTER_API_KEY present -> openrouter (requires openai package)
      3. Model name prefix heuristic
      4. litellm if installed (handles model strings with '/')
      5. First available library with a configured key

    Returns one of 'openai', 'anthropic', 'litellm', 'openrouter', or None.
    """
    hint = provider_hint or os.environ.get('ADVISOR_PROVIDER', 'auto')

    if hint != 'auto':
        if hint in ('openai', 'openrouter') and not HAS_OPENAI:
            log.warning(
                'Provider "%s" requested but openai package is not installed', hint
            )
            return None
        if hint == 'anthropic' and not HAS_ANTHROPIC:
            log.warning('Provider "anthropic" requested but anthropic package is not installed')
            return None
        if hint == 'litellm' and not HAS_LITELLM:
            log.warning('Provider "litellm" requested but litellm package is not installed')
            return None
        return hint

    # If OPENROUTER_API_KEY is set, prefer openrouter when openai is available
    if os.environ.get('OPENROUTER_API_KEY') and HAS_OPENAI:
        return 'openrouter'

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
You are an expert Zephyr RTOS code reviewer and QA engineer.
Analyze the GitHub pull request described below and respond with ONLY a
JSON object — no prose, no markdown fences, no explanation outside the JSON.

The JSON object must have exactly these fields:
{
  "summary": "<2-3 sentence description of what this PR does>",
  "complexity": "<trivial|low|medium|high|critical>",
  "complexity_rationale": "<1-2 sentences explaining the rating>",
  "affected_subsystems": ["<subsystem1>", ...],
  "risk_areas": ["<area of concern>", ...],
  "test_focus": ["<what aspects should be tested>", ...],
  "additional_platforms": ["<zephyr platform slug>", ...],
  "notes": "<special testing notes or caveats, or empty string>"
}

Complexity guidelines:
- trivial: docs-only, typo fix, comment update, CI-config only
- low: single driver/subsystem, narrow scope, < 100 lines
- medium: single subsystem but non-trivial, 100-500 lines, or API change
- high: multiple subsystems, kernel/arch changes, public API break, > 500 lines
- critical: cross-cutting refactor, ABI break, security fix, build system overhaul

For additional_platforms: suggest Zephyr board/platform slugs most relevant for
testing this change (e.g. native_sim, qemu_x86, nrf52dk_nrf52832).
Leave the list empty if the heuristic platforms already cover it well.

IMPORTANT: Your entire response must be a single valid JSON object.
Do NOT wrap it in ```json fences. Do NOT add any text before or after.
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
Heuristic complexity estimate: {heuristic_complexity}

Test directories mirroring the changed source (exist in tree):
{mirrored_tests}

Suggested test platforms (based on subsystem type):
{suggested_platforms}

Hardware-specific drivers (no generic emulation, need board detection):
{hw_specific}

Coverage gaps (source files with no matching test directory found):
{coverage_gaps}

Please analyze this PR and return the final_report JSON.
"""


def _build_user_content(pr_data, heuristic_result):
    """Build the user message string from PR data and heuristic results."""
    files = pr_data['files']
    file_list = '\n'.join(f'  {f}' for f in sorted(files))

    mirrored = [e['test_dir'] for e in heuristic_result.get('mirrored_tests', [])
                if e['exists']]
    gaps = heuristic_result.get('coverage_gaps', [])
    platforms = heuristic_result.get('suggested_platforms', [])
    hw_specific = heuristic_result.get('hw_specific', [])

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
        heuristic_complexity=heuristic_result['complexity']['level'],
        mirrored_tests=', '.join(mirrored) if mirrored else '(none found)',
        suggested_platforms=', '.join(platforms) if platforms else '(hardware-specific)',
        hw_specific=', '.join(hw_specific) if hw_specific else '(none)',
        coverage_gaps='\n'.join(f'  {g}' for g in gaps) if gaps else '(none detected)',
    )


def _extract_json(raw):
    """
    Extract a JSON object from raw LLM output.

    Tries in order:
    1. Direct json.loads (model returned pure JSON).
    2. Strip markdown code fences (```json ... ``` or ``` ... ```).
    3. Find the first {...} block in the text (model added prose around JSON).

    Raises json.JSONDecodeError if no valid JSON is found.
    """
    if not raw or not raw.strip():
        raise json.JSONDecodeError('Empty response from LLM', '', 0)

    # 1. Direct parse
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        pass

    # 2. Strip markdown code fences
    stripped = re.sub(r'^```(?:json)?\s*', '', raw.strip(), flags=re.IGNORECASE)
    stripped = re.sub(r'```\s*$', '', stripped.strip())
    try:
        return json.loads(stripped)
    except json.JSONDecodeError:
        pass

    # 3. Extract the first {...} block
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
        max_tokens=2048,
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
        max_tokens=2048,
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
        max_tokens=2048,
    )
    return response.choices[0].message.content

def _call_openrouter(model, system, user):
    """
    Call OpenRouter.ai using the OpenAI-compatible endpoint.

    OpenRouter accepts any model string from their catalogue, e.g.:
      anthropic/claude-opus-4
      google/gemini-pro
      meta-llama/llama-3-70b-instruct

    Note: response_format={'type': 'json_object'} is intentionally omitted
    here because many OpenRouter-proxied backends (Claude, Gemini, Llama, ...)
    do not support that OpenAI-specific parameter. JSON is extracted from the
    raw response text by _extract_json() instead.
    """
    api_key = os.environ.get('OPENROUTER_API_KEY')
    if not api_key:
        raise RuntimeError('OPENROUTER_API_KEY is not set')
    extra_headers = {}
    site_url = os.environ.get('OPENROUTER_SITE_URL', '')
    site_name = os.environ.get('OPENROUTER_SITE_NAME', 'Zephyr PR Test Advisor')
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
        max_tokens=2048,
    )
    content = response.choices[0].message.content
    # Some OpenRouter backends signal refusal or empty output via finish_reason
    finish_reason = response.choices[0].finish_reason
    if not content or not content.strip():
        raise json.JSONDecodeError(
            f'Empty response from OpenRouter (finish_reason={finish_reason!r})', '', 0
        )
    return content


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
        'openrouter': _call_openrouter,
    }

    backend_fn = _BACKENDS.get(resolved)
    if backend_fn is None:
        log.warning('Unknown provider: %s', resolved)
        return None

    log.info('Using LLM provider: %s  model: %s', resolved, model)
    pr_data['llm_provider'] = resolved
    try:
        raw = backend_fn(model, LLM_SYSTEM_PROMPT, user_content)
        log.debug('Raw LLM response: %s', raw)
        result = _extract_json(raw)
        return result
    except json.JSONDecodeError as exc:
        # Show the raw response so the user can diagnose what the model returned
        log.warning('LLM analysis failed (%s): %s', resolved, exc)
        if exc.doc and exc.doc.strip():
            log.warning(
                'Raw LLM response was:\n%s',
                exc.doc[:2000] + ('...' if len(exc.doc) > 2000 else ''),
            )
        else:
            log.warning(
                'The model returned an empty response. '
                'Try a different model with --model, or check your API key.'
            )
        return None
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

    # --- Maintainer area detection ---
    maintainer_resolver = MaintainerResolver()
    areas = maintainer_resolver.find_areas(files)
    area_labels = sorted({lbl for _, lbls in areas for lbl in lbls})

    # --- Complexity heuristic ---
    complexity = estimate_complexity(files, pr_data['diff_stats'])

    # --- Source -> test mirroring ---
    mirrored_tests = mirror_source_to_tests(files)

    # --- Driver -> board mapping ---
    driver_boards = find_driver_boards(files)

    # --- Platform suggestions ---
    suggested_platforms, hw_specific = suggest_platforms(files)

    # --- Coverage gap detection ---
    coverage_gaps = detect_coverage_gaps(files, mirrored_tests)

    heuristic_result = {
        'subsystems': sorted(subsystems),
        'area_labels': area_labels,
        'complexity': complexity,
        'mirrored_tests': mirrored_tests,
        'driver_boards': driver_boards,
        'suggested_platforms': suggested_platforms,
        'hw_specific': hw_specific,
        'coverage_gaps': coverage_gaps,
    }

    # --- Directly modified test paths (test/sample yamls in changed dirs) ---
    test_paths = find_test_paths_for_files(files)
    pytest_tests = find_pytest_tests(files)

    # --- LLM analysis (optional) ---
    pr_data['llm_model'] = llm_model
    llm_result = llm_analyze(pr_data, heuristic_result, provider=llm_provider)

    return {
        'pr': pr_data,
        'heuristic': heuristic_result,
        'test_paths': sorted(test_paths),
        'pytest_tests': pytest_tests,
        'llm': llm_result,
    }


# ---------------------------------------------------------------------------
# Report generation
# ---------------------------------------------------------------------------

def _twister_cmd(test_paths, platforms, integration=False):
    """Build a twister command string from explicit paths and platforms."""
    parts = ['./scripts/twister']
    if integration:
        parts.append('--integration')
    for p in sorted(test_paths):
        parts += ['-T', p]
    for p in sorted(platforms):
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
    lines.append(f"PR Test Advisor Report \u2014 #{pr['number']}: {pr['title']}")
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

    # --- Summary ---
    lines.append('SUMMARY')
    lines.append('-' * 40)
    if llm and llm.get('summary'):
        lines.append(llm['summary'])
    else:
        lines.append(
            '(No LLM summary \u2014 install openai/anthropic/litellm and set an API key '
            '(OPENAI_API_KEY, ANTHROPIC_API_KEY, or OPENROUTER_API_KEY) to enable AI analysis)'
        )
        lines.append('')
        lines.append('Affected subsystems (heuristic):')
        for s in h['subsystems']:
            lines.append(f'  \u2022 {s}')
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
            lines.append(f'    \u2022 {reason}')
    lines.append('')

    # --- Subsystems ---
    lines.append('AFFECTED SUBSYSTEMS / LABELS')
    lines.append('-' * 40)
    if llm and llm.get('affected_subsystems'):
        for s in llm['affected_subsystems']:
            lines.append(f'  \u2022 {s}')
    else:
        for s in h['subsystems']:
            lines.append(f'  \u2022 {s}')
    if h['area_labels']:
        lines.append('')
        lines.append('  GitHub labels:')
        for lbl in h['area_labels']:
            lines.append(f'    [{lbl}]')
    lines.append('')

    # --- Risk areas (LLM) ---
    if llm and llm.get('risk_areas'):
        lines.append('RISK AREAS')
        lines.append('-' * 40)
        for r in llm['risk_areas']:
            lines.append(f'  \u26a0  {r}')
        lines.append('')

    # --- Directly modified test/sample directories ---
    lines.append('DIRECTLY MODIFIED TESTS / SAMPLES')
    lines.append('-' * 40)
    if analysis['test_paths']:
        lines.append('  These test/sample directories are directly changed:')
        for path in analysis['test_paths']:
            lines.append(f'    {path}')
    else:
        lines.append('  No directly modified test or sample directories found.')
    lines.append('')

    # --- Mirrored test coverage ---
    lines.append('TEST COVERAGE (source \u2192 tests/ mirror)')
    lines.append('-' * 40)
    mirrored_existing = [e for e in h['mirrored_tests'] if e['exists']]
    mirrored_missing = [e for e in h['mirrored_tests'] if not e['exists']]
    if mirrored_existing:
        lines.append('  Test directories found for changed source:')
        for e in mirrored_existing:
            lines.append(f"    + {e['test_dir']}  (for {e['source_file']})")
    else:
        lines.append('  No mirrored test directories found.')
    if mirrored_missing:
        lines.append('  Expected test directories NOT found in tree:')
        for e in mirrored_missing:
            lines.append(f"    ? {e['test_dir']}  (for {e['source_file']})")
    lines.append('')

    # --- Coverage gaps ---
    if h['coverage_gaps']:
        lines.append('COVERAGE GAPS (no tests found for these files)')
        lines.append('-' * 40)
        lines.append('  The following changed files have no matching test directory.')
        lines.append('  Consider adding tests or extending existing ones:')
        for f in sorted(h['coverage_gaps']):
            lines.append(f'    \u26a0  {f}')
        lines.append('')

    # --- Driver -> board mapping ---
    if h['driver_boards']:
        lines.append('DRIVER \u2192 BOARD MAPPING')
        lines.append('-' * 40)
        for entry in h['driver_boards']:
            sym = entry['symbol']
            boards = entry['boards']
            lines.append(f"  {entry['file']}  (CONFIG_{sym})")
            if boards:
                lines.append(f"    Boards with CONFIG_{sym}=y:")
                for b in boards:
                    lines.append(f'      {b}')
            else:
                lines.append(f'    No boards found with CONFIG_{sym}=y in defconfigs.')
        lines.append('')

    # --- Platform suggestions ---
    lines.append('SUGGESTED TEST PLATFORMS')
    lines.append('-' * 40)
    all_platforms = list(h['suggested_platforms'])
    # Merge LLM platform hints
    if llm and llm.get('additional_platforms'):
        for p in llm['additional_platforms']:
            if p not in all_platforms:
                all_platforms.append(p)
    # Merge boards from driver detection
    board_platforms = []
    for entry in h['driver_boards']:
        for b in entry['boards']:
            if b not in all_platforms and b not in board_platforms:
                board_platforms.append(b)

    if all_platforms:
        lines.append('  Software-emulated / QEMU platforms:')
        for p in all_platforms:
            lines.append(f'    {p}')
    if board_platforms:
        lines.append('  Hardware boards (from driver defconfig search):')
        for b in board_platforms[:6]:
            lines.append(f'    {b}')
        if len(board_platforms) > 6:
            lines.append(f'    ... and {len(board_platforms) - 6} more')
    if h['hw_specific']:
        lines.append('  Hardware-specific subsystems (no emulation, need physical boards):')
        for prefix in h['hw_specific']:
            lines.append(f'    {prefix}')
    if not all_platforms and not board_platforms and not h['hw_specific']:
        lines.append('  No platform recommendations for this change type.')
    lines.append('')

    # --- Twister commands ---
    lines.append('TWISTER: SUGGESTED COMMANDS')
    lines.append('-' * 40)

    # Collect all test paths: directly modified + mirrored existing
    twister_test_paths = set(analysis['test_paths'])
    for e in mirrored_existing:
        twister_test_paths.add(e['test_dir'].rstrip('/'))

    if twister_test_paths and all_platforms:
        cmd = _twister_cmd(twister_test_paths, all_platforms)
        lines.append('  Targeted run (specific tests + platforms):')
        lines.append(f'    {cmd}')
        lines.append('')

    if twister_test_paths and not all_platforms and board_platforms:
        cmd = _twister_cmd(twister_test_paths, board_platforms[:3])
        lines.append('  Targeted run on hardware boards:')
        lines.append(f'    {cmd}')
        lines.append('')

    if twister_test_paths and not all_platforms and not board_platforms:
        cmd = _twister_cmd(twister_test_paths, [])
        lines.append('  Targeted run (tests only, select platform manually):')
        lines.append(f'    {cmd}')
        lines.append('')

    if all_platforms and not twister_test_paths:
        cmd = _twister_cmd([], all_platforms, integration=True)
        lines.append('  Integration run limited to relevant platforms:')
        lines.append(f'    {cmd}')
        lines.append('')

    if c_level in ('high', 'critical') or (not twister_test_paths and not all_platforms):
        lines.append('  \u26a0  Full integration run recommended:')
        lines.append('    ./scripts/twister --integration')
        lines.append('')

    # --- Pytest ---
    lines.append('PYTEST: RECOMMENDED TEST SUITES')
    lines.append('-' * 40)
    if analysis['pytest_tests']:
        for entry in analysis['pytest_tests']:
            lines.append(f"  {entry['description']}:")
            for p in entry['paths']:
                lines.append(f'    pytest {p}/')
    else:
        lines.append('  No matching pytest suites for the changed files.')
    lines.append('')

    # --- LLM test focus ---
    if llm and llm.get('test_focus'):
        lines.append('AI-RECOMMENDED TEST FOCUS')
        lines.append('-' * 40)
        for focus in llm['test_focus']:
            lines.append(f'  \u2022 {focus}')
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
        choices=['auto', 'openai', 'anthropic', 'litellm', 'openrouter'],
        metavar='PROVIDER',
        help='LLM provider: auto (default), openai, anthropic, litellm, openrouter. '
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
        '--debug',
        action='store_true',
        help='Enable DEBUG logging (shows raw LLM response and other details)',
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
