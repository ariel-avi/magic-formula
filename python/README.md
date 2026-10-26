# magic-formula for Python

Python bindings for the `magic-formula` C++ library: Pacejka Magic Formula tire models **PAC2002** and **MF 5.2**,
with combined and uncombined slip, evaluated on NumPy arrays.

The package is a pybind11 extension, `magic_formula._core`, that statically embeds the C++ library (`mflib`). The wheel
ships one native file. A thin Python layer adds the Python-style API (keywords, broadcasting, exceptions, warnings).

- Distribution name: `magic-formula`; import name: `magic_formula`.
- Python >= 3.10, NumPy >= 1.24.

## Requirements

- Python >= 3.10 (3.12 or 3.13 recommended)
- CMake >= 4.0, Ninja, Conan 2
- Linux: GCC 14 or Clang 19. macOS: Homebrew LLVM 21. Windows: untested (see below).

## Install

All commands run from the repository root. The build uses the Conan Release toolchain, so run the Release
`conan install` first.

### Linux

```bash
conan install . --build=missing -s build_type=Release -s compiler.cppstd=26 -o with_benchmark=False
python3.12 -m venv .venv
CC=gcc-14 CXX=g++-14 .venv/bin/python -m pip install ./python
```

Use `"./python[test]"` to add the test dependencies, or `"./python[notebook]"` for JupyterLab and matplotlib.

### macOS (Homebrew LLVM 21)

Run the same `conan install` with the Homebrew LLVM 21 settings from the "Platform support" section of the root `README.md` (the `CC`, `CXX`
and `SDKROOT` environment, `tools.build:exelinkflags` and `tools.build:sharedlinkflags` for the Homebrew libc++, and
`tools.build:defines=["_LIBCPP_DISABLE_AVAILABILITY"]`). Then use the same compiler environment for pip:

```bash
python3.12 -m venv .venv
CC=/opt/homebrew/opt/llvm@21/bin/clang CXX=/opt/homebrew/opt/llvm@21/bin/clang++ \
  SDKROOT=$(xcrun --show-sdk-path) .venv/bin/python -m pip install ./python
```

Use `python3.12` or `python3.13`, not Homebrew's newest Python.

### Windows

Untested. Use an MSVC developer prompt, run `conan install` with `-s compiler.cppstd=23`, and set
`CMAKE_GENERATOR=Ninja` before `pip install ./python`.

## Quick start

```python
import numpy as np
import magic_formula as mf

tire = mf.load_tir("tests/data/pac2002_S.tir")

# One array per input channel; NumPy broadcasting. Angles in rad.
alpha = np.deg2rad(np.linspace(-15, 15, 301))
res = tire.evaluate(fz=4000.0, sa=alpha, sr=0.0, ia=0.0, vx=10.0)
fy = res["fy"]            # shape (301,)

# Matrix API: (N, 5) in the order FZ, SA, SR, IA, VX -> (N, 10) out.
points = np.array([[4000.0, 0.0, 0.1, 0.0, 10.0]])
out = tire.evaluate_batch(points)
```

## API reference

### Loading

- `load_tir(path, *, combined=True) -> TireModel`: loads a `.tir` file (`str` or path-like).
- `parse_tir(text, *, file_name='', combined=True) -> TireModel`: parses `.tir` text; `file_name` appears in error
  locations.

`combined=True` builds the combined-slip model, `combined=False` the pure-slip model. The loader picks PAC2002 or
MF 5.2 from the file.

### `TireModel`

Immutable properties:

| Property | Meaning |
|----------|---------|
| `name` | `"PAC2002"` or `"MF 5.2"` |
| `combined` | `True` for combined slip |
| `inputs` | tuple of `Channel(name, unit)` in input order (FZ, SA, SR, IA, VX) |
| `outputs` | tuple of `Channel(name, unit)` in output order |
| `characteristic_channels` | tuple of `Channel(name, unit)` of the characteristics (below) |
| `limits` | `dict[str, float]`; absent bounds are `-inf` / `+inf`; includes `VXLOW` |
| `fittyp`, `use_mode` | `[MODEL]` metadata, `None` when absent |
| `tyre_side` | `"LEFT"` or `"RIGHT"` |
| `warnings` | load warnings (strings) |
| `ignored_keys` | keys present in the file but used by nothing |

