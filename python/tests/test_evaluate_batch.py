"""Tests of TireModel.evaluate_batch."""

from __future__ import annotations

import math

import numpy as np
import pytest

import magic_formula as mf

# Closed-form values agree to accumulated rounding of the model chain.
REL_TOL_CLOSED_FORM = 1e-9
# Reference values printed with 2 decimals.
PRINTED_TOL = 0.005
# Batch vs single-row evaluation differ by a few ULP at most.
BATCH_REL_TOL = 1e-12


def _error(model, rows, **kwargs):
    with pytest.raises(mf.MagicFormulaError) as info:
        model.evaluate_batch(rows, **kwargs)
    return info.value


def test_s_vectors_match_hand_derived_values(pac2002, s_vectors):
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0'):
        out = pac2002.evaluate_batch(s_vectors)
    expected_fx = 4000 * math.sin(1.5 * math.atan(4 / 3))
    assert out[0, 0] == pytest.approx(expected_fx, rel=REL_TOL_CLOSED_FORM)
    assert out[1, 1] == pytest.approx(-2376.40, abs=PRINTED_TOL)
    assert out[1, 2] == pytest.approx(60.46, abs=PRINTED_TOL)
    assert out[2, 0] == pytest.approx(2782.80, abs=PRINTED_TOL)
    assert out[3, 1] == pytest.approx(-1680.37, abs=PRINTED_TOL)
    for row in range(4):
        assert out[row, 4] == pytest.approx(-12, rel=REL_TOL_CLOSED_FORM)
    assert np.all(out[4:, :5] == 0.0)


def test_result_is_float64_rows_by_outputs(pac2002, s_vectors):
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0'):
        out = pac2002.evaluate_batch(s_vectors)
    assert out.shape == (6, 10)
    assert out.dtype == np.float64
    assert out.flags.f_contiguous


def test_c_order_lists_and_ints_equal_fortran_result(pac2002, s_vectors):
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0'):
        expected = pac2002.evaluate_batch(s_vectors)
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0'):
        assert np.array_equal(pac2002.evaluate_batch(np.ascontiguousarray(s_vectors)), expected)
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0'):
        assert np.array_equal(pac2002.evaluate_batch(s_vectors.tolist()), expected)
    ints = np.array([[4000, 0, 0, 0, 10]])
    floats = np.array([[4000.0, 0.0, 0.0, 0.0, 10.0]], order='F')
    assert np.array_equal(pac2002.evaluate_batch(ints), pac2002.evaluate_batch(floats))


def test_zero_rows_gives_empty_result(pac2002):
    out = pac2002.evaluate_batch(np.empty((0, 5)))
    assert out.shape == (0, 10)
    assert out.dtype == np.float64


def test_one_dimensional_input_raises_invalid_argument(pac2002):
    e = _error(pac2002, np.zeros(5))
    assert e.code == 'INVALID_ARGUMENT'
    assert e.row is None
    assert '2-D' in e.message
    assert 'got 1 dimension(s)' in e.message
    assert 'FZ, SA, SR, IA, VX' in e.message


def test_wrong_column_count_raises_invalid_argument(pac2002):
    e = _error(pac2002, np.zeros((2, 4)))
    assert e.code == 'INVALID_ARGUMENT'
    assert e.message == 'inputs must have 5 columns (FZ, SA, SR, IA, VX), got 4'


def test_negative_vx_raises_invalid_input_with_row(pac2002):
    e = _error(pac2002, [[4000, 0, 0, 0, 10], [4000, 0, 0, 0, -1]])
    assert e.code == 'INVALID_INPUT'
    assert e.row == 1
    assert e.key == 'VX'
    assert str(e).startswith('row 1: ')


def test_nan_input_raises_invalid_input(pac2002):
    e = _error(pac2002, [[math.nan, 0, 0, 0, 10]])
    assert e.code == 'INVALID_INPUT'
    assert e.row == 0
    assert e.key == 'FZ'


def test_overflowing_fz_raises_numerical_with_row(pac2002):
    e = _error(pac2002, [[4000, 0, 0, 0, 16.6], [1e308, 0, 0, 0, 16.6]])
    assert e.code == 'NUMERICAL'
    assert e.row == 1
    assert e.key in {c.name for c in pac2002.outputs}


