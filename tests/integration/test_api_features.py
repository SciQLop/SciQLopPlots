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
