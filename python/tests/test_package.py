"""Package-level tests."""

from __future__ import annotations

import importlib.metadata
import re
from pathlib import Path

import magic_formula as mf


def test_version_matches_cmake_project_version():
    cmake = (Path(__file__).resolve().parents[2] / 'CMakeLists.txt').read_text()
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
