"""Model metadata tests."""

from __future__ import annotations

import math

import magic_formula as mf


def test_inputs_are_layout_channels(pac2002):
    assert pac2002.inputs == (
        mf.Channel('FZ', 'N'),
        mf.Channel('SA', 'rad'),
        mf.Channel('SR', '-'),
        mf.Channel('IA', 'rad'),
        mf.Channel('VX', 'm/s'),
    )


def test_outputs_are_layout_channels(pac2002):
    names = ['FX', 'FY', 'MZ', 'MX', 'MY', 'FZ_CLAMPED', 'SR_CLAMPED', 'SA_CLAMPED', 'IA_CLAMPED', 'BELOW_VXLOW']
    units = ['N', 'N', 'N*m', 'N*m', 'N*m', '-', '-', '-', '-', '-']
    assert [c.name for c in pac2002.outputs] == names
    assert [c.unit for c in pac2002.outputs] == units


def test_limits_default_to_open_ranges_and_vxlow(pac2002):
    limits = pac2002.limits
    assert list(limits) == ['FZMIN', 'FZMAX', 'KPUMIN', 'KPUMAX', 'ALPMIN', 'ALPMAX', 'CAMMIN', 'CAMMAX', 'VXLOW']
    for key, value in limits.items():
        if key.endswith('MIN'):
            assert value == -math.inf
        elif key.endswith('MAX'):
            assert value == math.inf
    assert limits['VXLOW'] == 1.0


def test_limits_read_from_range_sections(data_dir):
    model = mf.load_tir(data_dir / 'pac2002_S_rng.tir')
    assert model.limits == {
        'FZMIN': 200,
        'FZMAX': 10000,
        'KPUMIN': -1,
        'KPUMAX': 1,
        'ALPMIN': -1.5,
        'ALPMAX': 1.5,
        'CAMMIN': -0.2,
        'CAMMAX': 0.2,
        'VXLOW': 1.0,
    }


def test_metadata_values(pac2002, mf52):
    assert pac2002.fittyp is None
    assert pac2002.use_mode == 4
    assert pac2002.tyre_side == 'LEFT'
    assert mf52.fittyp == 6
    assert mf52.use_mode == 4


def test_use_mode_absent_is_none(pac2002_text):
    model = mf.parse_tir(pac2002_text.replace('USE_MODE = 4\n', ''))
    assert model.use_mode is None


def test_repr_names_model_and_mode(pac2002):
    assert repr(pac2002) == "TireModel(name='PAC2002', combined=True)"


def test_limits_returns_a_copy(pac2002):
    first = pac2002.limits
    first['VXLOW'] = 99.0
    assert pac2002.limits['VXLOW'] == 1.0
