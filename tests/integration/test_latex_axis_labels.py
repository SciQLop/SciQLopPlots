"""LaTeX in axis labels.

Axis labels were painted with a single QPainter::drawText, so `$B_x$` appeared
literally, dollars and all. QCPAxis now takes a pluggable QCPLabelRenderer and
SciQLopPlots installs one backed by JKQTMathText, which typesets any `$...$`
span. Text without such a span still goes through drawText untouched -- that is
what keeps ordinary labels free, and what stops a lone `$` from turning prose
into an equation.

The assertions here read the rendered image by *horizontal ink bands*: rows of
the bottom of the plot that contain ink, grouped into contiguous runs. Typeset
markup is structural, so it shows up as structure -- a fraction splits into
numerator, rule and denominator bands, which no single line of text can do.
A fixed pixel strip would not work, because a taller label legitimately moves
the whole axis rect up.
"""
import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import SciQLopPlot
from conftest import process_events

BOTTOM = 60  # rows to inspect: the axis line, its tick labels, and the label


def _plot_with_label(qtbot, label):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    x = np.linspace(0, 10, 100).astype(np.float64)
    graph = plot.plot(x, np.sin(x), labels=["s"])
    qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
    process_events()
    plot.legend().set_visible(False)
    plot.x_axis().set_label(label)
    plot.rescale_axes()
    process_events()
    return plot


def _render(plot, tmp_path, name):
    path = tmp_path / f"{name}.png"
    assert plot.save_png(str(path), 600, 400) is True
    img = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    assert not img.isNull()
    return img


def _bands(img):
    """Contiguous runs of inked rows across the bottom of the plot."""
    inked = [any(img.pixelColor(x, y).lightness() < 140 for x in range(img.width()))
             for y in range(img.height() - BOTTOM, img.height())]
    return sum(1 for i, v in enumerate(inked) if v and (i == 0 or not inked[i - 1]))


def _last_band_width(img):
    """Ink width of the lowest band -- the axis label itself."""
    rows = [[x for x in range(img.width())
             if img.pixelColor(x, y).lightness() < 140]
            for y in range(img.height() - BOTTOM, img.height())]
    last = [xs for xs in rows if xs]
    if not last:
        return 0
    lowest, seen = [], False
    for xs in reversed(rows):
        if xs:
            lowest.extend(xs)
            seen = True
        elif seen:
            break
    return max(lowest) - min(lowest)


class TestLatexAxisLabels:
    def test_a_plain_label_still_paints(self, qtbot, tmp_path):
        labelled = _bands(_render(_plot_with_label(qtbot, "time [s]"), tmp_path, "p"))
        bare = _bands(_render(_plot_with_label(qtbot, ""), tmp_path, "e"))
        assert labelled > bare, "a plain label must still add a band of ink"

    def test_a_fraction_is_typeset_not_printed_literally(self, qtbot, tmp_path):
        r"""`$\frac{a}{b}$` must stack; `\frac{a}{b}` must stay one line."""
        typeset = _bands(_render(_plot_with_label(qtbot, r"$\frac{a}{b}$"),
                                 tmp_path, "typeset"))
        literal = _bands(_render(_plot_with_label(qtbot, r"\frac{a}{b}"),
                                 tmp_path, "literal"))
        assert typeset > literal, (
            f"a stacked fraction should occupy more bands than its markup "
            f"({typeset} vs {literal})")

    def test_a_subscript_is_narrower_than_its_markup(self, qtbot, tmp_path):
        """Typeset `B_x` drops the underscore and shrinks the x."""
        typeset = _last_band_width(
            _render(_plot_with_label(qtbot, r"$B_x$ [nT]"), tmp_path, "sub"))
        literal = _last_band_width(
            _render(_plot_with_label(qtbot, r"B_x [nT]"), tmp_path, "lit"))
        assert 0 < typeset < literal

    @pytest.mark.parametrize("label", ["Cost [$/kg]", "100 $"])
    def test_a_lone_dollar_is_currency_not_math(self, qtbot, tmp_path, label):
        """One `$` opens nothing; the label must set like any other text."""
        got = _bands(_render(_plot_with_label(qtbot, label), tmp_path, "cur"))
        plain = _bands(_render(_plot_with_label(qtbot, "Cost per kg"),
                               tmp_path, "plain"))
        assert got == plain