Methods:

- `evaluate_batch(inputs, *, check=True) -> ndarray`: `inputs` is an `(N, 5)` array-like (any float-convertible dtype,
  C or Fortran order, nested lists). Returns an `(N, 10)` float64 array with columns FX, FY, MZ, MX, MY, FZ_CLAMPED,
  SR_CLAMPED, SA_CLAMPED, IA_CLAMPED, BELOW_VXLOW.
- `evaluate(*, check=True, **inputs) -> dict[str, ndarray]`: keywords `fz, sa, sr, ia, vx` (scalars or array-likes,
  all required). Inputs are broadcast with NumPy rules. Returns a dict keyed `fx, fy, mz, mx, my, fz_clamped,
  sr_clamped, sa_clamped, ia_clamped, below_vxlow`, each with the broadcast shape (0-d arrays for scalars).
- `characteristics_batch(inputs, *, check=True) -> ndarray`: same inputs as `evaluate_batch`; returns an `(N, 8)`
  float64 array with columns DFY_DSA [N/rad], DFX_DSR [N], FY_OVER_SA [N/rad], FX_OVER_SR [N], DY [N], DX [N],
  SA_AT_PEAK_FY [rad], SR_AT_PEAK_FX [-].
- `characteristics(*, check=True, **inputs) -> dict[str, ndarray]`: like `evaluate`, keyed `dfy_dsa, dfx_dsr,
  fy_over_sa, fx_over_sr, dy, dx, sa_at_peak_fy, sr_at_peak_fx`.
- `find_coordinate_system() -> CoordinateIdentification`, `convert_coordinate_system(target) -> TireModel`,
  `convert_inputs(values, *, source, target)` and `convert_outputs(values, *, source, target)`: see
  "Coordinate systems" below.

Characteristics are analytic (no finite differences). DFY_DSA and DFX_DSR are the derivatives of the full combined
force at the row; FY_OVER_SA and FX_OVER_SR are force over raw slip; DY and DX are the pure-slip peak forces; the last
two channels are the slips at the peaks, on the side of the row's slip. NaN occurs only in SA_AT_PEAK_FY and
SR_AT_PEAK_FX (no peak in the slip range, or no contact). At zero slip the secant equals the slope
(`fy_over_sa == dfy_dsa`, `fx_over_sr == dfx_dsr`). Rows without contact give 0 in every other channel.

```python
c = tire.characteristics(fz=4000.0, sa=0.0, sr=0.1, ia=0.0, vx=10.0)
c["dfx_dsr"], c["sr_at_peak_fx"]
```

`check=True` validates inputs and outputs. `check=False` is the unchecked fast path: values are computed as is (NaN in
gives NaN out). A wrong input shape is always an error, whatever `check` is.

## Coordinate systems

- **ISO**: x forward, y left, z up. A loaded tire has FZ > 0 and dFy/dSA > 0.
- **SAE**: x forward, y right, z down. A loaded tire has FZ < 0 and dFy/dSA < 0.
- Slip angle, slip ratio and inclination angle have the same meaning in both systems. FZ, FY, MZ and MY change sign
  between them; FX and MX do not.
- Evaluation never converts anything: inputs go straight to the equations, and results are in the model's own system.
  A model expressed in SAE expects FZ < 0.

`find_coordinate_system()` identifies the system of a model from its evaluated behaviour (not from the raw coefficient
signs). The vertical axis (the sign of the loaded FZ) and the lateral axis (the sign of dFy/dSA) are identified
separately, each from several weighted indicators evaluated at several loads. It returns a `CoordinateIdentification`:

