"""Phase 3 of the 2026-10-20 API review (docs/api-review-2026-10-20.md): missing features."""
import numpy as np
import pytest
from PySide6.QtCore import QPointF, Qt

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlot
from conftest import process_events

X = np.linspace(0, 10, 100)


def _laid_out(qtbot, plot):
    plot.resize(600, 400)
    plot.show()
    qtbot.waitExposed(plot)
    plot.replot(True)
    process_events()


@pytest.fixture
def three_lines(qtbot, plot):
    for k in range(3):
        plot.plot(X, np.sin(X) + k, labels=[f"line {k}"])
    _laid_out(qtbot, plot)
    return plot


class TestLegendPlacement:  # task 15, findings 2.1, 4.4
    def test_alignment_moves_the_legend_to_a_corner(self, qtbot, three_lines):
        legend = three_lines.legend()
        legend.set_alignment(Qt.AlignBottom | Qt.AlignLeft)
        _laid_out(qtbot, three_lines)
        assert legend.alignment() == Qt.AlignBottom | Qt.AlignLeft
        p = legend.position()
        assert p.x() < 0.05 and p.y() > 0.5

    def test_free_position_in_plot_area_fractions(self, qtbot, three_lines):
        legend = three_lines.legend()
        legend.set_position(QPointF(0.3, 0.4))
        _laid_out(qtbot, three_lines)
        p = legend.position()
        assert p.x() == pytest.approx(0.3, abs=0.01) and p.y() == pytest.approx(0.4, abs=0.01)

    def test_position_signal(self, qtbot, three_lines):
        seen = []
        three_lines.legend().position_changed.connect(seen.append)
        three_lines.legend().set_position(QPointF(0.2, 0.2))
        assert len(seen) == 1

    def test_columns_put_entries_side_by_side(self, qtbot, three_lines):
        legend = three_lines.legend()
        tall = [legend.row_rect(i) for i in range(3)]
        assert tall[0].x() == tall[1].x() == tall[2].x()
        legend.set_columns(3)
        _laid_out(qtbot, three_lines)
        assert legend.columns() == 3
        wide = [legend.row_rect(i) for i in range(3)]
        assert wide[0].y() == wide[1].y() == wide[2].y()
        assert wide[0].x() < wide[1].x() < wide[2].x()


