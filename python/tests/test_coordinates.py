"""Tests of coordinate-system identification, model conversion and data conversion."""

from __future__ import annotations

import warnings

import numpy as np
import pytest

import magic_formula as mf

ROWS = np.array(
    [[4000, 0.05, 0.1, 0.02, 16.6], [3000, -0.04, -0.05, 0.0, 20.0]],
    dtype=np.float64,
)
SAE_ROWS = ROWS * np.array([-1.0, 1.0, 1.0, 1.0, 1.0])
REL = 1e-12


def _load(data_dir, name):
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', mf.MagicFormulaWarning)
        return mf.load_tir(data_dir / name)


@pytest.fixture(scope='module')
def iso_model(data_dir):
    return _load(data_dir, 'pac2002_S_iso.tir')


@pytest.fixture(scope='module')
def sae_model(data_dir):
    return _load(data_dir, 'pac2002_S_sae.tir')


def test_identify_pac2002_s_is_mixed(pac2002):
    ident = pac2002.find_coordinate_system()
    assert isinstance(ident, mf.CoordinateIdentification)
    assert ident.classification == 'MIXED'
    assert ident.vertical.sign == 'POSITIVE'
    assert ident.lateral.sign == 'NEGATIVE'
    assert ident.vertical.basis == 'DECIDED'
    assert ident.lateral.basis == 'DECIDED'
    assert ident.vertical.confidence == 1.0
    assert ident.lateral.confidence == 1.0
    assert len(ident.readings) == 29
    fz0 = next(r for r in ident.readings if r.name == 'FZ0')
    assert fz0.status == 'VOTE'
    assert fz0.vote == 1
    assert len(ident.warnings) == 1
    assert 'mix conventions' in ident.warnings[0]


def test_identify_iso_and_sae_fixtures(iso_model, sae_model):
    iso = iso_model.find_coordinate_system()
    sae = sae_model.find_coordinate_system()
    assert iso.classification == 'ISO'
    assert sae.classification == 'SAE'
    assert iso.warnings == ()
    assert sae.warnings == ()


def test_reading_fields_types(iso_model):
    ident = iso_model.find_coordinate_system()
    assert isinstance(ident.vertical, mf.AxisDecision)
    assert isinstance(ident.lateral, mf.AxisDecision)
    for decision in (ident.vertical, ident.lateral):
        assert isinstance(decision.sign, str)
        assert isinstance(decision.basis, str)
        assert isinstance(decision.confidence, float)
        assert isinstance(decision.positive_weight, float)
        assert isinstance(decision.negative_weight, float)
    assert ident.effective_vertical == 'POSITIVE'
    assert ident.effective_lateral == 'POSITIVE'
    assert isinstance(ident.readings, tuple)
    assert isinstance(ident.warnings, tuple)
    for r in ident.readings:
        assert isinstance(r, mf.IndicatorReading)
        for text in (r.name, r.meaning, r.unit, r.axis, r.status):
            assert isinstance(text, str)
        for number in (r.load_scale, r.load, r.value, r.normalised, r.weight):
            assert isinstance(number, float)
        assert isinstance(r.vote, int)
        assert r.axis in ('VERTICAL', 'LATERAL', 'CHECK')
    assert isinstance(ident.reference_input, np.ndarray)
    assert ident.reference_input.shape == (5,)
    assert ident.reference_input[0] == pytest.approx(4000.0)


def test_convert_coordinate_system_iso_to_sae_matches_sae_fixture(iso_model, sae_model):
    converted = iso_model.convert_coordinate_system('SAE')
    assert isinstance(converted, mf.TireModel)
    assert converted.find_coordinate_system().classification == 'SAE'
    out = converted.evaluate_batch(SAE_ROWS)
    np.testing.assert_allclose(out, sae_model.evaluate_batch(SAE_ROWS), rtol=REL, atol=1e-9)
    iso_out = iso_model.evaluate_batch(ROWS)
    expected = iso_out.copy()
    for j, channel in enumerate(iso_model.outputs):
        if channel.name in ('FY', 'MZ', 'MY'):
            expected[:, j] = -iso_out[:, j]
    np.testing.assert_allclose(out, expected, rtol=REL, atol=1e-9)
    names = [c.name for c in iso_model.outputs]
    for name in ('FX', 'MX'):
        j = names.index(name)
        np.testing.assert_allclose(out[:, j], iso_out[:, j], rtol=REL, atol=1e-9)


