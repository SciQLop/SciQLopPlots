"""Integration tests for plot and panel export API."""
import pytest
import numpy as np
from pathlib import Path
from PySide6.QtGui import QImage

from SciQLopPlots import SciQLopPlot, SciQLopMultiPlotPanel


class TestPlotExportPdf:
    def test_save_pdf_creates_file(self, plot, tmp_path):
        path = str(tmp_path / "test.pdf")
        assert plot.save_pdf(path) is True
        assert Path(path).stat().st_size > 0

    def test_save_pdf_with_dimensions(self, plot, tmp_path):
        path = str(tmp_path / "test.pdf")
        assert plot.save_pdf(path, 800, 600) is True
        assert Path(path).stat().st_size > 0


class TestPlotExportRaster:
    def test_save_png_creates_file(self, plot, tmp_path):
        path = str(tmp_path / "test.png")
        assert plot.save_png(path) is True
        assert Path(path).stat().st_size > 0

    def test_save_png_roundtrip(self, plot, tmp_path):
        path = str(tmp_path / "test.png")
        plot.save_png(path)
        img = QImage(path)
        assert not img.isNull()

    def test_save_png_with_dimensions(self, plot, tmp_path):
        path = str(tmp_path / "test.png")
        plot.save_png(path, 640, 480)
        img = QImage(path)
        assert img.width() == 640
        assert img.height() == 480

    def test_save_png_with_scale(self, plot, tmp_path):
        path = str(tmp_path / "test.png")
        plot.save_png(path, scale=2.0)
        assert Path(path).stat().st_size > 0

    def test_save_jpg_creates_file(self, plot, tmp_path):
        path = str(tmp_path / "test.jpg")
        assert plot.save_jpg(path) is True
        assert Path(path).stat().st_size > 0

    def test_save_bmp_creates_file(self, plot, tmp_path):
        path = str(tmp_path / "test.bmp")
        assert plot.save_bmp(path) is True
        assert Path(path).stat().st_size > 0


class TestPlotSaveDispatch:
    def test_save_pdf(self, plot, tmp_path):
        path = str(tmp_path / "test.pdf")
        assert plot.save(path) is True
        assert Path(path).stat().st_size > 0

    def test_save_png(self, plot, tmp_path):
        path = str(tmp_path / "test.png")
        assert plot.save(path) is True

    def test_save_jpg(self, plot, tmp_path):
        path = str(tmp_path / "test.jpg")
        assert plot.save(path) is True

    def test_save_jpeg(self, plot, tmp_path):
        path = str(tmp_path / "test.jpeg")
        assert plot.save(path) is True

    def test_save_bmp(self, plot, tmp_path):
        path = str(tmp_path / "test.bmp")
        assert plot.save(path) is True

    def test_save_case_insensitive(self, plot, tmp_path):
        path = str(tmp_path / "test.PNG")
        assert plot.save(path) is True

    def test_save_unknown_extension(self, plot, tmp_path):
        path = str(tmp_path / "test.xyz")
        assert plot.save(path) is False


@pytest.fixture
def panel_with_plots(qtbot):
    """Panel with 3 plots containing data."""
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    for i in range(3):
        x = np.linspace(0, 10, 100).astype(np.float64)
        y = np.sin(x + i).astype(np.float64)
        panel.plot(x, y, labels=[f"signal_{i}"])
    return panel


class TestPanelExportPdf:
    def test_save_pdf_creates_file(self, panel_with_plots, tmp_path):
        path = str(tmp_path / "panel.pdf")
        assert panel_with_plots.save_pdf(path) is True
        assert Path(path).stat().st_size > 0

    def test_save_pdf_with_dimensions(self, panel_with_plots, tmp_path):
        path = str(tmp_path / "panel.pdf")
        assert panel_with_plots.save_pdf(path, 800, 1200) is True
        assert Path(path).stat().st_size > 0


