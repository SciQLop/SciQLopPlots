"""Issue #123: interval timeline."""
import threading

import numpy as np
import pytest

from SciQLopPlots import SciQLopPlotRange, SciQLopTimeSeriesPlot, SciQLopTimeline
from conftest import process_events


def _strip(ts_plot, **kwargs):
    tl = ts_plot.add_timeline(**kwargs)
    assert isinstance(tl, SciQLopTimeline)
    return tl


def test_set_intervals_counts_and_lanes_in_first_seen_order(ts_plot):
    tl = _strip(ts_plot)
    tl.set_intervals([0, 10, 20], [5, 15, 25], lane=["MSA", "MGF", "MSA"])
    process_events()
    assert tl.count() == 3
    assert tl.lanes == ["MSA", "MGF"]


def test_datetime64_times_are_epoch_seconds(ts_plot):
    tl = _strip(ts_plot)
    start = np.array(["2026-01-01T00:00:00"], dtype="datetime64[ns]")
    tl.set_intervals(start, start + np.timedelta64(60, "s"), lane=["A"])
    process_events()
    assert tl.count() == 1


@pytest.mark.parametrize(
    "kwargs, message",
    [
        (dict(start=[0, 1], stop=[1]), "same length"),
        (dict(start=[2], stop=[1]), "before start"),
        (dict(start=[np.nan], stop=[1]), "NaN"),
        (dict(start=[0], stop=[1], lane=["A", "B"]), "same length"),
    ],
)
def test_invalid_intervals_raise_value_error(ts_plot, kwargs, message):
    tl = _strip(ts_plot)
    with pytest.raises(ValueError, match=message):
        tl.set_intervals(**kwargs)


def test_two_timelines_on_one_plot_share_lanes_by_name(ts_plot):
    plan = _strip(ts_plot)
    catalog = _strip(ts_plot)
    plan.set_intervals([0], [1], lane=["MSA"])
    catalog.set_intervals([0], [1], lane=["events"])
    catalog.set_intervals([0], [1], lane=["MSA"])
    assert plan.lanes == ["MSA", "events"]
    assert catalog.lanes == plan.lanes


def test_category_colour_is_the_same_everywhere(qtbot):
    a, b = SciQLopTimeSeriesPlot(), SciQLopTimeSeriesPlot()
    qtbot.addWidget(a)
    qtbot.addWidget(b)
    ta, tb = a.add_timeline(), b.add_timeline()
    ta.set_intervals([0], [1], lane=["X"], category=["LM"])
    tb.set_intervals([0], [1], lane=["Y"], category=["LM"])
    assert ta.category_color("LM") == tb.category_color("LM")
    from PySide6.QtGui import QColor
    ta.set_category_colors({"LM": QColor("#f59e0b")})
    assert tb.category_color("LM") == QColor("#f59e0b")


def test_lanes_setter_reorders_hides_and_rename(ts_plot):
    tl = _strip(ts_plot)
    tl.set_intervals([0, 0], [1, 1], lane=["A", "B"])
    tl.set_lanes(["B"])
    assert tl.lanes == ["B"]
    assert tl.rename_lane("B", "Bee")
    assert tl.lanes == ["Bee"]


def test_assigning_lanes_hides_the_missing_ones(ts_plot):
    tl = _strip(ts_plot)
    tl.set_intervals([0, 0], [1, 1], lane=["A", "B"])
    tl.lanes = ["B"]
    assert "lanes" not in vars(tl)
    assert tl.lanes == ["B"]
    assert ts_plot.add_timeline().lanes == ["B"]


def test_add_timeline_off_the_gui_thread_raises(ts_plot):
    errors = []

    def worker():
        try:
            ts_plot.add_timeline()
        except RuntimeError as e:
            errors.append(e)

    t = threading.Thread(target=worker)
    t.start()
    t.join()
    assert errors


def test_set_intervals_from_a_worker_thread_is_queued(ts_plot, qtbot):
    tl = _strip(ts_plot)
    t = threading.Thread(target=lambda: tl.set_intervals([0], [1], lane=["A"]))
    t.start()
    t.join()
    qtbot.waitUntil(lambda: tl.count() == 1, timeout=2000)


def _panel_timeline(panel, lanes=("MSA", "MPPE", "MGF")):
    plot, tl = panel.add_timeline()
    tl.set_intervals([0] * len(lanes), [10] * len(lanes), lane=list(lanes))
    process_events()
    return plot, tl