class TestPanelLegend:  # task 15, finding 4.5
    def test_one_call_for_every_plot_now_and_later(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        first = panel.create_plot(-1)
        panel.set_legend_visible(False)
        later = panel.create_plot(-1)
        assert not panel.legend_visible()
        assert not first.legend().is_visible()
        assert not later.legend().is_visible()
        panel.set_legend_visible(True)
        assert first.legend().is_visible() and later.legend().is_visible()


def _top_band(plot, tmp_path, name):
    """The top 40 pixel rows of a 600x400 export, as bytes: where a title draws."""
    from PySide6.QtGui import QImage
    path = str(tmp_path / f"{name}.png")
    plot.save(path, 600, 400)
    img = QImage(path).convertToFormat(QImage.Format_ARGB32)
    return bytes(img.copy(0, 0, 600, 40).constBits())


class TestPlotTitle:  # task 16, finding 4.2
    def test_set_title(self, qtbot, plot):
        seen = []
        plot.title_changed.connect(seen.append)
        plot.set_title("Run 42")
        assert plot.title() == "Run 42"
        assert seen == ["Run 42"]

    def test_the_title_is_drawn_and_removed(self, qtbot, plot, tmp_path):
        plot.plot(X, np.sin(X))
        bare = _top_band(plot, tmp_path, "bare")
        plot.set_title("Run 42")
        assert _top_band(plot, tmp_path, "titled") != bare
        plot.set_title("")
        assert plot.title() == ""
        assert _top_band(plot, tmp_path, "removed") == bare

    @pytest.mark.parametrize("title_first", [True, False])
    def test_a_colour_scale_sits_beside_the_plot_area(self, qtbot, plot, tmp_path, title_first):
        z = np.random.rand(30, 20)
        if title_first:
            plot.set_title("spectrogram")
        plot.plot(np.arange(30.), np.arange(20.), z)
        if not title_first:
            plot.set_title("spectrogram")
        assert _gradient_beside_the_plot_area(plot, tmp_path, "cmap")
        plot.set_title("")  # the colour scale stays, the title row goes
        assert _gradient_beside_the_plot_area(plot, tmp_path, "cmap2")


def _gradient_beside_the_plot_area(plot, tmp_path, name):
    """Coloured pixels in the strip right of the plot area's labels (x 552-578 of 600), level
    with the plot area: the colour bar. Put in the title's row, it collapses to a stub at the top."""
    from PySide6.QtGui import QImage
    path = str(tmp_path / f"{name}.png")
    plot.save(path, 600, 400)
    img = QImage(path).convertToFormat(QImage.Format_ARGB32)
    return sum(1 for x in range(552, 578) for y in range(120, 330)
               if img.pixelColor(x, y).saturation() > 80) > 50


class TestSvgExport:  # task 18, finding 4.7
    @staticmethod
    def _targets(qtbot):
        from SciQLopPlots import SciQLopNDProjectionPlot
        plot = SciQLopPlot()
        plot.plot(X, np.sin(X))
        panel = SciQLopMultiPlotPanel()
        panel.plot(X, np.cos(X))
        proj = SciQLopNDProjectionPlot(3)
        for w in (plot, panel, proj):
            qtbot.addWidget(w)
        return {"plot": plot, "panel": panel, "projection": proj}

    @pytest.mark.parametrize("kind", ["plot", "panel", "projection"])
    def test_writes_an_svg(self, qtbot, tmp_path, kind):
        targets = self._targets(qtbot)
        path = tmp_path / f"{kind}.svg"
        assert targets[kind].save(str(path), 400, 300) is True
        text = path.read_text()
        assert "<svg" in text and "</svg>" in text
        if kind != "projection":  # an empty projection draws only its frames
            assert "<polyline" in text  # the graph, as vector lines

    LINE_COLORS = ["#ff0000", "#00ff00", "#0000ff"]

    @staticmethod
    def _scenes(qtbot):
        """kind -> (exported widget, plots hosting one coloured line each)."""
        from SciQLopPlots import SciQLopNDProjectionPlot
        plot = SciQLopPlot()
        panel = SciQLopMultiPlotPanel()
        for _ in TestSvgExport.LINE_COLORS:
            panel.plot(X, np.sin(X))
        proj = SciQLopNDProjectionPlot(3)
        for w in (plot, panel, proj):
            qtbot.addWidget(w)
        return {"plot": (plot, [plot]),
                "panel": (panel, [panel.plot_at(i) for i in range(3)]),
                "projection": (proj, [proj.subplot(0)])}

    @staticmethod
    def _rect_in(root, plot):
        top_left = plot.mapTo(root, plot.rect().topLeft())
        return top_left.x(), top_left.y(), top_left.x() + plot.width(), top_left.y() + plot.height()

    @staticmethod
    def _clip_boxes(svg_root, color):
        """Bounding boxes (x0, y0, x1, y1) of every clip path applied to polylines stroked `color`."""
        import re
        svg = "{http://www.w3.org/2000/svg}"
        parent = {child: node for node in svg_root.iter() for child in node}
        clips = {c.get("id"): [float(v) for v in re.findall(r"-?\d+(?:\.\d+)?", c.find(svg + "path").get("d"))]
                 for c in svg_root.iter(svg + "clipPath")}

        def ancestors(node):
            while node in parent:
                node = parent[node]
                yield node

        def stroke_of(polyline):
            return next((n.get("stroke") for n in ancestors(polyline) if n.get("stroke")), None)

        ids = {re.search(r"#([^)]+)", n.get("clip-path")).group(1)
               for line in svg_root.iter(svg + "polyline") if stroke_of(line) == color
               for n in ancestors(line) if n.get("clip-path")}
        return [(min(xy[0::2]), min(xy[1::2]), max(xy[0::2]), max(xy[1::2])) for xy in map(clips.get, ids)]

    @pytest.mark.parametrize("kind", ["plot", "panel", "projection"])
    def test_a_line_leaving_the_view_is_clipped_to_its_plot_area(self, qtbot, tmp_path, kind):
        # Read the SVG itself: QSvgRenderer implements SVG Tiny, which ignores clip paths.
        import xml.etree.ElementTree as ET
        from PySide6.QtGui import QColor
        root, plots = self._scenes(qtbot)[kind]
        for p, color in zip(plots, self.LINE_COLORS):
            p.plot(X, 3 * np.sin(X), colors=[QColor(color)])
        root.resize(600, 400)
        root.show()
        qtbot.waitExposed(root)
        for p in plots:
            p.x_axis().set_range(2, 8)
            p.y_axis().set_range(-1, 1)
            p.replot(True)
        process_events()
        path = tmp_path / "zoomed.svg"
        assert root.save(str(path), root.width(), root.height()) is True
        svg_root = ET.parse(path).getroot()
        assert svg_root.get("version") == "1.1" and svg_root.get("baseProfile") != "tiny"
        for p, color in zip(plots, self.LINE_COLORS):
            left, top, right, bottom = self._rect_in(root, p)
            boxes = self._clip_boxes(svg_root, color)
            assert boxes, f"no clip path on the {color} line"
            for x0, y0, x1, y1 in boxes:
                assert left < x0 and x1 < right, f"{color} clip leaks in x: {(x0, x1)} vs {(left, right)}"
                assert top - 1 <= y0 and y1 <= bottom + 1, f"{color} clip leaks in y: {(y0, y1)} vs {(top, bottom)}"

    def test_a_bad_path_raises(self, qtbot, tmp_path):
        targets = self._targets(qtbot)
        with pytest.raises(OSError, match="failed to save"):
            targets["plot"].save(str(tmp_path / "no_such_dir" / "x.svg"))

    def test_unknown_format_lists_svg(self, qtbot, tmp_path):
        targets = self._targets(qtbot)
        with pytest.raises(ValueError, match="svg"):
            targets["plot"].save(str(tmp_path / "x.tiff"))
