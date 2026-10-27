"""Runs the Python examples page (docs/examples/python.md) and renders its figures.

Every ```python block runs in page order, in one shared namespace, from the repository root, with
matplotlib's non-interactive Agg backend. A ```text block that directly follows a python block (only blank
lines between) is that block's expected stdout; a python block without one must print nothing. The figures
a block leaves open are saved, in creation order, to the image links ![...](../images/python/<name>.png)
that follow the block before the next python block. Other fences (for example ```bash) are not run.

Usage, from the repository root:  python python/examples/run_examples_page.py [--page P] [--image-dir D]
"""

from __future__ import annotations

import argparse
import contextlib
import difflib
import io
import os
import re
import sys
import warnings
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

REPO_ROOT: Path = Path(__file__).resolve().parents[2]
PAGE: Path = REPO_ROOT / 'docs' / 'examples' / 'python.md'
IMAGE_DIR: Path = REPO_ROOT / 'docs' / 'images' / 'python'
IMAGE_LINK: re.Pattern[str] = re.compile(r'!\[[^\]]*\]\(\.\./images/python/([a-z0-9-]+\.png)\)')
FIGURE_DPI: int = 110

_FENCE = '```'


@dataclass(frozen=True)
class Snippet:
    """One python block of the page."""

    line: int
    code: str
    expected_output: str | None
    images: tuple[str, ...]


@dataclass(frozen=True)
class Mismatch:
    """A snippet whose printed output differs from its text block (both normalised)."""

    line: int
    expected: str
    actual: str


class PageError(Exception):
    """A snippet raised, a fence is malformed, or a block's figures do not match its image links."""


def parse_page(markdown: str) -> list[Snippet]:
    """Splits the page into its python blocks, with expected outputs and image links."""
    lines = markdown.splitlines()
    codes: list[tuple[int, str]] = []
    outputs: dict[int, str] = {}
    images: dict[int, list[str]] = {}
    current: int | None = None  # index of the python block that owns the following images
    last_closed: tuple[int, int] | None = None  # (python block index, line index of its closing fence)
    index = 0
    while index < len(lines):
        text = lines[index]
        if not text.startswith(_FENCE):
            if current is not None:
                images[current].extend(m.group(1) for m in IMAGE_LINK.finditer(text))
            index += 1
            continue
        info = text[len(_FENCE):].strip()
        start = index
        index += 1
        body: list[str] = []
        while index < len(lines) and not lines[index].startswith(_FENCE):
            body.append(lines[index])
            index += 1
        if index >= len(lines):
            raise PageError(f'{start + 1}: unclosed fence')
        closing = index
        index += 1
        if info == 'python':
            codes.append((start + 1, '\n'.join(body)))
            current = len(codes) - 1
            images[current] = []
            last_closed = (current, closing)
        elif info == 'text':
            attached = (
                last_closed is not None
                and last_closed[0] not in outputs
                and all(not between.strip() for between in lines[last_closed[1] + 1:start])
            )
            if not attached or last_closed is None:
                raise PageError(f'{start + 1}: text block does not directly follow a python block')
            outputs[last_closed[0]] = '\n'.join(body)
            last_closed = None
    return [
        Snippet(line, code, outputs.get(i), tuple(images[i]))
        for i, (line, code) in enumerate(codes)
    ]


def _normalise(text: str | None) -> str:
    return '\n'.join(row.rstrip() for row in (text or '').strip('\n').splitlines())


def run_page(snippets: Sequence[Snippet], *, image_dir: Path, page: Path = PAGE) -> list[Mismatch]:
    """Runs the snippets, saves their figures and returns the output mismatches."""
    import matplotlib

    matplotlib.use('Agg', force=True)
    import matplotlib.pyplot as plt

    image_dir.mkdir(parents=True, exist_ok=True)
    namespace: dict[str, object] = {'__name__': '__examples_page__'}
    mismatches: list[Mismatch] = []
    old_cwd = os.getcwd()
    os.chdir(REPO_ROOT)
    try:
        for snippet in snippets:
            plt.close('all')
            out = io.StringIO()
            try:
                with contextlib.redirect_stdout(out), warnings.catch_warnings():
                    warnings.filterwarnings('ignore', message='FigureCanvasAgg is non-interactive')
                    exec(compile('\n' * snippet.line + snippet.code, str(page), 'exec'), namespace)
            except Exception as exc:
                raise PageError(f'{page.name}:{snippet.line}: {exc!r}') from exc
            expected = _normalise(snippet.expected_output)
            actual = _normalise(out.getvalue())
            if expected != actual:
                mismatches.append(Mismatch(snippet.line, expected, actual))
            numbers = plt.get_fignums()
            if len(numbers) != len(snippet.images):
                raise PageError(
                    f'{page.name}:{snippet.line}: {len(numbers)} figure(s) open, '
                    f'{len(snippet.images)} image link(s)'
                )
            for number, name in zip(numbers, snippet.images):
                plt.figure(number).savefig(
                    image_dir / name, dpi=FIGURE_DPI, bbox_inches='tight', metadata={'Software': None}
                )
    finally:
        plt.close('all')
        os.chdir(old_cwd)
    return mismatches


def main(argv: Sequence[str] | None = None) -> int:
    """Runs the page; returns 0 when every output matches, 1 otherwise."""
    parser = argparse.ArgumentParser(description='Run the Python examples page and render its figures.')
    parser.add_argument('--page', type=Path, default=PAGE)
    parser.add_argument('--image-dir', type=Path, default=IMAGE_DIR)
    args = parser.parse_args(argv)
    try:
        snippets = parse_page(args.page.read_text(encoding='utf-8'))
        mismatches = run_page(snippets, image_dir=args.image_dir, page=args.page)
    except PageError as error:
        print(error, file=sys.stderr)
        return 1
    for mismatch in mismatches:
        print(f'{args.page}:{mismatch.line}: output differs')
        for row in difflib.unified_diff(
            mismatch.expected.splitlines(), mismatch.actual.splitlines(), 'expected', 'actual', lineterm=''
        ):
            print(row)
        print('actual output:')
        print(mismatch.actual)
    if mismatches:
        return 1
    figures = sum(len(s.images) for s in snippets)
    print(f'{len(snippets)} snippets ran, {figures} figures written to {args.image_dir}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