def test_panel_add_timeline_returns_a_time_series_plot(panel):
    plot, tl = _panel_timeline(panel)
    assert isinstance(plot, SciQLopTimeSeriesPlot)
    assert isinstance(tl, SciQLopTimeline)


def test_second_timeline_keeps_the_panel_lane_height(panel):
    plot, tl = panel.add_timeline(lane_height=20)
    second = plot.add_timeline()
    assert second.lane_height() == 20
    assert tl.lane_height() == 20


def test_timeline_plot_shows_lane_names_on_its_y_axis(panel):
    plot, _ = _panel_timeline(panel)
    assert list(plot.y_axis().tick_labels().values()) == ["MSA", "MPPE", "MGF"]


def test_timeline_plot_height_is_lanes_times_lane_height(panel, qtbot):
    plot, tl = _panel_timeline(panel)
    panel.show()
    qtbot.waitUntil(lambda: plot.minimumHeight() == plot.maximumHeight(), timeout=2000)
    h3 = plot.height()
    tl.set_intervals([0] * 4, [1] * 4, lane=["MSA", "MPPE", "MGF", "MAG"])
    qtbot.waitUntil(lambda: plot.height() == h3 + 14, timeout=2000)


def test_timeline_plot_follows_the_panel_time_axis(panel):
    panel.plot(np.linspace(0, 100, 10), np.zeros(10))
    plot, _ = _panel_timeline(panel)
    panel.set_time_axis_range(SciQLopPlotRange(20, 30))
    process_events()
    r = plot.x_axis().range()
    assert (r.start(), r.stop()) == (20, 30)


def test_line_graph_on_a_timeline_plot_uses_the_right_axis(panel):
    plot, _ = _panel_timeline(panel)
    graph = plot.plot(np.linspace(0, 10, 10), np.linspace(0, 1, 10))
    process_events()
    assert graph.y_axis() == plot.y2_axis()
    assert plot.y2_axis().visible()
    assert list(plot.y_axis().tick_labels().values()) == ["MSA", "MPPE", "MGF"]


from PySide6.QtCore import QPoint, Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QWidget


def _canvas(plot):
    return next(w for w in plot.findChildren(QWidget) if w.inherits("QCustomPlot"))


def _two_bar_strip(ts_plot, qtbot):
    ts_plot.show()
    qtbot.waitExposed(ts_plot)
    tl = ts_plot.add_timeline()
    tl.set_intervals([10, 60], [40, 90], lane=["A", "B"], ids=[7, 8])
    ts_plot.x_axis().set_range(SciQLopPlotRange(0, 100))
    process_events()
    return tl


def _at(tl, key, lane):
    p = tl.pixel_of(key, lane)
    return QPoint(round(p.x()), round(p.y()))


def test_hover_reports_the_user_id(ts_plot, qtbot):
    tl = _two_bar_strip(ts_plot, qtbot)
    seen = []
    tl.hovered.connect(seen.append)
    QTest.mouseMove(_canvas(ts_plot), _at(tl, 25, "A"))
    qtbot.waitUntil(lambda: seen and seen[-1] == 7, timeout=2000)
    QTest.mouseMove(_canvas(ts_plot), _at(tl, 50, "A"))
    qtbot.waitUntil(lambda: seen[-1] == -1, timeout=2000)


def test_hidden_timeline_reports_no_hover(ts_plot, qtbot):
    tl = _two_bar_strip(ts_plot, qtbot)
    seen = []
    tl.hovered.connect(seen.append)
    QTest.mouseMove(_canvas(ts_plot), _at(tl, 25, "A"))
    qtbot.waitUntil(lambda: seen and seen[-1] == 7, timeout=2000)
    tl.set_visible(False)
    QTest.mouseMove(_canvas(ts_plot), _at(tl, 26, "A"))
    qtbot.waitUntil(lambda: seen[-1] == -1, timeout=2000)


def test_click_selection_reports_ids(ts_plot, qtbot):
    tl = _two_bar_strip(ts_plot, qtbot)
    seen = []
    tl.selected_intervals_changed.connect(seen.append)
    QTest.mouseClick(_canvas(ts_plot), Qt.LeftButton, Qt.NoModifier, _at(tl, 75, "B"))
    qtbot.waitUntil(lambda: seen and seen[-1] == [8], timeout=2000)
    assert tl.selected_ids() == [8]


def test_select_ids_round_trips(ts_plot, qtbot):
    tl = _two_bar_strip(ts_plot, qtbot)
    tl.select_ids([8, 7])
    assert sorted(tl.selected_ids()) == [7, 8]
    assert tl.selected()
    tl.set_selected(False)
    assert tl.selected_ids() == []
