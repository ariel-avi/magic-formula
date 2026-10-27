"""Checks for the Python examples page: every snippet runs, its printed output and figures are the real ones."""

from __future__ import annotations

import importlib.util
import os
import re
import sys
import textwrap
from pathlib import Path

import pytest

RUNNER_PATH = Path(__file__).resolve().parents[2] / 'python' / 'examples' / 'run_examples_page.py'


@pytest.fixture(scope='module')
def runner():
    """The runner module, loaded by file path (it is a script outside the package)."""
    spec = importlib.util.spec_from_file_location('run_examples_page', RUNNER_PATH)
    module = importlib.util.module_from_spec(spec)
    sys.modules['run_examples_page'] = module
    try:
        spec.loader.exec_module(module)
        yield module
    finally:
        del sys.modules['run_examples_page']


def _page_snippets(runner):
    return runner.parse_page(runner.PAGE.read_text(encoding='utf-8'))


def _page_images(runner) -> set[str]:
    return {name for snippet in _page_snippets(runner) for name in snippet.images}


def test_page_runs_and_shows_real_outputs(runner, tmp_path):
    """Every snippet runs, its output matches the page, and every linked figure is rendered."""
    assert runner.main(['--image-dir', str(tmp_path)]) == 0
    assert {p.name for p in tmp_path.glob('*.png')} == _page_images(runner)


def test_page_images_are_committed(runner):
    """The images the page links are exactly the committed files."""
    assert _page_images(runner) == {p.name for p in runner.IMAGE_DIR.glob('*.png')}


def test_readme_shows_only_page_images(runner):
    """The README only references figures that the page renders."""
    readme = (runner.REPO_ROOT / 'README.md').read_text(encoding='utf-8')
    shown = set(re.findall(r'docs/images/python/([a-z0-9-]+\.png)', readme))
    assert shown <= _page_images(runner)


def test_page_uses_only_public_api(runner):
    """The snippets never reach into private modules."""
    for snippet in _page_snippets(runner):
        assert '_core' not in snippet.code
        assert 'magic_formula._' not in snippet.code


def test_page_covers_the_api(runner):
    """The page exercises the whole public workflow."""
    code = '\n'.join(snippet.code for snippet in _page_snippets(runner))
    for needle in (
        'load_tir(',
        'parse_tir(',
        '.evaluate(',
        '.evaluate_batch(',
        '.characteristics(',
        '.characteristics_batch(',
        '.characteristic_channels',
        '.find_coordinate_system(',
        '.convert_coordinate_system(',
        '.convert_inputs(',
        '.convert_outputs(',
        'MagicFormulaError',
    ):
        assert needle in code


def test_parse_page_pairs_outputs_and_images(runner):
    """Outputs and images attach to the python block they follow; other fences are ignored."""
    page = textwrap.dedent(
        """\
        ![early](../images/python/early.png)

        ```python
        print('hi')
        ```

        ```text
        hi
        ```

        Prose.

        ![a](../images/python/one.png)

        ```python
        x = 1
        ```

        ```bash
        echo ignored
        ```
        """
    )
    snippets = runner.parse_page(page)
    assert len(snippets) == 2
    assert snippets[0].line == 3
    assert snippets[0].code == "print('hi')"
    assert snippets[0].expected_output == 'hi'
    assert snippets[0].images == ('one.png',)
    assert snippets[1].expected_output is None
    assert snippets[1].images == ()


def test_parse_page_rejects_an_orphan_output_block(runner):
    """A text block must directly follow a python block."""
    with pytest.raises(runner.PageError):
        runner.parse_page('Prose.\n\n```text\nhi\n```\n')
    separated = '```python\nprint(1)\n```\n\nProse.\n\n```text\n1\n```\n'
    with pytest.raises(runner.PageError):
        runner.parse_page(separated)
    doubled = '```python\nprint(1)\n```\n\n```text\n1\n```\n\n```text\n1\n```\n'
    with pytest.raises(runner.PageError):
        runner.parse_page(doubled)


def test_parse_page_rejects_an_unclosed_fence(runner):
    """An unclosed fence is reported with its line."""
    with pytest.raises(runner.PageError, match='3'):
        runner.parse_page('Intro.\n\n```python\nprint(1)\n')


def test_run_page_reports_an_output_mismatch(runner, tmp_path):
    """Differing output is reported; matching and silent snippets are not."""
    wrong = runner.run_page([runner.Snippet(1, "print('a')", 'b', ())], image_dir=tmp_path)
    assert wrong == [runner.Mismatch(line=1, expected='b', actual='a')]
    right = runner.run_page(
        [runner.Snippet(1, "print('a')", 'a\n', ()), runner.Snippet(3, 'y = 2', None, ())],
        image_dir=tmp_path,
    )
    assert right == []


def test_run_page_rejects_an_unshown_figure(runner, tmp_path):
    """A figure without an image link is an error naming both counts."""
    snippet = runner.Snippet(5, 'import matplotlib.pyplot as plt\nplt.figure()', None, ())
    with pytest.raises(runner.PageError, match=r'1 figure\(s\) open, 0 image link\(s\)'):
        runner.run_page([snippet], image_dir=tmp_path)


def test_run_page_reports_the_page_line_of_an_error(runner, tmp_path):
    """A raising snippet is reported with the page line; the working directory is restored."""
    before = os.getcwd()
    with pytest.raises(runner.PageError, match=':7:') as info:
        runner.run_page([runner.Snippet(7, 'raise ValueError("boom")', None, ())], image_dir=tmp_path)
    assert isinstance(info.value.__cause__, ValueError)
    assert os.getcwd() == before


def test_run_page_labels_errors_with_the_given_page(runner, tmp_path):
    """Errors and tracebacks name the page that was passed, not the default page."""
    other = tmp_path / 'other.md'
    with pytest.raises(runner.PageError, match=r'other\.md:4:') as info:
        runner.run_page(
            [runner.Snippet(4, 'raise ValueError("boom")', None, ())], image_dir=tmp_path, page=other
        )
    assert info.value.__cause__.__traceback__.tb_next.tb_frame.f_code.co_filename == str(other)


def test_main_fails_and_prints_the_actual_output(runner, tmp_path, capsys):
    """main returns 1 with the real output on a mismatch or an error, and 0 on a good page."""
    images = tmp_path / 'img'
    wrong = tmp_path / 'wrong.md'
    wrong.write_text("```python\nprint('real')\n```\n\n```text\ninvented\n```\n", encoding='utf-8')
    assert runner.main(['--page', str(wrong), '--image-dir', str(images)]) == 1
    captured = capsys.readouterr()
    assert 'output differs' in captured.out
    assert 'real' in captured.out

    broken = tmp_path / 'broken.md'
    broken.write_text('```python\nraise RuntimeError("bad")\n```\n', encoding='utf-8')
    assert runner.main(['--page', str(broken), '--image-dir', str(images)]) == 1
    assert 'bad' in capsys.readouterr().err

    good = tmp_path / 'good.md'
    good.write_text("```python\nprint('ok')\n```\n\n```text\nok\n```\n", encoding='utf-8')
    assert runner.main(['--page', str(good), '--image-dir', str(images)]) == 0
    assert 'snippets ran' in capsys.readouterr().out
