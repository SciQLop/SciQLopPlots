"""A PDF export of a dark panel had white bands between its plots: the PNG export fills the
panel background first, the PDF export left the page white where no plot paints."""
import numpy as np
from PySide6.QtCore import QSize
from PySide6.QtGui import QImage, QPalette
from PySide6.QtPdf import QPdfDocument
from SciQLopPlots import SciQLopTheme
from conftest import process_events


def _dark_panel_with_two_plots(panel):
    theme = SciQLopTheme.dark()
    panel.set_theme(theme)
    panel.setPalette(QPalette(theme.background()))  # as SciQLop and the browser library do
    x = np.linspace(0, 10, 100)
    for i in range(2):
        panel.plot(x, np.sin(x + i), labels=[f"s{i}"])
    panel.resize(400, 400)
    panel.show()
    process_events()
    return panel.plots()


def _render_pdf(path, size):
    doc = QPdfDocument()
    doc.load(str(path))
    return doc.render(0, size)


def _gap_row(panel, first, second):
    bottom = first.mapTo(panel, first.rect().bottomLeft()).y()
    top = second.mapTo(panel, second.rect().topLeft()).y()
    assert top - bottom > 1, "no gap between the plots to look at"
    return (bottom + top) // 2


def test_pdf_gaps_between_plots_take_the_panel_background(panel, tmp_path):
    first, second = _dark_panel_with_two_plots(panel)
    row = _gap_row(panel, first, second)
    panel.save_pdf(str(tmp_path / "p.pdf"))
    panel.save_png(str(tmp_path / "p.png"))
    pdf = _render_pdf(tmp_path / "p.pdf", QSize(panel.width(), panel.height()))
    png = QImage(str(tmp_path / "p.png"))
    x = panel.width() // 2
    background = panel.palette().color(QPalette.Window)
    assert png.pixelColor(x, row) == background
    assert pdf.pixelColor(x, row) == background     # was transparent: a viewer shows white paper
