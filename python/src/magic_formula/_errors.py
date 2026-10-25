"""Exception and warning types of the magic_formula package."""

from __future__ import annotations


def _rebuild_error(
    text: str, code: str, message: str, file: str, line: int, key: str, row: int | None
) -> MagicFormulaError:
    """Recreate a ``MagicFormulaError`` (used by pickle)."""
    return MagicFormulaError(text, code=code, message=message, file=file, line=line, key=key, row=row)


class MagicFormulaError(Exception):
    """A recoverable magic-formula error.

    Raised for an unreadable or invalid ``.tir`` file, an unsupported model,
    invalid input values and similar conditions. ``str(exc)`` is the library text.

    Attributes:
        code: error code name, e.g. ``'MISSING_KEY'``, ``'INVALID_INPUT'``,
            ``'INVALID_ARGUMENT'``.
        message: the description without the location prefix.
        file: file name the error refers to, ``''`` when not applicable.
        line: 1-based line number, ``0`` when not applicable.
        key: the ``.tir`` key or the channel name involved, ``''`` when not applicable.
        row: failing batch row (flat C-order index for ``TireModel.evaluate``),
            ``None`` when not applicable.
    """

    code: str
    message: str
    file: str
    line: int
    key: str
    row: int | None

    def __init__(
        self,
        text: str,
        *,
        code: str,
        message: str,
        file: str = '',
        line: int = 0,
        key: str = '',
        row: int | None = None,
    ) -> None:
        """Create the error from the library text and its structured fields."""
        super().__init__(text)
        self.code = code
        self.message = message
        self.file = file
        self.line = line
        self.key = key
        self.row = row

    def __reduce__(self) -> tuple:
        """Support pickling despite the keyword-only constructor."""
        return (
            _rebuild_error,
            (str(self), self.code, self.message, self.file, self.line, self.key, self.row),
        )


class MagicFormulaWarning(UserWarning):
    """A non-fatal load diagnostic (also kept in ``TireModel.warnings``)."""
