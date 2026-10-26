"""Tests of TireModel.characteristic_channels, characteristics_batch and characteristics."""

from __future__ import annotations

import math
from pathlib import Path

import numpy as np
import pytest

import magic_formula as mf

REL_TOL = 1e-9
BATCH_REL_TOL = 1e-12
ELL_TIR = Path(__file__).resolve().parents[2] / 'tests' / 'data' / 'pac2002_S_ell.tir'
NAMES = [
    'DFY_DSA',
    'DFX_DSR',
    'FY_OVER_SA',
    'FX_OVER_SR',
    'DY',
    'DX',
    'SA_AT_PEAK_FY',
    'SR_AT_PEAK_FX',
]
UNITS = ['N/rad', 'N', 'N/rad', 'N', 'N', 'N', 'rad', '-']
KEYS = [n.lower() for n in NAMES]
IDX = {k: i for i, k in enumerate(KEYS)}

# Input order: FZ, SA, SR, IA, VX.
C1 = [4000, 0.0, 0.1, 0, 10]  # sa raw 0, sr 0.1
C2 = [4000, 0.05, 0.0, 0, 10]  # sr raw 0, sa 0.05
C4 = [4000, 0.0, 0.0, 0, 10]


# Per-channel magnitude floors: six force/slope channels, then the two peak slips.
CHANNEL_SCALES = np.array([1e4] * 6 + [0.1, 0.1], dtype=np.float64)


def assert_close_nan_aware(actual, expected):
    """NaN positions must match; finite values agree within BATCH_REL_TOL of the channel scale."""
    actual = np.asarray(actual, dtype=np.float64)
    expected = np.asarray(expected, dtype=np.float64)
    assert actual.shape == expected.shape
    assert np.array_equal(np.isnan(actual), np.isnan(expected))
    scales = np.broadcast_to(CHANNEL_SCALES, actual.shape)
    finite = ~np.isnan(actual)
    a, e, sc = actual[finite], expected[finite], scales[finite]
    assert np.all(np.abs(a - e) <= BATCH_REL_TOL * (sc + np.maximum(np.abs(a), np.abs(e))))


def test_characteristic_channels_names_and_units(pac2002):
    channels = pac2002.characteristic_channels
    assert [c.name for c in channels] == NAMES
    assert [c.unit for c in channels] == UNITS


def test_characteristics_batch_shape_n_by_8(pac2002):
    out = pac2002.characteristics_batch([C1, C2, C4])
    assert out.shape == (3, 8)
    assert out.dtype == np.float64
    assert pac2002.characteristics_batch(np.empty((0, 5))).shape == (0, 8)


def test_characteristics_batch_c1_c2_values(pac2002):
    out = pac2002.characteristics_batch([C1, C2])
    c1, c2 = out[0], out[1]
    assert c1[IDX['dfx_dsr']] == pytest.approx(5151.900623800, rel=REL_TOL)
    assert c1[IDX['fx_over_sr']] == pytest.approx(39354.79640314, rel=REL_TOL)
    assert c1[IDX['sr_at_peak_fx']] == pytest.approx(0.1299038105893, rel=REL_TOL)
    assert c1[IDX['sr_at_peak_fx']] == pytest.approx(math.sqrt(3) / (80000 / (6000 + 1e-6)), rel=REL_TOL)
    assert c1[IDX['dx']] == pytest.approx(4000, rel=REL_TOL)
    assert c2[IDX['dfy_dsa']] == pytest.approx(-34707.69217580, rel=REL_TOL)
    assert c2[IDX['fy_over_sa']] == pytest.approx(-47527.96304983, rel=REL_TOL)
    assert c2[IDX['sa_at_peak_fy']] == pytest.approx(0.2475646539106, rel=REL_TOL)
    assert c2[IDX['dy']] == pytest.approx(4000, rel=REL_TOL)


