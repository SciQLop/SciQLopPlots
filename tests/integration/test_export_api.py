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
