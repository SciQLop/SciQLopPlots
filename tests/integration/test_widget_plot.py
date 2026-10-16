"""WidgetPlot puts any QWidget in a panel, as a row that follows the panel's time range."""
import gc

import numpy as np
import pytest
from PySide6.QtWidgets import QLabel

from SciQLopPlots import SciQLopPlotRange, SciQLopTheme, WidgetPlot
from conftest import process_events


def _abstract_calls(qtlog):
    return [r.message for r in qtlog.records if "Abstract method called" in r.message]


@pytest.fixture
def row(panel):
    calls = []
    row = WidgetPlot(QLabel("embedded"), on_time_range=lambda start, stop: calls.append((start, stop)))
    row.calls = calls
    panel.add_plot(row)
    panel.resize(600, 400)
    panel.show()
    process_events()
    return row


def test_a_widget_plot_is_a_row_of_the_panel(panel, row):
    assert panel.plot_count() == 1
    assert isinstance(row.widget, QLabel)
    assert row.widget.isVisible()


def test_the_panel_finds_every_method_it_calls(panel, row, qtlog):
    panel.plot(np.linspace(0, 10, 100), np.linspace(0, 10, 100))
    panel.set_time_axis_range(SciQLopPlotRange(1, 2))
    panel.set_theme(SciQLopTheme.dark())
    panel.save_png("/dev/null")
    process_events()
    assert _abstract_calls(qtlog) == []


def test_the_panel_time_range_reaches_it(panel, row):
    panel.set_time_axis_range(SciQLopPlotRange(10, 20))
    process_events()
    assert row.calls[-1] == (10, 20)
    assert row.time_axis().range().start() == 10


def test_panning_a_plot_of_the_panel_reaches_it(panel, row):
    plot, _ = panel.plot(np.linspace(0, 100, 100), np.linspace(0, 100, 100))
    process_events()
    plot.x_axis().set_range(30, 40)        # a default panel shares its x axis
    process_events()
    assert row.calls[-1] == (30, 40)


def test_a_time_synchronised_panel_reaches_it(qtbot):
    from SciQLopPlots import SciQLopMultiPlotPanel
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(panel)
    calls = []
    panel.add_plot(WidgetPlot(QLabel(), on_time_range=lambda *r: calls.append(r)))
    plot, _ = panel.plot(np.linspace(0, 100, 100), np.linspace(0, 100, 100))
    process_events()
    plot.time_axis().set_range(30, 40)
    process_events()
    assert calls[-1] == (30, 40)


def test_a_subclass_overrides_on_time_range(panel):
    class Row(WidgetPlot):
        def __init__(self):
            super().__init__(QLabel())
            self.seen = None

        def on_time_range(self, start, stop):
            self.seen = (start, stop)

    row = Row()
    panel.add_plot(row)
    panel.set_time_axis_range(SciQLopPlotRange(3, 4))
    process_events()
    assert row.seen == (3, 4)


def test_it_survives_python_dropping_its_references(panel):
    panel.add_plot(WidgetPlot(QLabel("orphan")))
    gc.collect()
    process_events()
    row = panel.plots()[0]
    panel.set_time_axis_range(SciQLopPlotRange(5, 6))
    process_events()
    assert row.time_axis().range().start() == 5


def test_a_dummy_axis_keeps_its_visibility(qtlog):
    from SciQLopPlots import SciQLopPlotDummyAxis
    axis = SciQLopPlotDummyAxis(None)
    axis.set_visible(False)
    assert not axis.visible()
    assert _abstract_calls(qtlog) == []


def test_a_widget_without_a_size_hint_still_gets_room(panel):
    from PySide6.QtWidgets import QWidget
    hintless = QWidget()                     # like a QQuickWidget or a window container
    assert hintless.sizeHint().height() <= 0
    panel.plot(np.linspace(0, 10, 100), np.linspace(0, 10, 100))
    row = WidgetPlot(hintless)
    panel.add_plot(row)
    panel.resize(600, 600)
    panel.show()
    process_events()
    assert row.height() > 150


def test_a_row_added_later_gets_the_current_range(qtbot):
    from SciQLopPlots import SciQLopMultiPlotPanel
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(panel)
    panel.set_time_axis_range(100, 200)
    calls = []
    panel.add_plot(WidgetPlot(QLabel(), on_time_range=lambda *r: calls.append(r)))
    process_events()
    assert calls == [(100, 200)]