def test_convert_coordinate_system_case_insensitive(iso_model):
    assert iso_model.convert_coordinate_system('sae').find_coordinate_system().classification == 'SAE'
    assert iso_model.convert_coordinate_system('Iso').find_coordinate_system().classification == 'ISO'


def test_convert_coordinate_system_bad_target_raises_invalid_argument(iso_model):
    with pytest.raises(mf.MagicFormulaError) as e:
        iso_model.convert_coordinate_system('ISO-W')
    assert e.value.code == 'INVALID_ARGUMENT'


def test_convert_inputs_flips_only_fz(pac2002):
    expected = ROWS.copy()
    expected[:, 0] *= -1
    flipped = pac2002.convert_inputs(ROWS, source='ISO', target='SAE')
    assert np.array_equal(flipped, expected)
    assert np.array_equal(flipped[:, 1], ROWS[:, 1])
    one = pac2002.convert_inputs(ROWS[0], source='iso', target='sae')
    assert one.shape == ROWS[0].shape
    assert np.array_equal(one, expected[0])


def test_convert_outputs_flips_fy_mz_my(pac2002):
    out = pac2002.evaluate_batch(ROWS)
    flipped = pac2002.convert_outputs(out, source='ISO', target='SAE')
    for j, channel in enumerate(pac2002.outputs):
        sign = -1.0 if channel.name in ('FY', 'MZ', 'MY') else 1.0
        assert np.array_equal(flipped[:, j], sign * out[:, j])
    one = pac2002.convert_outputs(out[0], source='ISO', target='SAE')
    assert np.array_equal(one, flipped[0])


def test_convert_round_trip_exact(pac2002):
    for values in (ROWS, ROWS[0]):
        flipped = pac2002.convert_inputs(values, source='ISO', target='SAE')
        assert np.array_equal(pac2002.convert_inputs(flipped, source='SAE', target='ISO'), values)
    out = pac2002.evaluate_batch(ROWS)
    flipped = pac2002.convert_outputs(out, source='ISO', target='SAE')
    assert np.array_equal(pac2002.convert_outputs(flipped, source='SAE', target='ISO'), out)


def test_convert_bad_system_or_kind_raises(pac2002):
    from magic_formula._model import _checked

    with pytest.raises(mf.MagicFormulaError) as e:
        pac2002.convert_inputs(ROWS, source='ISO-W', target='SAE')
    assert e.value.code == 'INVALID_ARGUMENT'
    with pytest.raises(mf.MagicFormulaError) as e:
        pac2002.convert_outputs(ROWS, source='SAE', target='XYZ')
    assert e.value.code == 'INVALID_ARGUMENT'
    with pytest.raises(mf.MagicFormulaError) as e:
        pac2002.convert_inputs(ROWS[:, :4], source='ISO', target='SAE')
    assert e.value.code == 'INVALID_ARGUMENT'
    with pytest.raises(mf.MagicFormulaError) as e:
        pac2002.convert_inputs(np.zeros((1, 2, 5)), source='ISO', target='SAE')
    assert e.value.code == 'INVALID_ARGUMENT'
    with pytest.raises(mf.MagicFormulaError) as e:
        _checked(pac2002._core.convert(ROWS, 'bogus', 'ISO', 'SAE'))
    assert e.value.code == 'INVALID_ARGUMENT'


def test_evaluate_batch_negative_fz_on_iso_model_warns(iso_model):
    rows = np.array([[4000, 0, 0, 0, 16.6], [-100, 0, 0, 0, 16.6]], dtype=np.float64)
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0 in 1 row') as record:
        iso_model.evaluate_batch(rows)
    assert record[0].filename == __file__


def test_evaluate_batch_check_false_also_warns(iso_model):
    rows = np.array([[-100, 0, 0, 0, 16.6]], dtype=np.float64)
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0 in 1 row'):
        iso_model.evaluate_batch(rows, check=False)


def test_evaluate_batch_shape_error_check_false_raises(iso_model):
    with pytest.raises(mf.MagicFormulaError) as e:
        iso_model.evaluate_batch(np.zeros(5), check=False)
    assert e.value.code == 'INVALID_ARGUMENT'
