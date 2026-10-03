"""MultiPlotsVerticalLine: one vertical line shared by every plot of a panel (#124)."""
from SciQLopPlots import MultiPlotsVerticalLine, SciQLopVerticalLine
from PySide6.QtGui import QColor

from conftest import force_gc


def _lines(panel):
    return [line for i in range(len(panel.plots()))
            for line in panel.plot_at(i).findChildren(SciQLopVerticalLine)]


def _two_plot_panel(panel, sample_data):
    x, y = sample_data
    panel.line(x, y)
    panel.line(x, y)
    return panel


class TestMultiPlotsVerticalLine:

    def test_one_line_per_plot_at_the_position(self, panel, sample_data):
        _two_plot_panel(panel, sample_data)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        assert vline.position == 3.0
        lines = _lines(panel)
        assert len(lines) == 2
        assert all(line.position == 3.0 for line in lines)

    def test_plot_added_later_gets_the_line(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        vline.position = 4.0
        panel.line(x, y)
        lines = _lines(panel)
        assert len(lines) == 2
        assert all(line.position == 4.0 for line in lines)

    def test_moving_one_line_moves_all_with_one_signal(self, panel, sample_data):
        _two_plot_panel(panel, sample_data)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        received = []
        vline.position_changed.connect(received.append)
        _lines(panel)[0].position = 6.0
        assert vline.position == 6.0
        assert all(line.position == 6.0 for line in _lines(panel))
        assert received == [6.0]

    def test_set_position_does_not_emit_when_unchanged(self, panel, sample_data):
        _two_plot_panel(panel, sample_data)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        received = []
        vline.position_changed.connect(received.append)
        vline.position = 3.0
        assert received == []

    def test_read_only_lines_are_not_movable(self, panel, sample_data):
        _two_plot_panel(panel, sample_data)
        vline = MultiPlotsVerticalLine(panel, 3.0, read_only=True)
        assert vline.read_only
        assert not any(line.movable() for line in _lines(panel))
        vline.set_read_only(False)
        assert all(line.movable() for line in _lines(panel))

    def test_setters_propagate_to_every_line(self, panel, sample_data):
        _two_plot_panel(panel, sample_data)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        vline.set_color(QColor(255, 0, 0))
        vline.set_line_width(3.0)
        vline.set_visible(False)
        vline.set_tool_tip("cursor")
        for line in _lines(panel):
            assert line.color().red() == 255
            assert line.line_width() == 3.0
            assert not line.visible()
            assert line.tool_tip() == "cursor"

    def test_new_plot_inherits_the_style(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        vline = MultiPlotsVerticalLine(panel, 3.0, QColor(0, 255, 0), read_only=True,
                                       visible=False, tool_tip="cursor")
        vline.set_line_width(2.0)
        panel.line(x, y)
        late = _lines(panel)[-1]
        assert late.color().green() == 255
        assert late.line_width() == 2.0
        assert not late.movable()
        assert not late.visible()
        assert late.tool_tip() == "cursor"

    def test_delete_removes_lines(self, panel, sample_data):
        _two_plot_panel(panel, sample_data)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        vline.deleteLater()
        del vline
        force_gc()
        from PySide6.QtCore import QCoreApplication, QEvent
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        assert _lines(panel) == []

    def test_plot_destruction_does_not_crash_setters(self, qtbot, panel, sample_data):
        _two_plot_panel(panel, sample_data)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        panel.plot_at(0).deleteLater()
        qtbot.wait(100)
        vline.position = 5.0
        vline.set_color(QColor(255, 0, 0))
        vline.set_line_width(2.0)
        vline.set_visible(False)
        vline.set_read_only(True)
        vline.set_tool_tip("x")
        assert all(line.position == 5.0 for line in _lines(panel))

    def test_python_properties_reach_every_line(self, panel, sample_data):
        _two_plot_panel(panel, sample_data)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        vline.color = QColor(255, 0, 0)
        vline.line_width = 3.0
        vline.visible = False
        vline.read_only = True
        vline.tooltip = "cursor"
        for line in _lines(panel):
            assert line.color().red() == 255
            assert line.line_width() == 3.0
            assert not line.visible()
            assert not line.movable()
            assert line.tool_tip() == "cursor"

    def test_moving_any_line_moves_all(self, panel, sample_data):
        _two_plot_panel(panel, sample_data)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        _lines(panel)[-1].position = 7.0
        assert vline.position == 7.0
        assert all(line.position == 7.0 for line in _lines(panel))

    def test_adding_a_plot_does_not_emit(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        received = []
        vline.position_changed.connect(received.append)
        panel.line(x, y)
        assert received == []

    def test_remove_plot_drops_only_its_line(self, qtbot, panel, sample_data):
        _two_plot_panel(panel, sample_data)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        kept = panel.plot_at(1)
        panel.remove_plot(panel.plot_at(0))
        qtbot.wait(100)
        assert len(kept.findChildren(SciQLopVerticalLine)) == 1
        vline.position = 5.0
        assert kept.findChildren(SciQLopVerticalLine)[0].position == 5.0

    def test_panel_destruction_with_live_line(self, qtbot, sample_data):
        from SciQLopPlots import SciQLopMultiPlotPanel
        panel = SciQLopMultiPlotPanel()
        _two_plot_panel(panel, sample_data)
        vline = MultiPlotsVerticalLine(panel, 3.0)
        destroyed = []
        vline.destroyed.connect(lambda: destroyed.append(True))
        panel.deleteLater()
        qtbot.waitUntil(lambda: destroyed == [True], timeout=1000)
