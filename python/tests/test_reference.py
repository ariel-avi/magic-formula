"""Comparison against the external reference data."""

from __future__ import annotations

import numpy as np

# Documented reference tolerance, relative to the channel scale.
MFEVAL_REL_TOL = 1e-10
# FNOMIN 4000 N for FX/FY; FNOMIN * R0 = 4000 * 0.3 N*m for MZ/MX/MY.
SCALE = np.array([4000.0, 4000.0, 1200.0, 1200.0, 1200.0])


def test_mf52_matches_mfeval_reference(mf52, data_dir):
    ref = np.loadtxt(data_dir / 'reference' / 'mf52_mfeval.csv', delimiter=',', skiprows=1)
    assert ref.shape == (190, 10)
    ours = mf52.evaluate_batch(ref[:, :5])[:, :5]
    expected = ref[:, 5:]
    err = np.abs(ours - expected)
    bound = MFEVAL_REL_TOL * (SCALE + np.maximum(np.abs(ours), np.abs(expected)))
    worst = np.unravel_index(np.argmax(err / bound), err.shape)
    assert np.all(err <= bound), f'worst row/column {worst}: ours={ours[worst]}, ref={expected[worst]}'
