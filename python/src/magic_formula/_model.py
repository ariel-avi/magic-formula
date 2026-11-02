"""Tire model facade over the compiled ``_core`` extension."""

from __future__ import annotations

import os
import warnings as _warnings
from typing import NamedTuple

import numpy as np
import numpy.typing as npt

from magic_formula import _core
from magic_formula._errors import MagicFormulaError, MagicFormulaWarning


class Channel(NamedTuple):
    """A named input or output channel."""

    name: str  # upper-case layout name, e.g. 'FZ'
    unit: str  # SI unit


def _checked(result):
    """Return ``result`` or raise MagicFormulaError when it is a ``_core.Error``."""
    if isinstance(result, _core.Error):
        raise MagicFormulaError(
            result.text,
            code=result.code,
            message=result.message,
            file=result.file,
            line=result.line,
            key=result.key,
            row=result.row,
        )
    return result


class AxisDecision(NamedTuple):
    """The weighted vote of one axis of the coordinate-system identification."""

    sign: str  # 'POSITIVE' (the ISO side), 'NEGATIVE' (the SAE side) or 'UNDETERMINED'
    basis: str  # 'DECIDED', 'INSUFFICIENT_EVIDENCE' or 'CONFLICTING_EVIDENCE'
    confidence: float  # share of the valid vote weight held by the winning side
    positive_weight: float  # vote weight for the ISO side
    negative_weight: float  # vote weight for the SAE side


class IndicatorReading(NamedTuple):
    """One identification indicator evaluated at one probe load."""

    name: str  # indicator name, e.g. 'dFy/dSA'
    meaning: str  # the physical expectation behind the vote
    unit: str  # SI unit of ``value``
    axis: str  # 'VERTICAL', 'LATERAL' or 'CHECK'
    load_scale: float  # probe load as a multiple of the reference FZ
    load: float  # probe FZ in N
    value: float  # indicator value in ``unit``
    normalised: float  # value / |load|
    status: str  # 'VOTE', 'BELOW_THRESHOLD', 'NOT_FINITE', 'CLAMPED' or 'MISSING_CHANNEL'
    vote: int  # +1 (ISO side), -1 (SAE side) or 0 (abstains)
    weight: float  # vote weight of this reading


class CoordinateIdentification(NamedTuple):
    """The coordinate system a coefficient set is expressed in.

    ``classification`` is ``'ISO'``, ``'SAE'`` or ``'MIXED'`` (the vertical and
    lateral axes disagree). ``effective_vertical`` and ``effective_lateral`` are
    the signs used for the classification (``'POSITIVE'`` or ``'NEGATIVE'``).
    ``reference_input`` is the reference operating point, in input order.
    """

    classification: str
    vertical: AxisDecision
    lateral: AxisDecision
    effective_vertical: str
    effective_lateral: str
    readings: tuple[IndicatorReading, ...]
    reference_input: npt.NDArray[np.float64]
    warnings: tuple[str, ...]


