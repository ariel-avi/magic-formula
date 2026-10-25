"""Checks for the example notebook: structure, public-API-only use, and clean execution."""

from pathlib import Path

import nbformat
from nbclient import NotebookClient

EXAMPLES_DIR = Path(__file__).resolve().parents[1] / 'examples'
NOTEBOOK = EXAMPLES_DIR / 'magic_formula_demo.ipynb'
EXECUTION_TIMEOUT_S = 600


def _read() -> nbformat.NotebookNode:
    return nbformat.read(NOTEBOOK, as_version=4)


def test_notebook_is_valid_and_has_no_outputs():
    nb = _read()
    nbformat.validate(nb)
    assert nb.nbformat == 4
    assert nb.nbformat_minor >= 5
    assert nb.metadata.kernelspec.name == 'python3'
    for cell in nb.cells:
        assert cell.get('id')
        if cell.cell_type == 'code':
            assert cell.outputs == []
            assert cell.execution_count is None


def test_notebook_uses_only_public_api():
    for cell in _read().cells:
        assert '_core' not in cell.source
        assert 'magic_formula._' not in cell.source


def test_notebook_executes_without_errors():
    nb = _read()
    NotebookClient(
        nb,
        timeout=EXECUTION_TIMEOUT_S,
        kernel_name='python3',
        resources={'metadata': {'path': str(EXAMPLES_DIR)}},
    ).execute()
    for cell in nb.cells:
        if cell.cell_type == 'code':
            assert all(o.output_type != 'error' for o in cell.outputs)
