"""Shared pytest fixtures."""

from __future__ import annotations

import warnings
from pathlib import Path

import numpy as np
import pytest

import magic_formula as mf

DATA_DIR = Path(__file__).resolve().parents[2] / 'tests' / 'data'


@pytest.fixture(scope='session')
def data_dir() -> Path:
    """Directory with the shared .tir files and reference data."""
    return DATA_DIR


@pytest.fixture(scope='session')
def pac2002() -> mf.TireModel:
    """The PAC2002 S model, loaded without any warning."""
    with warnings.catch_warnings():
        warnings.simplefilter('error', mf.MagicFormulaWarning)
        return mf.load_tir(DATA_DIR / 'pac2002_S.tir')


@pytest.fixture(scope='session')
def mf52() -> mf.TireModel:
    """The MF 5.2 S model."""
    with pytest.warns(mf.MagicFormulaWarning):
        return mf.load_tir(DATA_DIR / 'mf52_S52.tir')


@pytest.fixture
def pac2002_text() -> str:
    """Text of pac2002_S.tir."""
    return (DATA_DIR / 'pac2002_S.tir').read_text()


@pytest.fixture
def s_vectors() -> np.ndarray:
    """The S vectors V2, V3, V12, V13, V22 (Fz 0) and V22 (Fz -100)."""
    return np.array(
        [
            [4000, 0, 0.1, 0, 10],
            [4000, 0.05, 0, 0, 10],
            [4000, 0.1, 0.1, 0, 10],
            [4000, 0.05, 0.1, 0, 10],
            [0, 0.05, 0.1, 0, 10],
            [-100, 0.05, 0.1, 0, 10],
        ],
        dtype=np.float64,
        order='F',
    )