def test_characteristics_batch_zero_slip_secant_equals_tangent(pac2002):
    out = pac2002.characteristics_batch([C1, C2, C4])
    # C1 has raw sa 0, C2 has raw sr 0, C4 has both 0.
    for row in (0, 2):
        assert out[row, IDX['fy_over_sa']] == out[row, IDX['dfy_dsa']]
    for row in (1, 2):
        assert out[row, IDX['fx_over_sr']] == out[row, IDX['dfx_dsr']]
    assert out[0, IDX['fy_over_sa']] == pytest.approx(-39162.83710, rel=REL_TOL)
    assert out[1, IDX['fx_over_sr']] == pytest.approx(71554.17527, rel=REL_TOL)
    assert out[2, IDX['fy_over_sa']] == pytest.approx(-55384.61537396, rel=REL_TOL)
    assert out[2, IDX['fx_over_sr']] == pytest.approx(79999.99998667, rel=REL_TOL)
    assert not np.isnan(out[:, :6]).any()


def test_characteristics_batch_no_contact_rows(pac2002):
    rows = [
        [0, 0.05, 0.1, 0, 10],
        [0, 0, 0, 0, 10],
        [-100, 0.05, 0.1, 0, 10],
        [-100, 0, 0, 0, 10],
    ]
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0') as record:
        out = pac2002.characteristics_batch(rows)
    assert record[0].filename == __file__
    assert np.all(out[:, :6] == 0.0)
    assert np.all(np.isnan(out[:, 6:]))


def test_characteristics_batch_check_false_skips_validation(pac2002):
    out = pac2002.characteristics_batch([[4000, 0, 0, 0, -1]], check=False)
    assert out.shape == (1, 8)


def test_characteristics_batch_invalid_row_raises_with_row(pac2002):
    with pytest.raises(mf.MagicFormulaError) as info:
        pac2002.characteristics_batch([C1, [4000, 0, 0, 0, -1]])
    assert info.value.code == 'INVALID_INPUT'
    assert info.value.row == 1
    assert info.value.key == 'VX'


def test_characteristics_batch_shape_errors(pac2002):
    for check in (True, False):
        for rows in (np.zeros(5), np.zeros((1, 4))):
            with pytest.raises(mf.MagicFormulaError) as info:
                pac2002.characteristics_batch(rows, check=check)
            assert info.value.code == 'INVALID_ARGUMENT'


def test_characteristics_named_dict_keys(pac2002):
    res = pac2002.characteristics(fz=4000.0, sa=0.05, sr=0.0, ia=0.0, vx=10.0)
    assert list(res) == KEYS


def test_characteristics_named_broadcasting(pac2002):
    sa = np.linspace(-0.1, 0.1, 3)
    sr = np.linspace(-0.1, 0.1, 8).reshape(2, 4, 1) * np.ones((1, 1, 3))
    res = pac2002.characteristics(fz=4000.0, sa=sa, sr=sr, ia=0.0, vx=10.0)
    assert all(v.shape == (2, 4, 3) for v in res.values())


def test_characteristics_named_scalars_zero_d(pac2002):
    res = pac2002.characteristics(fz=4000.0, sa=0.05, sr=0.0, ia=0.0, vx=10.0)
    assert all(v.shape == () for v in res.values())
    assert float(res['dy']) == pytest.approx(4000, rel=REL_TOL)


def test_characteristics_named_missing_input_type_error(pac2002):
    with pytest.raises(TypeError, match=r'^characteristics\(\) missing'):
        pac2002.characteristics(fz=4000.0)


def test_characteristics_named_unknown_input_type_error(pac2002):
    with pytest.raises(TypeError, match=r'^characteristics\(\) got unexpected'):
        pac2002.characteristics(fz=4000.0, sa=0.0, sr=0.0, ia=0.0, vx=10.0, foo=1.0)


def test_characteristics_named_incompatible_shapes_value_error(pac2002):
    with pytest.raises(ValueError):
        pac2002.characteristics(
            fz=4000.0, sa=np.zeros(3), sr=np.zeros(4), ia=0.0, vx=10.0
        )