class TireModel:
    """A loaded tire model.

    Units are SI (N, m, rad, m/s, Pa, 1/m). The equations never convert axis systems:
    inputs are used as given and outputs are in the model's own system (see
    ``find_coordinate_system`` and ``convert_coordinate_system``). Input names and
    column order come from ``inputs``: MF 5.x uses ``FZ, SA, SR, IA, VX``;
    MF 6.1 appends ``P`` [Pa] and ``TS`` [1/m]. Named evaluation requires pressure
    for MF 6.1 and defaults omitted turn slip to zero. Batch matrices must supply
    every input column in layout order. Outputs are listed in ``outputs``.
    A model is immutable after loading and the GIL is released during
    evaluation, so threads may share one instance. Do not mutate an input array
    from another thread while a call is using it.
    """

    __slots__ = ('_core', '_inputs', '_outputs', '_characteristics', '_limits')

    def __init__(self, core: _core.Model) -> None:
        """Wrap a ``_core.Model``; use ``load_tir`` or ``parse_tir`` instead."""
        self._core = core
        self._inputs = tuple(Channel(n, u) for n, u in core.inputs)
        self._outputs = tuple(Channel(n, u) for n, u in core.outputs)
        self._characteristics = tuple(Channel(n, u) for n, u in core.characteristics)
        self._limits = dict(core.limits)

    def __repr__(self) -> str:
        """Return e.g. ``TireModel(name='PAC2002', combined=True)``."""
        return f'TireModel(name={self.name!r}, combined={self.combined!r})'

    @property
    def name(self) -> str:
        """Model name, e.g. ``'PAC2002'``."""
        return self._core.name

    @property
    def combined(self) -> bool:
        """Whether combined-slip coefficients are used."""
        return self._core.combined

    @property
    def turn_slip(self) -> bool:
        """Whether MF6.1 turn-slip corrections are enabled."""
        return self._core.turn_slip

    @property
    def inputs(self) -> tuple[Channel, ...]:
        """Input channels in column order."""
        return self._inputs

    @property
    def outputs(self) -> tuple[Channel, ...]:
        """Output channels in column order."""
        return self._outputs

    @property
    def characteristic_channels(self) -> tuple[Channel, ...]:
        """Characteristic channels in column order.

        DFY_DSA [N/rad], DFX_DSR [N], FY_OVER_SA [N/rad], FX_OVER_SR [N],
        DY [N], DX [N], SA_AT_PEAK_FY [rad], SR_AT_PEAK_FX [-].
        """
        return self._characteristics

    @property
    def limits(self) -> dict[str, float]:
        """Validity limits (a fresh copy on each access), in layout order."""
        return dict(self._limits)

    @property
    def fittyp(self) -> int | None:
        """The FITTYP value of the file, ``None`` when absent."""
        return self._core.fittyp

    @property
    def use_mode(self) -> int | None:
        """The USE_MODE value of the file, ``None`` when absent."""
        return self._core.use_mode

    @property
    def tyre_side(self) -> str:
        """Tyre side, ``'LEFT'`` or ``'RIGHT'``."""
        return self._core.tyre_side

    @property
    def warnings(self) -> tuple[str, ...]:
        """Diagnostics produced while loading."""
        return tuple(self._core.warnings)

    @property
    def ignored_keys(self) -> tuple[str, ...]:
        """File keys that the model does not use."""
        return tuple(self._core.ignored_keys)

    def find_coordinate_system(self) -> CoordinateIdentification:
        """Identify the coordinate system the coefficient set is expressed in.

        ISO is x forward, y left, z up (a loaded tire has FZ > 0 and
        dFy/dSA > 0); SAE is x forward, y right, z down (FZ < 0 and
        dFy/dSA < 0). The vertical and lateral axes are identified separately
        from several evaluated indicators; ``'MIXED'`` means they disagree.
        """
        r = self._core.find_coordinate_system()

        def decision(d) -> AxisDecision:
            return AxisDecision(d.sign, d.basis, d.confidence, d.positive_weight, d.negative_weight)

        readings = tuple(
            IndicatorReading(
                x.name, x.meaning, x.unit, x.axis, x.load_scale, x.load,
                x.value, x.normalised, x.status, x.vote, x.weight,
            )
            for x in r.readings
        )
        return CoordinateIdentification(
            classification=r.classification,
            vertical=decision(r.vertical),
            lateral=decision(r.lateral),
            effective_vertical=r.effective_vertical,
            effective_lateral=r.effective_lateral,
            readings=readings,
            reference_input=np.asarray(r.reference_input, dtype=np.float64),
            warnings=tuple(r.warnings),
        )

    def convert_coordinate_system(self, target: str) -> TireModel:
        """Return a model converted to ``target`` by flipping coefficient signs.

        The equations never change. ``target`` is ``'ISO'`` or ``'SAE'``
        (case-insensitive).

        Raises:
            MagicFormulaError: unknown system.
        """
        return TireModel(_checked(self._core.convert_coordinate_system(target)))

    def _convert(
        self, values: npt.ArrayLike, kind: str, source: str, target: str
    ) -> npt.NDArray[np.float64]:
        a = np.asarray(values, dtype=np.float64)
        one_row = a.ndim == 1
        result = _checked(
            self._core.convert(a.reshape(1, -1) if one_row else a, kind, source, target)
        )
        return result.reshape(-1) if one_row else result

    def convert_inputs(
        self, values: npt.ArrayLike, *, source: str, target: str
    ) -> npt.NDArray[np.float64]:
        """Convert input rows between ``'ISO'`` and ``'SAE'`` (data utility).

        Names are case-insensitive. Only FZ changes sign; SA, SR, IA and VX
        keep their meaning. A 1-D array is one row. Evaluation never calls this.

        Raises:
            MagicFormulaError: unknown system, wrong dimensions or columns.
        """
        return self._convert(values, 'inputs', source, target)

    def convert_outputs(
        self, values: npt.ArrayLike, *, source: str, target: str
    ) -> npt.NDArray[np.float64]:
        """Convert output rows between ``'ISO'`` and ``'SAE'``.

        FY, MZ and MY change sign; FX, MX and the flags do not. See
        ``convert_inputs`` for names and shapes.
        """
        return self._convert(values, 'outputs', source, target)

    def evaluate_batch(
        self,
        inputs: npt.ArrayLike,
        *,
        check: bool = True,
    ) -> npt.NDArray[np.float64]:
        """Evaluate rows of operating points in the model's own coordinate system.

        Args:
            inputs: array-like of shape ``(N, n_inputs)``, with every column in
                the model's ``inputs`` order. MF 5.x uses FZ [N], SA [rad], SR [-],
                IA [rad], VX [m/s]; MF 6.1 appends P [Pa] and TS [1/m]. Matrix
                calls require all columns, including TS even when it is zero.
            check: when true, validate every row and raise MagicFormulaError on
                the first invalid or non-finite one; when false, skip validation.
                The shape is always checked.

        Returns:
            A float64 array of shape ``(N, n_outputs)``. Rows with FZ on the
            unloaded side of the model are lift-off (zero forces and moments)
            and are reported as MagicFormulaWarning.
        """
        return self._run(self._core.evaluate_batch, inputs, check, stacklevel=3)

    def _run(self, core_method, matrix, check: bool, *, stacklevel: int) -> npt.NDArray[np.float64]:
        """Run a ``_core`` batch method and re-emit its warnings at ``stacklevel``."""
        solution = _checked(core_method(matrix, check))
        for message in solution.warnings:
            _warnings.warn(message, MagicFormulaWarning, stacklevel=stacklevel)
        return solution.outputs

    def _pack(
        self, inputs: dict[str, npt.ArrayLike], method: str
    ) -> tuple[tuple[int, ...], np.ndarray]:
        """Validate names, broadcast, and build the F-ordered input matrix."""
        names = [c.name.lower() for c in self._inputs]
        unknown = sorted(set(inputs) - set(names))
        if unknown:
            raise TypeError(
                f'{method}() got unexpected input(s) for {self.name}: '
                f'{", ".join(unknown)}; expected {", ".join(names)}'
            )
        if 'ts' in names:
            inputs = {'ts': 0.0, **inputs}
        missing = [n for n in names if n not in inputs]
        if missing:
            raise TypeError(
                f'{method}() missing input(s) for {self.name}: {", ".join(missing)}'
            )
        arrays = np.broadcast_arrays(
            *(np.asarray(inputs[n], dtype=np.float64) for n in names)
        )
        shape = arrays[0].shape
        matrix = np.empty((arrays[0].size, len(names)), dtype=np.float64, order='F')
        for j, a in enumerate(arrays):
            matrix[:, j] = a.reshape(-1)
        return shape, matrix

    def evaluate(
        self,
        *,
        check: bool = True,
        **inputs: npt.ArrayLike,
    ) -> dict[str, npt.NDArray[np.float64]]:
        """Evaluate named channels with NumPy broadcasting.

        Args:
            check: see ``evaluate_batch``.
            **inputs: array-likes keyed by the lower-case channel names in
                ``self.inputs``; SI units and radians. MF 5.x requires fz, sa,
                sr, ia and vx. MF 6.1 also requires p [Pa]; omitted ts [1/m]
                defaults to zero. Values are broadcast to a common shape.

        Returns:
            A dict of float64 arrays keyed by lower-case output name, each with
            the broadcast shape (0-d for scalar inputs). ``MagicFormulaError.row``
            is the flat C-order index into the broadcast inputs.

        Raises:
            TypeError: an input is missing or unknown.
            ValueError: the input shapes do not broadcast.
        """
        shape, matrix = self._pack(inputs, 'evaluate')
        out = self._run(self._core.evaluate_batch, matrix, check, stacklevel=3)
        return {c.name.lower(): out[:, j].reshape(shape) for j, c in enumerate(self._outputs)}

    def characteristics_batch(
        self,
        inputs: npt.ArrayLike,
        *,
        check: bool = True,
    ) -> npt.NDArray[np.float64]:
        """Evaluate the tire characteristics of rows of operating points.

        The channels are listed in ``characteristic_channels``: the slopes
        ``DFY_DSA`` and ``DFX_DSR`` of the full (combined) force at the row, the
        secants ``FY_OVER_SA`` and ``FX_OVER_SR`` (force over raw slip), the pure
        slip peak forces ``DY`` and ``DX``, and the slips at the peaks
        ``SA_AT_PEAK_FY`` and ``SR_AT_PEAK_FX``. All are analytic.

        NaN occurs only in ``SA_AT_PEAK_FY`` and ``SR_AT_PEAK_FX`` (no stationary
        point in range, or no contact). At zero slip the secant equals the slope
        (``FY_OVER_SA == DFY_DSA``, ``FX_OVER_SR == DFX_DSR``). Rows without
        contact give 0 in every channel except the two peak slips.

        Args:
            inputs: see ``evaluate_batch``.
            check: see ``evaluate_batch``.

        Returns:
            A float64 array of shape ``(N, 8)``. Load-side warnings behave as in
            ``evaluate_batch``.
        """
        return self._run(self._core.evaluate_characteristics_batch, inputs, check, stacklevel=3)

    def characteristics(
        self,
        *,
        check: bool = True,
        **inputs: npt.ArrayLike,
    ) -> dict[str, npt.NDArray[np.float64]]:
        """Evaluate the characteristics of named channels with NumPy broadcasting.

        Inputs and errors are as in ``evaluate``. Returns a dict keyed
        ``dfy_dsa, dfx_dsr, fy_over_sa, fx_over_sr, dy, dx, sa_at_peak_fy,
        sr_at_peak_fx``, each with the broadcast shape. NaN occurs only in
        ``sa_at_peak_fy`` and ``sr_at_peak_fx``; at zero slip ``fy_over_sa ==
        dfy_dsa`` and ``fx_over_sr == dfx_dsr``; rows without contact give 0.
        """
        shape, matrix = self._pack(inputs, 'characteristics')
        out = self._run(self._core.evaluate_characteristics_batch, matrix, check, stacklevel=3)
        return {c.name.lower(): out[:, j].reshape(shape) for j, c in enumerate(self._characteristics)}


def _wrap(result) -> TireModel:
    """Build a TireModel from a ``_core`` result and re-emit its load warnings."""
    model = TireModel(_checked(result))
    for text in model.warnings:
        _warnings.warn(text, MagicFormulaWarning, stacklevel=3)
    return model


def load_tir(path: str | os.PathLike[str], *, combined: bool = True, turn_slip: bool = False) -> TireModel:
    """Load a tire model from a ``.tir`` file.

    Args:
        path: file path.
        combined: use the combined-slip coefficients.
        turn_slip: enable MF6.1 turn-slip corrections (default false).

    Raises:
        MagicFormulaError: the file is unreadable, invalid or an unsupported model.
    """
    return _wrap(_core.load_tir(os.fspath(path), combined, turn_slip))


def parse_tir(text: str, *, file_name: str = '', combined: bool = True, turn_slip: bool = False) -> TireModel:
    """Parse a tire model from ``.tir`` text.

    Args:
        text: the file content.
        file_name: name used in error locations.
        combined: use the combined-slip coefficients.
        turn_slip: enable MF6.1 turn-slip corrections (default false).

    Raises:
        MagicFormulaError: the text is invalid or an unsupported model.
    """
    return _wrap(_core.parse_tir(text, file_name, combined, turn_slip))
