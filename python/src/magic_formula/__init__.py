"""Pacejka Magic Formula tire models (PAC2002, MF 5.2) from the magic-formula C++ library.

All quantities are SI (N, m, rad, m/s) in the ISO-W axis system. Example::

    import numpy as np
    import magic_formula as mf

    model = mf.load_tir('tire.tir')
    out = model.evaluate(fz=4000.0, sa=np.linspace(-0.2, 0.2, 101), sr=0.0, ia=0.0, vx=20.0)
    fy = out['fy']
"""

from __future__ import annotations

from magic_formula import _core
from magic_formula._errors import MagicFormulaError, MagicFormulaWarning
from magic_formula._model import Channel, TireModel, load_tir, parse_tir

__version__: str = _core.version()

__all__ = [
    'Channel',
    'MagicFormulaError',
    'MagicFormulaWarning',
    'TireModel',
    'load_tir',
    'parse_tir',
]