def test_check_false_skips_validation(pac2002):
    out = pac2002.evaluate_batch([[4000, 0, 0, 0, -1]], check=False)
    assert out.shape == (1, 10)
    assert np.all(np.isfinite(out))
    out = pac2002.evaluate_batch([[math.nan, 0, 0, 0, 10]], check=False)
    # Unchecked: NaN is not rejected; the result is unspecified but well-formed.
    assert out.shape == (1, 10)


def test_check_false_still_rejects_wrong_shape(pac2002):
    for rows in (np.zeros(5), np.zeros((1, 4))):
        assert _error(pac2002, rows, check=False).code == 'INVALID_ARGUMENT'


def test_non_numeric_input_raises_type_error(pac2002):
    with pytest.raises(TypeError):
        pac2002.evaluate_batch([['a', 'b', 'c', 'd', 'e']])


def test_uncombined_returns_pure_slip_forces(data_dir, pac2002):
    uncombined = mf.load_tir(data_dir / 'pac2002_S.tir', combined=False)
    fy_unc = uncombined.evaluate_batch([[4000, 0.05, 0.1, 0, 10]])[0, 1]
    fy_pure = pac2002.evaluate_batch([[4000, 0.05, 0, 0, 10]])[0, 1]
    fy_comb = pac2002.evaluate_batch([[4000, 0.05, 0.1, 0, 10]])[0, 1]
    assert fy_unc == pytest.approx(fy_pure, rel=REL_TOL_CLOSED_FORM)
    assert abs(fy_unc - fy_comb) > 1.0


def test_flags_mark_clamped_inputs_and_low_speed(data_dir):
    model = mf.load_tir(data_dir / 'pac2002_S_rng.tir')
    out = model.evaluate_batch([[4000, 0, 10, 0, 10], [4000, 0, 0.1, 0, 0.5], [4000, 0, 0.1, 0, 10]])
    names = [c.name for c in model.outputs]
    flags = {n: out[:, names.index(n)] for n in names[5:]}
    assert flags['SR_CLAMPED'][0] == 1.0
    for n in ('FZ_CLAMPED', 'SA_CLAMPED', 'IA_CLAMPED', 'BELOW_VXLOW'):
        assert flags[n][0] == 0.0
    assert flags['BELOW_VXLOW'][1] == 1.0
    assert flags['SR_CLAMPED'][1] == 0.0
    assert all(flags[n][2] == 0.0 for n in flags)


def test_large_batch_matches_single_rows(pac2002):
    rng = np.random.default_rng(12345)
    n = 1000
    rows = np.column_stack(
        [
            rng.uniform(500, 8000, n),
            rng.uniform(-0.3, 0.3, n),
            rng.uniform(-0.5, 0.5, n),
            rng.uniform(-0.1, 0.1, n),
            rng.uniform(1, 40, n),
        ]
    )
    batch = pac2002.evaluate_batch(rows)
    scales = np.array([4000, 4000, 1200, 1200, 1200, 1, 1, 1, 1, 1], dtype=np.float64)
    for i in range(n):
        single = pac2002.evaluate_batch(rows[i][None, :])[0]
        bound = BATCH_REL_TOL * (scales + np.maximum(np.abs(single), np.abs(batch[i])))
        assert np.all(np.abs(single - batch[i]) <= bound), i


def test_mf52_zero_vx_raises_invalid_input(mf52, pac2002):
    e = _error(mf52, [[4000, 0, 0, 0, 0]])
    assert e.code == 'INVALID_INPUT'
    assert e.key == 'VX'
    assert e.row == 0
    assert pac2002.evaluate_batch([[4000, 0, 0, 0, 0]]).shape == (1, 10)


def test_load_side_warning_points_at_the_caller(pac2002, s_vectors):
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0') as batch:
        pac2002.evaluate_batch(s_vectors)
    assert batch[0].filename == __file__
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0') as named:
        pac2002.evaluate(fz=-1000.0, sa=0.0, sr=0.0, ia=0.0, vx=10.0)
    assert named[0].filename == __file__