class TestPanelExportRaster:
    def test_save_png_creates_file(self, panel_with_plots, tmp_path):
        path = str(tmp_path / "panel.png")
        assert panel_with_plots.save_png(path) is True
        assert Path(path).stat().st_size > 0

    def test_save_png_roundtrip(self, panel_with_plots, tmp_path):
        path = str(tmp_path / "panel.png")
        panel_with_plots.save_png(path)
        img = QImage(path)
        assert not img.isNull()

    def test_save_jpg_creates_file(self, panel_with_plots, tmp_path):
        path = str(tmp_path / "panel.jpg")
        assert panel_with_plots.save_jpg(path) is True
        assert Path(path).stat().st_size > 0

    def test_save_bmp_creates_file(self, panel_with_plots, tmp_path):
        path = str(tmp_path / "panel.bmp")
        assert panel_with_plots.save_bmp(path) is True
        assert Path(path).stat().st_size > 0


class TestPanelSaveDispatch:
    def test_save_pdf(self, panel_with_plots, tmp_path):
        path = str(tmp_path / "panel.pdf")
        assert panel_with_plots.save(path) is True

    def test_save_png(self, panel_with_plots, tmp_path):
        path = str(tmp_path / "panel.png")
        assert panel_with_plots.save(path) is True

    def test_save_unknown_extension(self, panel_with_plots, tmp_path):
        path = str(tmp_path / "panel.xyz")
        assert panel_with_plots.save(path) is False


