"""MF6.1 pressure, turn-slip and rolling-radius integration."""

from __future__ import annotations

import warnings

import numpy as np
import pytest

import magic_formula as mf


def load_model(data_dir, **options):
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', mf.MagicFormulaWarning)
        return mf.load_tir(data_dir / 'mf61_solver.tir', **options)


def test_pressure_turn_slip_and_radii(data_dir):
    model = load_model(data_dir)
    assert model.name == 'MF 6.1'
    assert not model.turn_slip
    assert [c.name for c in model.inputs] == ['FZ', 'SA', 'SR', 'IA', 'VX', 'P', 'TS']
    point = dict(fz=5000, sa=0.02, sr=0.1, ia=0.01, vx=27, p=180000)
    defaulted = model.evaluate(**point)
    explicit = model.evaluate(**point, ts=0)
    assert defaulted.keys() == explicit.keys()
    for key in defaulted:
        np.testing.assert_array_equal(defaulted[key], explicit[key])
    assert defaulted['re'] > 0
    assert defaulted['rl'] > 0
    assert np.isfinite(defaulted['fx'])
    changed = model.evaluate(**{**point, 'p': 200000})
    assert not np.isclose(defaulted['fy'], changed['fy'])
    with pytest.raises(TypeError, match='p'):
        model.evaluate(**{key: value for key, value in point.items() if key != 'p'})
    with pytest.raises(mf.MagicFormulaError, match='7 columns'):
        model.evaluate_batch([[5000, 0.02, 0.1, 0.01, 27, 180000]])
    with pytest.warns(mf.MagicFormulaWarning, match='turn slip is disabled'):
        model.evaluate(**point, ts=0.05)
    enabled = load_model(data_dir, turn_slip=True)
    assert enabled.turn_slip
    with warnings.catch_warnings():
        warnings.simplefilter('error', mf.MagicFormulaWarning)
        solved = enabled.evaluate(**point, ts=0.05)
    assert all(np.isfinite(value) for value in solved.values())
    chars = enabled.characteristics(**point, ts=0.05)
    assert np.isfinite(chars['dfy_dsa'])
    assert np.isfinite(chars['dfx_dsr'])


def test_parse_options_and_pressure_domain(data_dir):
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', mf.MagicFormulaWarning)
        model = mf.parse_tir((data_dir / 'mf61_solver.tir').read_text(), combined=False, turn_slip=True)
    assert not model.combined
    assert model.turn_slip
    with pytest.raises(mf.MagicFormulaError) as failure:
        model.evaluate(fz=5000, sa=0, sr=0, ia=0, vx=27, p=0)
    assert failure.value.key == 'P'


def test_old_models_reject_turn_slip(data_dir):
    for name in ('pac2002_S.tir', 'mf52_S52.tir'):
        with pytest.raises(mf.MagicFormulaError) as failure:
            mf.load_tir(data_dir / name, turn_slip=True)
        assert failure.value.code == 'NOT_IMPLEMENTED'