def test_characteristics_uncombined_model_pure_slopes(data_dir, pac2002):
    uncombined = mf.load_tir(data_dir / 'pac2002_S.tir', combined=False)
    out = uncombined.characteristics_batch([[4000, 0.05, 0.1, 0, 10]])[0]
    assert out[IDX['dfy_dsa']] == pytest.approx(-34707.69217580, rel=REL_TOL)
    assert out[IDX['dfx_dsr']] == pytest.approx(5151.900623800, rel=REL_TOL)
    combined = pac2002.characteristics_batch([[4000, 0.05, 0.1, 0, 10]])[0]
    assert abs(combined[IDX['dfy_dsa']] - out[IDX['dfy_dsa']]) > 1.0


def test_characteristics_ellipse_zero_slip_directional_limit():
    model = mf.load_tir(ELL_TIR)
    c14 = model.characteristics_batch([C2])[0]  # sr 0, sa 0.05
    c15 = model.characteristics_batch([C1])[0]  # sa 0, sr 0.1
    assert c14[IDX['dfx_dsr']] == pytest.approx(56580.32736, rel=REL_TOL)
    assert c15[IDX['dfy_dsa']] == pytest.approx(-32427.16913, rel=REL_TOL)


def test_characteristics_matches_batch_columns(pac2002):
    sa = np.array([0.0, 0.05, -0.05])
    sr = np.array([0.1, 0.0, 0.05])
    res = pac2002.characteristics(fz=4000.0, sa=sa, sr=sr, ia=0.0, vx=10.0)
    rows = np.column_stack([np.full(3, 4000.0), sa, sr, np.zeros(3), np.full(3, 10.0)])
    out = pac2002.characteristics_batch(rows)
    for j, key in enumerate(KEYS):
        assert np.array_equal(res[key], out[:, j], equal_nan=True)


MF52_ROWS = [
    C1,
    C2,
    C4,
    [4000, 0.1, 0.1, 0, 10],
    [3000, -0.05, 0.05, 0.02, 20],
    [0, 0.05, 0.1, 0, 10],
]


def test_characteristics_mf52_channel_names_and_units(mf52):
    channels = mf52.characteristic_channels
    assert [c.name for c in channels] == NAMES
    assert [c.unit for c in channels] == UNITS


def test_characteristics_mf52_batch_shape(mf52):
    out = mf52.characteristics_batch(MF52_ROWS)
    assert out.shape == (len(MF52_ROWS), 8)
    assert out.dtype == np.float64
    assert mf52.characteristics_batch(np.empty((0, 5))).shape == (0, 8)


def test_characteristics_mf52_equals_pac2002_on_s(mf52, pac2002):
    expected = pac2002.characteristics_batch(MF52_ROWS)
    actual = mf52.characteristics_batch(MF52_ROWS)
    assert np.array_equal(actual, expected, equal_nan=True)


def test_characteristics_mf52_named_matches_batch_columns(mf52):
    sa = np.array([0.0, 0.05, -0.05])
    sr = np.array([0.1, 0.0, 0.05])
    res = mf52.characteristics(fz=4000.0, sa=sa, sr=sr, ia=0.0, vx=10.0)
    assert list(res) == KEYS
    rows = np.column_stack([np.full(3, 4000.0), sa, sr, np.zeros(3), np.full(3, 10.0)])
    out = mf52.characteristics_batch(rows)
    for j, key in enumerate(KEYS):
        assert np.array_equal(res[key], out[:, j], equal_nan=True)


def test_characteristics_mf52_per_point_equals_batch(mf52):
    out = mf52.characteristics_batch(MF52_ROWS)
    for i, row in enumerate(MF52_ROWS):
        single = mf52.characteristics_batch([row])[0]
        assert_close_nan_aware(single, out[i])
