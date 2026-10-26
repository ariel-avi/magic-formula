"""Pacejka Magic Formula tire models (PAC2002, MF 5.2) from the magic-formula C++ library.

All quantities are SI (N, m, rad, m/s). Evaluation never converts axis systems: inputs are used as given.
``find_coordinate_system`` identifies whether a model is ISO, SAE or MIXED, and ``convert_coordinate_system`` returns a model
converted between ISO and SAE by flipping coefficient signs. Example::

    import numpy as np
    import magic_formula as mf

    model = mf.load_tir('tire.tir')
    out = model.evaluate(fz=4000.0, sa=np.linspace(-0.2, 0.2, 101), sr=0.0, ia=0.0, vx=20.0)
    fy = out['fy']
"""

from __future__ import annotations

from magic_formula import _core
from magic_formula._errors import MagicFormulaError, MagicFormulaWarning
from magic_formula._model import (
    AxisDecision,
    Channel,
    CoordinateIdentification,
    IndicatorReading,
    TireModel,
    load_tir,
    parse_tir,
)

__version__: str = _core.version()

__all__ = [
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