| Field | Meaning |
|-------|---------|
| `classification` | `'ISO'`, `'SAE'` or `'MIXED'` (the two axes disagree) |
| `vertical`, `lateral` | `AxisDecision(sign, basis, confidence, positive_weight, negative_weight)`; `sign` is `'POSITIVE'` (ISO side), `'NEGATIVE'` (SAE side) or `'UNDETERMINED'`; `basis` is `'DECIDED'`, `'INSUFFICIENT_EVIDENCE'` or `'CONFLICTING_EVIDENCE'` |
| `effective_vertical`, `effective_lateral` | the signs used for the classification (`'POSITIVE'` or `'NEGATIVE'`); an undetermined axis takes the other axis's sign |
| `readings` | tuple of `IndicatorReading(name, meaning, unit, axis, load_scale, load, value, normalised, status, vote, weight)`, one per indicator and probe load; `status` is `'VOTE'`, `'BELOW_THRESHOLD'`, `'NOT_FINITE'`, `'CLAMPED'` or `'MISSING_CHANNEL'`; `vote` is +1, -1 or 0 |
| `reference_input` | NumPy array, the reference operating point in input order |
| `warnings` | tuple of strings: disagreeing indicators, undetermined axes, and mixed conventions |

A model whose coefficients mix conventions (for example a positive FNOMIN with a negative dFy/dSA) is classified
`'MIXED'` and carries a warning containing "mix conventions".

`convert_coordinate_system(target)` returns a new `TireModel` in `'ISO'` or `'SAE'` (case-insensitive). The conversion
flips coefficient signs; the equations never change, and converting twice restores the original coefficients. Other
names, including `'ISO-W'`, raise `MagicFormulaError` with code `INVALID_ARGUMENT`.

`convert_inputs` and `convert_outputs` are a data utility, never used by evaluation: they convert rows (a 1-D array is
one row) between `'ISO'` and `'SAE'` (case-insensitive). Inputs: only FZ changes sign. Outputs: FY, MZ and MY change
sign.

`evaluate_batch` and `evaluate` emit a `MagicFormulaWarning` ("row R: ..." / "FZ < 0 in N row(s) of an ISO table ...")
when rows have FZ on the unloaded side of the model; those rows are lift-off with zero forces and moments. This
happens with `check=False` too. The v1 keywords `input_system` and `output_system` (and the `'auto'` detection) no
longer exist.

## Errors and warnings

- `MagicFormulaError` (derives from `Exception`) is raised for library errors: unreadable or invalid `.tir`,
  unsupported model, invalid input, non-finite output. Attributes: `code` (for example `IO`, `SYNTAX`, `MISSING_KEY`,
  `INVALID_INPUT`, `INVALID_ARGUMENT`, `NUMERICAL`), `message`, `file`, `line`, `key`, `row` (failing batch row, flat
  C-order index for `evaluate`, `None` otherwise). `str(error)` includes the location.
- `TypeError` for missing or unknown keywords in `evaluate`.
- NumPy's `ValueError` for shapes that do not broadcast.
- `MagicFormulaWarning` (a `UserWarning`) is emitted once per load warning (for example defaulted coefficients,
  `TYRESIDE = 'RIGHT'`) and kept on `TireModel.warnings`. `evaluate` and `evaluate_batch` also emit it for the
  load-side warning (FZ on the unloaded side).
- An unknown axis-system name for `target` / `source` raises `MagicFormulaError`
  with code `INVALID_ARGUMENT`.

Clamped inputs and speeds below VXLOW are reported through the flag outputs, not through warnings.

## Units and conventions

SI units throughout: N, m/s, N*m; angles (slip angle, inclination) in radians; slip ratio dimensionless. Axis
systems are described under "Coordinate systems" (in ISO, My is negative when rolling forward). Flag outputs are floats, 0.0 or 1.0. Use `np.deg2rad` for
degrees.

## Performance

- A float64 Fortran-ordered `(N, 5)` input is passed without a copy; other layouts or dtypes are converted once.
- Outputs are returned without a copy.
- The GIL is released during evaluation, and models are immutable, so Python threads can evaluate chunks in parallel.
  Do not mutate an input array from another thread during a call.

## Tests

```bash
.venv/bin/python -m pip install "./python[test]"
.venv/bin/python -m pytest python/tests
```

The pytest suite enforces a 100% line and branch coverage gate on the `magic_formula` package (pytest-cov, configured
in `pyproject.toml`) and runs the example notebook.

## Example notebook

`python/examples/magic_formula_demo.ipynb` walks through loading, sweeps, plots, the batch API, MF 5.2, clamp flags and
error handling. It is committed without outputs.

```bash
.venv/bin/python -m pip install "./python[notebook]"
.venv/bin/jupyter lab python/examples/magic_formula_demo.ipynb
```
