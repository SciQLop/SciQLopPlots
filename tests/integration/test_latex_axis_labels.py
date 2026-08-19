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


# A descender or an underscore can sit a row or two clear of the rest of the
# glyphs, and whether it does is a font and DPI question -- it differs between
# this machine and CI. Rows separated by no more than this many blank rows are
# therefore one band; the gap up to the tick labels above is far wider.
_BAND_GAP = 3


def _last_band_width(img):
    """Ink width of the lowest band -- the axis label itself."""
    rows = [[x for x in range(img.width())
             if img.pixelColor(x, y).lightness() < 140]
            for y in range(img.height() - BOTTOM, img.height())]
    lowest, seen, gap = [], False, 0
    for xs in reversed(rows):
        if xs:
            lowest.extend(xs)
            seen, gap = True, 0
        elif seen:
            gap += 1
            if gap > _BAND_GAP:
                break
    return (max(lowest) - min(lowest)) if lowest else 0


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
        """Typeset, `B_{xyz}` loses its braces and underscore and shrinks the xyz.

        A multi-character subscript is used on purpose: with `B_x` the two
        renderings differ by only a few pixels, which is too thin a margin to
        mean anything across fonts and DPIs.
        """
        typeset = _last_band_width(
            _render(_plot_with_label(qtbot, r"$B_{xyz}$ [nT]"), tmp_path, "sub"))
        literal = _last_band_width(
            _render(_plot_with_label(qtbot, r"B_{xyz} [nT]"), tmp_path, "lit"))
        assert 0 < typeset < literal

    @pytest.mark.parametrize("label", ["Cost [$/kg]", "100 $"])
    def test_a_lone_dollar_is_currency_not_math(self, qtbot, tmp_path, label):
        """One `$` opens nothing; the label must set like any other text."""
        got = _bands(_render(_plot_with_label(qtbot, label), tmp_path, "cur"))
        plain = _bands(_render(_plot_with_label(qtbot, "Cost per kg"),
                               tmp_path, "plain"))
        assert got == plain


def _bands_in(img, y0, y1):
    """Contiguous runs of inked rows between y0 and y1."""
    inked = [any(img.pixelColor(x, y).lightness() < 140 for x in range(img.width()))
             for y in range(y0, y1)]
    return sum(1 for i, v in enumerate(inked) if v and (i == 0 or not inked[i - 1]))


def _ink_height(img, y0, y1):
    """Vertical extent of ink between y0 and y1."""
    rows = [y for y in range(y0, y1)
            if any(img.pixelColor(x, y).lightness() < 140 for x in range(img.width()))]
    return (max(rows) - min(rows)) if rows else 0


def _quiet_plot(qtbot):
    """A plot with its axes hidden, so only the element under test leaves ink.

    The axis rect's borders are vertical lines, which put ink in *every* row and
    would collapse any band count to one.
    """
    from SciQLopPlots import SciQLopPlotRange
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    plot.y_axis().set_range(SciQLopPlotRange(-1.0, 1.0))
    for axis in (plot.x_axis(), plot.y_axis()):
        axis.set_visible(False)
    process_events()
    return plot


class TestLatexElsewhere:
    """The same renderer serves every element that sets user text."""

    def test_a_legend_entry_grows_to_fit_a_fraction(self, qtbot, tmp_path):
        r"""A graph named `$\frac{a}{b}$` stacks, so its legend entry gets taller.

        This also pins the measurement half: the legend sizes itself from the
        text, so it can only grow if the size hint went through the renderer
        too, not just the painting.
        """
        def legend_height(name, tag):
            plot = _quiet_plot(qtbot)
            x = np.linspace(0, 10, 50).astype(np.float64)
            graph = plot.plot(x, np.full_like(x, -0.9), labels=[name])
            qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
            process_events()
            return _ink_height(_render(plot, tmp_path, tag), 0, 100)

        typeset = legend_height(r"$\frac{a}{b}$", "leg_tex")
        literal = legend_height(r"\frac{a}{b}", "leg_lit")
        assert typeset > literal, f"legend did not grow: {typeset} vs {literal}"

    def test_a_text_item_is_typeset(self, qtbot, tmp_path):
        r"""QCPItemText goes through the same seam, so items typeset too."""
        from PySide6.QtCore import QPointF
        from SciQLopPlots import SciQLopTextItem, Coordinates

        def item_bands(text, tag):
            plot = _quiet_plot(qtbot)
            item = SciQLopTextItem(plot, text, QPointF(5.0, 0.0), False,
                                   Coordinates.Data)
            process_events()
            bands = _bands_in(_render(plot, tmp_path, tag), 100, 300)
            del item
            return bands

        assert item_bands(r"$\frac{a}{b}$", "item_tex") > \
               item_bands(r"\frac{a}{b}", "item_lit")

    def test_a_plain_item_is_unaffected(self, qtbot, tmp_path):
        from PySide6.QtCore import QPointF
        from SciQLopPlots import SciQLopTextItem, Coordinates

        plot = _quiet_plot(qtbot)
        item = SciQLopTextItem(plot, "hello", QPointF(5.0, 0.0), False,
                               Coordinates.Data)
        process_events()
        assert _bands_in(_render(plot, tmp_path, "item_plain"), 100, 300) == 1
        del item


class TestMathSpanSpacing:
    """A space touching a `$` delimiter used to be swallowed.

    LaTeX renders it about a pixel wide where an ordinary text-mode space is
    six, so `time $t$ [s]` set as "timet [s]". The renderer now makes those
    spaces explicit before parsing.
    """

    def test_spaces_around_a_math_span_survive(self, qtbot, tmp_path):
        spaced = _last_band_width(
            _render(_plot_with_label(qtbot, r"aaa $b$ ccc"), tmp_path, "spaced"))
        tight = _last_band_width(
            _render(_plot_with_label(qtbot, r"aaa$b$ccc"), tmp_path, "tight"))
        assert spaced > tight + 4, (
            f"the two spaces barely widened the label: {spaced} vs {tight}")

    def test_spacing_matches_ordinary_text(self, qtbot, tmp_path):
        """The gap must be a word space, not a hairline.

        Compared against the same string set entirely as text, so this pins the
        actual width rather than just 'wider than nothing'.
        """
        math = _last_band_width(
            _render(_plot_with_label(qtbot, r"aaa $b$ ccc"), tmp_path, "m"))
        plain = _last_band_width(
            _render(_plot_with_label(qtbot, r"aaa b ccc"), tmp_path, "p"))
        assert abs(math - plain) < 0.35 * plain, (
            f"math-span spacing is off from plain text: {math} vs {plain}")
