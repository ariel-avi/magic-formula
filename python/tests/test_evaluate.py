"""Tests of TireModel.evaluate."""

from __future__ import annotations

import numpy as np
import pytest

import magic_formula as mf

# Broadcast and batch paths may differ by a few ULP.
GRID_REL_TOL = 1e-12
OUTPUT_KEYS = ['fx', 'fy', 'mz', 'mx', 'my', 'fz_clamped', 'sr_clamped', 'sa_clamped', 'ia_clamped', 'below_vxlow']


def test_scalars_return_zero_dimensional_arrays(pac2002):
    result = pac2002.evaluate(fz=4000.0, sa=0.05, sr=0.0, ia=0.0, vx=10.0)
    for value in result.values():
        assert value.shape == ()
        assert value.dtype == np.float64
    batch = pac2002.evaluate_batch([[4000, 0.05, 0, 0, 10]])
    assert result['fx'] == batch[0, 0]


def test_sweep_broadcasts_against_scalars(pac2002):
    sa = np.linspace(-0.2, 0.2, 301)
    result = pac2002.evaluate(fz=4000.0, sa=sa, sr=0.0, ia=0.0, vx=10.0)
    assert all(v.shape == (301,) for v in result.values())


def test_grid_broadcasts(pac2002):
    fz = np.array([[2000.0], [4000.0], [6000.0]])
    sa = np.linspace(-0.2, 0.2, 301)
    result = pac2002.evaluate(fz=fz, sa=sa, sr=0.0, ia=0.0, vx=10.0)
    assert result['fy'].shape == (3, 301)
    for i, j in [(0, 0), (1, 150), (2, 300)]:
        expected = pac2002.evaluate_batch([[fz[i, 0], sa[j], 0, 0, 10]])[0, 1]
        assert result['fy'][i, j] == pytest.approx(expected, rel=GRID_REL_TOL)


def test_keys_are_lower_case_output_names(pac2002):
    result = pac2002.evaluate(fz=4000.0, sa=0.0, sr=0.0, ia=0.0, vx=10.0)
    assert list(result) == OUTPUT_KEYS


def test_matches_evaluate_batch_columns(pac2002, s_vectors):
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0'):
        result = pac2002.evaluate(
            fz=s_vectors[:, 0], sa=s_vectors[:, 1], sr=s_vectors[:, 2], ia=s_vectors[:, 3], vx=s_vectors[:, 4]
        )
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0'):
        batch = pac2002.evaluate_batch(s_vectors)
    for j, key in enumerate(OUTPUT_KEYS):
        assert np.array_equal(result[key], batch[:, j])


def test_missing_input_raises_type_error(pac2002):
    with pytest.raises(TypeError) as info:
        pac2002.evaluate(fz=4000.0, sa=0.0, sr=0.0, ia=0.0)
    assert str(info.value) == 'evaluate() missing input(s) for PAC2002: vx'


def test_unknown_input_raises_type_error(pac2002):
    with pytest.raises(TypeError) as info:
        pac2002.evaluate(fz=4000.0, sa=0.0, sr=0.0, ia=0.0, vx=10.0, p=1e5)
    assert 'p' in str(info.value)
    assert 'expected fz, sa, sr, ia, vx' in str(info.value)


def test_incompatible_shapes_raise_value_error(pac2002):
    with pytest.raises(ValueError):
        pac2002.evaluate(fz=4000.0, sa=np.zeros(3), sr=np.zeros(4), ia=0.0, vx=10.0)


def test_error_row_is_flat_index(pac2002):
    vx = np.full(6, 10.0)
    vx[4] = -1.0
    with pytest.raises(mf.MagicFormulaError) as info:
        pac2002.evaluate(fz=4000.0, sa=0.0, sr=0.0, ia=0.0, vx=vx)
    assert info.value.row == 4
    vx2 = np.full((2, 3), 10.0)
    vx2[1, 1] = -1.0
    with pytest.raises(mf.MagicFormulaError) as info:
        pac2002.evaluate(fz=4000.0, sa=0.0, sr=0.0, ia=0.0, vx=vx2)
    assert info.value.row == 4


def test_check_false_is_passed_through(pac2002):
    result = pac2002.evaluate(fz=4000.0, sa=0.0, sr=0.0, ia=0.0, vx=-1.0, check=False)
    assert all(np.isfinite(v) for v in result.values())
