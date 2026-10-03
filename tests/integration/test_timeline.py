"""Issue #123: interval timeline."""
import threading

import numpy as np
import pytest
from PySide6.QtGui import QColor

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


def test_new_lanes_notify_once_per_set_intervals(ts_plot):
    tl = _strip(ts_plot)
    seen = []
    tl.lanes_changed.connect(lambda: seen.append(1))
    tl.set_intervals([0, 1, 2], [1, 2, 3], lane=["A", "B", "C"])
    assert len(seen) == 1


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
    qtbot.waitUntil(lambda: plot.minimumHeight() > 0, timeout=2000)
    h3 = plot.minimumHeight()
    tl.set_intervals([0] * 4, [1] * 4, lane=["MSA", "MPPE", "MGF", "MAG"])
    qtbot.waitUntil(lambda: plot.minimumHeight() == h3 + tl.lane_height(), timeout=2000)


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


# --- Issue #127: lane names must sit on their bars, at any plot height.

def _rgb(path):
    from PySide6.QtGui import QImage
    img = QImage(str(path)).convertToFormat(QImage.Format.Format_RGB32)
    raw = np.frombuffer(img.constBits(), np.uint8).reshape(img.height(), img.bytesPerLine() // 4, 4)
    return raw[:, :img.width(), 2::-1].astype(int)  # BGRA -> RGB


def _row_clusters(mask_rows):
    rows = np.flatnonzero(mask_rows)
    if rows.size == 0:
        return []
    groups = np.split(rows, np.flatnonzero(np.diff(rows) > 1) + 1)
    return [g.mean() for g in groups]


def _label_and_bar_centres(path):
    rgb = _rgb(path)
    saturated = (rgb.max(axis=2) - rgb.min(axis=2)) > 60
    ink = rgb.sum(axis=2) < 300
    bar_rows = saturated.any(axis=1)
    lanes_end = np.flatnonzero(bar_rows).max() + 2  # below the last bar: time axis labels
    labels = _row_clusters(ink[:lanes_end, :20].any(axis=1))  # lane names reach the left edge
    return labels, _row_clusters(bar_rows)


@pytest.fixture
def staircase(panel, qtbot):
    lanes = [f"lane_{i}" for i in range(6)]
    panel.resize(1000, 600)
    panel.show()
    plot, tl = panel.add_timeline(lane_height=18)
    start = np.arange(6) * 1.5e4
    tl.set_intervals(start, start + 1.2e4, lane=lanes, category=["bar"] * 6)
    # Category colours are process-wide; pin a saturated one so other tests can't change it.
    tl.set_category_colors({"bar": QColor(220, 40, 40)})
    plot.x_axis().set_range(SciQLopPlotRange(0, 1e5))
    qtbot.waitUntil(lambda: plot.minimumHeight() > 0, timeout=2000)
    qtbot.wait(100)
    return plot


@pytest.mark.parametrize("height", [None, 300], ids=["natural", "taller"])
def test_lane_names_sit_on_their_bars(staircase, tmp_path, height):
    path = tmp_path / "timeline.png"
    assert staircase.save_png(str(path), 1000, height or staircase.minimumHeight())
    labels, bars = _label_and_bar_centres(path)
    assert len(bars) == 6
    assert len(labels) == 6, f"lane names at rows {labels}, bars at {bars}"
    assert np.allclose(labels, bars, atol=3), f"lane names at rows {labels}, bars at {bars}"


# --- Issue #126: wave-view style.

def test_timeline_defaults_to_the_wave_style(ts_plot):
    assert _strip(ts_plot).style == "wave"


def test_timeline_style_round_trips(ts_plot):
    tl = _strip(ts_plot)
    assert isinstance(type(tl).style, property)  # else assigning only sets a Python attribute
    tl.style = "bars"
    assert tl.style == "bars"
    tl.style = "wave"
    assert tl.style == "wave"


def test_unknown_timeline_style_raises(ts_plot):
    with pytest.raises(ValueError, match="wave"):
        _strip(ts_plot).style = "gantt"


# --- Readable default lane height; lanes grow with a taller timeline plot.

def test_default_lane_height_is_readable(panel, ts_plot):
    _, tl = panel.add_timeline()
    assert tl.lane_height() == 22
    assert ts_plot.add_timeline().lane_height() == 22


def _splitter(panel):
    from PySide6.QtWidgets import QSplitter
    return panel.findChild(QSplitter)


def test_timeline_plot_has_a_natural_height_but_can_grow(staircase):
    natural = staircase.minimumHeight()
    assert staircase.sizeHint().height() == natural
    assert staircase.maximumHeight() > natural


def test_lanes_fill_a_taller_timeline_plot(staircase, panel, qtbot, tmp_path):
    natural = staircase.minimumHeight()
    panel.plot(np.linspace(0, 1e5, 100), np.zeros(100))
    splitter = _splitter(panel)
    total = sum(splitter.sizes())
    splitter.setSizes([natural + 120, total - natural - 120])
    qtbot.waitUntil(lambda: staircase.height() == natural + 120, timeout=2000)
    qtbot.wait(100)
    path = tmp_path / "taller.png"
    assert staircase.save_png(str(path), 1000, staircase.height())
    labels, bars = _label_and_bar_centres(path)
    assert len(bars) == 6
    assert np.diff(bars).mean() > 18 + 15, f"lanes did not grow: bars at {bars}"
    assert np.allclose(labels, bars, atol=3), f"lane names at rows {labels}, bars at {bars}"


def test_organize_plots_keeps_the_timeline_at_its_natural_height(staircase, panel, qtbot):
    panel.plot(np.linspace(0, 1e5, 100), np.zeros(100))
    panel.plot(np.linspace(0, 1e5, 100), np.zeros(100))
    _splitter(panel).setSizes([300, 100, 100])
    qtbot.wait(50)
    panel.organize_plots()
    # The natural height is read now: only the bottom plot keeps its time labels.
    qtbot.waitUntil(lambda: staircase.height() == staircase.minimumHeight(), timeout=2000)
    others = _splitter(panel).sizes()[1:]
    assert abs(others[0] - others[1]) <= 1


def test_a_timeline_added_among_plots_starts_at_its_natural_height(panel, qtbot):
    panel.resize(1000, 600)
    panel.show()
    panel.plot(np.linspace(0, 1e5, 100), np.zeros(100))
    panel.plot(np.linspace(0, 1e5, 100), np.zeros(100))
    plot, tl = panel.add_timeline(index=0)
    tl.set_intervals([0, 10], [5, 15], lane=["A", "B"])
    qtbot.waitUntil(lambda: plot.minimumHeight() > 0 and plot.height() == plot.minimumHeight(),
                    timeout=2000)


def test_timeline_above_a_shown_plot_gets_its_natural_height(panel, qtbot, qtlog):
    """SciQLop: a timeline inserted above a plot in a shown panel ended up 0 px tall.

    Measured right after insertion, the plot was 0 px tall but its axis rect still had
    its old height, so the "natural height" came out negative.
    """
    panel.resize(1000, 600)
    panel.show()
    panel.plot(np.linspace(0, 1e5, 100), np.zeros(100))
    qtbot.wait(100)
    plot, tl = panel.add_timeline(index=0)
    tl.set_intervals([0, 10], [5, 15], lane=["A", "B"])
    qtbot.waitUntil(lambda: plot.minimumHeight() > 0 and plot.height() == plot.minimumHeight(),
                    timeout=2000)
    assert not [r for r in qtlog.records if "Negative sizes" in r.message]


# --- #125: overlapping intervals.

def test_stack_and_forbid_round_trip_and_reject_unknown(ts_plot):
    tl = _strip(ts_plot)
    for name in ("stack", "forbid_overlap", "category_order"):
        assert isinstance(getattr(type(tl), name), property), name
    assert tl.stack is None and tl.forbid_overlap is False and tl.category_order == []
    for mode in ("time", "category", None):
        tl.stack = mode
        assert tl.stack == mode
    with pytest.raises(ValueError, match="category"):
        tl.stack = "merge"
    tl.forbid_overlap = True
    assert tl.forbid_overlap is True
    tl.category_order = ["LM", "BASE"]
    assert tl.category_order == ["LM", "BASE"]


def test_stacked_overlaps_make_their_lane_taller(panel, qtbot):
    plot, tl = panel.add_timeline()
    # MPPE runs three modes at once; MGF one.
    tl.set_intervals([0, 5, 8, 0], [10, 20, 30, 10], lane=["MPPE", "MPPE", "MPPE", "MGF"],
                     category=["BASE", "HKM", "LM", "BASE"])
    panel.show()
    qtbot.waitUntil(lambda: plot.minimumHeight() > 0, timeout=2000)
    flat = plot.minimumHeight()
    tl.stack = "time"
    qtbot.waitUntil(lambda: plot.minimumHeight() == flat + 2 * tl.lane_height(), timeout=2000)
    # Names sit in the middle of their lane's rows: MPPE spans rows 0..3, MGF row 3..4.
    assert plot.y_axis().tick_labels() == {1.5: "MPPE", 3.5: "MGF"}


def test_category_stack_gives_each_mode_a_row(panel, qtbot):
    """Tohban: one fixed sub-row per mode, in a chosen order, even when they never overlap."""
    plot, tl = panel.add_timeline()
    tl.set_intervals([0, 40, 80], [30, 70, 90], lane=["MPPE"] * 3, category=["LM", "BASE", "LM"])
    panel.show()
    qtbot.waitUntil(lambda: plot.minimumHeight() > 0, timeout=2000)
    flat = plot.minimumHeight()
    tl.category_order = ["BASE", "HKM", "LM"]   # HKM is not used: ignored
    tl.stack = "category"
    qtbot.waitUntil(lambda: plot.minimumHeight() == flat + tl.lane_height(), timeout=2000)


def test_interval_returns_what_a_hover_needs(ts_plot):
    """#125: hovered(id) only carries the id; interval(id) gives the rest."""
    tl = _strip(ts_plot)
    tl.set_intervals([0, 100], [60, 160], lane=["MPPE", "MGF"], category=["LM", "BASE"],
                     label=["low mass", "base"], ids=[7, 9])
    assert tl.interval(9) == {"id": 9, "start": 100.0, "stop": 160.0, "duration": 60.0,
                              "lane": "MGF", "category": "BASE", "label": "base"}
    assert tl.interval(42) is None


def test_closing_a_panel_with_a_stacked_timeline_does_not_crash(qtbot):
    """Tearing a timeline down changed the lane layout, which made the dying plot report a
    new natural height to its half-destroyed container (SIGSEGV in QSplitter::sizes)."""
    from SciQLopPlots import SciQLopMultiPlotPanel
    panel = SciQLopMultiPlotPanel()
    panel.resize(1000, 600)
    panel.show()
    panel.plot(np.linspace(0, 100, 10), np.zeros(10))
    plot, tl = panel.add_timeline(index=0)
    tl.set_intervals([0, 5], [10, 20], lane=["A", "A"])
    tl.overlap = "stack"
    qtbot.waitUntil(lambda: plot.minimumHeight() > 0, timeout=2000)
    panel.deleteLater()
    qtbot.wait(50)
