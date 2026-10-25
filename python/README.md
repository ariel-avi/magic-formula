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

`check=True` validates inputs and outputs. `check=False` is the unchecked fast path: values are computed as is (NaN in
gives NaN out). A wrong input shape is always an error, whatever `check` is.

## Errors and warnings

- `MagicFormulaError` (derives from `Exception`) is raised for library errors: unreadable or invalid `.tir`,
  unsupported model, invalid input, non-finite output. Attributes: `code` (for example `IO`, `SYNTAX`, `MISSING_KEY`,
  `INVALID_INPUT`, `INVALID_ARGUMENT`, `NUMERICAL`), `message`, `file`, `line`, `key`, `row` (failing batch row, flat
  C-order index for `evaluate`, `None` otherwise). `str(error)` includes the location.
- `TypeError` for missing or unknown keywords in `evaluate`.
- NumPy's `ValueError` for shapes that do not broadcast.
- `MagicFormulaWarning` (a `UserWarning`) is emitted once per load warning (for example defaulted coefficients,
  `TYRESIDE = 'RIGHT'`) and kept on `TireModel.warnings`.

Clamped inputs and speeds below VXLOW are reported through the flag outputs, not through warnings.

## Units and conventions

SI units throughout: N, m/s, N*m; angles (slip angle, inclination) in radians; slip ratio dimensionless. Sign
convention is ISO-W; My is negative when rolling forward. Flag outputs are floats, 0.0 or 1.0. Use `np.deg2rad` for
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