class TestPanelScrolledContent:
    """Panel with enough plots to overflow scroll area still exports all plots."""

    @pytest.fixture
    def scrolled_panel(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        panel.resize(400, 300)
        qtbot.addWidget(panel)
        for i in range(10):
            x = np.linspace(0, 10, 100).astype(np.float64)
            y = np.sin(x + i).astype(np.float64)
            panel.plot(x, y, labels=[f"signal_{i}"])
        return panel

    def test_scrolled_png_captures_all(self, scrolled_panel, tmp_path):
        path = str(tmp_path / "scrolled.png")
        assert scrolled_panel.save_png(path) is True
        img = QImage(path)
        assert img.height() > 300

    def test_scrolled_pdf_creates_file(self, scrolled_panel, tmp_path):
        path = str(tmp_path / "scrolled.pdf")
        assert scrolled_panel.save_pdf(path) is True
        assert Path(path).stat().st_size > 0


def _render_stats(png_path, region=None):
    """Content stats for a saved image, optionally over a sub-region.

    Returns ``(opaque_fraction, unique_colors)`` sampled on a grid.

    A rendered widget fills its area with an opaque background and draws
    chrome/data on top, giving high opaque coverage AND many distinct colours.
    A dropped/blank region stays transparent (opaque ~ 0) or a flat fill (~1
    colour). Comparing to a fixed "background" pixel does NOT work here: a
    correct plot's background equals the corner pixel, so a difference metric
    would only see the sparse foreground and read as "blank".

    `region` is ``(x0, y0, x1, y1)`` as fractions of width/height.
    """
    img = QImage(str(png_path)).convertToFormat(QImage.Format_ARGB32)
    assert not img.isNull()
    w, h = img.width(), img.height()
    x0, y0, x1, y1 = region or (0.0, 0.0, 1.0, 1.0)
    xa, ya, xb, yb = int(x0 * w), int(y0 * h), int(x1 * w), int(y1 * h)
    step = max(1, min(w, h) // 200)
    opaque = total = 0
    colors = set()
    for yy in range(ya, yb, step):
        for xx in range(xa, xb, step):
            total += 1
            c = img.pixelColor(xx, yy)
            colors.add((c.red(), c.green(), c.blue(), c.alpha()))
            if c.alpha() > 8:
                opaque += 1
    return (opaque / total if total else 0.0), len(colors)


class TestNestedPanelExport:
    """Nested sub-panels must appear in PDF/PNG export (backlog 2026-06-14)."""

    @pytest.fixture
    def nested_only(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        panel.resize(800, 600)
        qtbot.addWidget(panel)
        sub = SciQLopMultiPlotPanel()
        panel.add_panel(sub)
        x = np.linspace(0, 10, 100).astype(np.float64)
        sub.plot(x, np.sin(x), labels=["nested"])
        panel.show()
        qtbot.waitExposed(panel)
        qtbot.wait(200)
        return panel

    @pytest.fixture
    def top_plus_nested(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        panel.resize(800, 600)
        qtbot.addWidget(panel)
        x = np.linspace(0, 10, 100).astype(np.float64)
        panel.plot(x, np.cos(x), labels=["top"])
        sub = SciQLopMultiPlotPanel()
        panel.add_panel(sub)
        sub.plot(x, np.sin(x), labels=["nested"])
        panel.show()
        qtbot.waitExposed(panel)
        qtbot.wait(200)
        return panel

    def test_nested_only_pdf_succeeds(self, nested_only, tmp_path):
        path = tmp_path / "nested.pdf"
        assert nested_only.save_pdf(str(path)) is True
        assert path.stat().st_size > 0

    def test_nested_only_png_has_content(self, nested_only, tmp_path):
        path = tmp_path / "nested.png"
        assert nested_only.save_png(str(path)) is True
        opaque, colors = _render_stats(path)
        assert opaque > 0.5  # nested plot painted its (opaque) background
        assert colors > 8  # axes / grid / line / legend present, not a flat fill

    def test_top_plus_nested_png_both_regions_rendered(self, top_plus_nested, tmp_path):
        path = tmp_path / "mixed.png"
        assert top_plus_nested.save_png(str(path)) is True
        # top half = top-level plot, bottom half = nested sub-panel.
        # The bug dropped the nested region (left transparent); assert BOTH
        # halves render an opaque background with real plot content.
        top_opaque, top_colors = _render_stats(path, region=(0.0, 0.0, 1.0, 0.5))
        bottom_opaque, bottom_colors = _render_stats(path, region=(0.0, 0.5, 1.0, 1.0))
        assert top_opaque > 0.5 and top_colors > 8
        assert bottom_opaque > 0.5 and bottom_colors > 8

    def test_top_plus_nested_pdf_has_nested(self, top_plus_nested, tmp_path):
        nested_path = tmp_path / "mixed.pdf"
        assert top_plus_nested.save_pdf(str(nested_path)) is True
        single = SciQLopMultiPlotPanel()
        single.resize(800, 600)
        x = np.linspace(0, 10, 100).astype(np.float64)
        single.plot(x, np.cos(x), labels=["top"])
        single_path = tmp_path / "single.pdf"
        assert single.save_pdf(str(single_path)) is True
        assert nested_path.stat().st_size > single_path.stat().st_size


class TestPlainWidgetExport:
    """A non-plot QWidget child renders via the QWidget::render fallback.

    This is the matplotlib-canvas path: a plain QWidget added via the public
    addWidget(QWidget*) exports for free once the walker is in place.
    """

    def test_label_region_rendered(self, qtbot, tmp_path):
        from PySide6.QtWidgets import QLabel
        from PySide6.QtCore import Qt
        panel = SciQLopMultiPlotPanel()
        panel.resize(400, 300)
        qtbot.addWidget(panel)
        label = QLabel("EXPORT_ME")
        label.setStyleSheet("background:#ff0000;")
        label.setAlignment(Qt.AlignCenter)
        panel.addWidget(label)
        panel.show()
        qtbot.waitExposed(panel)
        qtbot.wait(200)
        path = tmp_path / "label.png"
        assert panel.save_png(str(path)) is True
        opaque, colors = _render_stats(path)
        assert opaque > 0.5  # the red label filled its region
        assert colors > 8  # background + anti-aliased text, not a flat fill
