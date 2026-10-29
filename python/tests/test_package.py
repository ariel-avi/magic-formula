"""Package-level tests."""

from __future__ import annotations

import hashlib
import importlib.metadata
import re
from pathlib import Path

import pytest

import magic_formula as mf

REPO_ROOT = Path(__file__).resolve().parents[2]
# SHA-256 of the canonical text at https://www.apache.org/licenses/LICENSE-2.0.txt
APACHE_2_0_SHA256 = 'cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30'


def test_version_matches_cmake_project_version():
    cmake = (REPO_ROOT / 'CMakeLists.txt').read_text()
    match = re.search(r'project\(magic_formula VERSION ([0-9.]+)', cmake)
    assert match is not None
    assert match.group(1) == mf.__version__


def test_version_matches_distribution_metadata():
    assert importlib.metadata.version('magic-formula') == mf.__version__


def test_public_api_exports():
    expected = [
        'AxisDecision',
        'Channel',
        'CoordinateIdentification',
        'IndicatorReading',
        'MagicFormulaError',
        'MagicFormulaWarning',
        'TireModel',
        'load_tir',
        'parse_tir',
    ]
    assert sorted(mf.__all__) == expected
    for name in expected:
        assert hasattr(mf, name)


def test_license_is_the_canonical_apache_2_0_text():
    assert hashlib.sha256((REPO_ROOT / 'LICENSE').read_bytes()).hexdigest() == APACHE_2_0_SHA256


def test_distribution_declares_apache_2_0_license():
    meta = importlib.metadata.metadata('magic-formula')
    assert meta['License-Expression'] == 'Apache-2.0'
    assert meta.get_all('License-File') == ['LICENSE', 'NOTICE']


@pytest.mark.parametrize('name', ['LICENSE', 'NOTICE'])
def test_license_file_copies_match_repository_root(name):
    root_copy = REPO_ROOT / name
    assert (REPO_ROOT / 'python' / name).read_bytes() == root_copy.read_bytes()
    installed = importlib.metadata.distribution('magic-formula').read_text(f'licenses/{name}')
    assert installed == root_copy.read_text()
