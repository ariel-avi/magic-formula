"""Loading and parsing tests."""

from __future__ import annotations

import warnings

import numpy as np
import pytest

import magic_formula as mf

BAD_TEXT = '[A]\nX = 1\nthis is not an entry\n'


def test_load_tir_pac2002_accepts_str_and_path(data_dir):
    path = data_dir / 'pac2002_S.tir'
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter('always')
        models = [mf.load_tir(str(path)), mf.load_tir(path)]
    assert not [w for w in caught if issubclass(w.category, mf.MagicFormulaWarning)]
    for model in models:
        assert model.name == 'PAC2002'
        assert model.combined is True
        assert model.warnings == ()


def test_load_tir_mf52_warns_about_foreign_camber_factors(data_dir):
    with pytest.warns(mf.MagicFormulaWarning, match='LGAX, LGAY, LGAZ') as record:
        model = mf.load_tir(data_dir / 'mf52_S52.tir')
    warned = [w for w in record if issubclass(w.category, mf.MagicFormulaWarning)]
    assert len(warned) == 1
    assert warned[0].filename == __file__
    assert len(model.warnings) == 1
    assert 'LGAX, LGAY, LGAZ' in model.warnings[0]
    assert model.name == 'MF 5.2'


def test_load_tir_missing_file_raises_io(data_dir):
    with pytest.raises(mf.MagicFormulaError) as info:
        mf.load_tir(data_dir / 'does_not_exist.tir')
    assert info.value.code == 'IO'
    assert info.value.row is None
    assert 'does_not_exist.tir' in str(info.value)


def test_load_tir_mf62_raises_unsupported_model(data_dir):
    with pytest.raises(mf.MagicFormulaError) as info:
        mf.load_tir(data_dir / 'mf62_unsupported.tir')
    assert info.value.code == 'UNSUPPORTED_MODEL'


def test_load_tir_partial_combined_raises_missing_key(data_dir):
    with pytest.raises(mf.MagicFormulaError) as info:
        mf.load_tir(data_dir / 'pac2002_S_part.tir')
    e = info.value
    assert e.code == 'MISSING_KEY'
    assert e.key == 'RHX1'
    assert e.file.endswith('pac2002_S_part.tir')
    assert e.line == 0  # the missing key has no source line
    assert str(e).startswith(e.file)
    assert 'RHX1' in str(e)
    assert e.row is None


def test_parse_tir_matches_load_tir(pac2002, pac2002_text, s_vectors):
    parsed = mf.parse_tir(pac2002_text)
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0'):
        from_parsed = parsed.evaluate_batch(s_vectors)
    with pytest.warns(mf.MagicFormulaWarning, match='FZ < 0'):
        from_loaded = pac2002.evaluate_batch(s_vectors)
    assert np.array_equal(from_parsed, from_loaded)


def test_parse_tir_syntax_error_reports_line():
    with pytest.raises(mf.MagicFormulaError) as info:
        mf.parse_tir(BAD_TEXT)
    assert info.value.code == 'SYNTAX'
    assert info.value.line == 3
    assert info.value.file == ''
    assert str(info.value).startswith('line 3: ')


def test_parse_tir_file_name_in_error_location():
    with pytest.raises(mf.MagicFormulaError) as info:
        mf.parse_tir(BAD_TEXT, file_name='custom.tir')
    assert info.value.file == 'custom.tir'
    assert str(info.value).startswith('custom.tir:3: ')


def test_parse_tir_unsupported_model_raises(data_dir):
    with pytest.raises(mf.MagicFormulaError) as info:
        mf.parse_tir((data_dir / 'mf62_unsupported.tir').read_text())
    assert info.value.code == 'UNSUPPORTED_MODEL'


def test_combined_false_builds_uncombined_model(data_dir):
    model = mf.load_tir(data_dir / 'pac2002_S.tir', combined=False)
    assert model.combined is False
    assert 'combined=False' in repr(model)


def test_ignored_keys_lists_unused_keys(data_dir):
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', mf.MagicFormulaWarning)
        model = mf.load_tir(data_dir / 'mf52_user_pff.tir')
    assert isinstance(model.ignored_keys, tuple)
    assert 'USER_SUB_ID' in model.ignored_keys


def test_tyre_side_right_warns(pac2002_text):
    text = pac2002_text.replace("TYRESIDE = 'LEFT'", "TYRESIDE = 'RIGHT'")
    with pytest.warns(mf.MagicFormulaWarning, match='RIGHT'):
        model = mf.parse_tir(text)
    assert model.tyre_side == 'RIGHT'


def test_error_pickle_round_trip():
    import pickle

    original = mf.MagicFormulaError(
        'f.tir:3: boom', code='SYNTAX', message='boom', file='f.tir', line=3, key='K', row=7
    )
    clone = pickle.loads(pickle.dumps(original))
    assert isinstance(clone, mf.MagicFormulaError)
    assert str(clone) == str(original)
    assert (clone.code, clone.message, clone.file, clone.line, clone.key, clone.row) == (
        'SYNTAX', 'boom', 'f.tir', 3, 'K', 7)
