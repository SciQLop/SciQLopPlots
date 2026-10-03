"""Issue #123: editing intervals reports through signals."""
import threading

import numpy as np
import pytest
from PySide6.QtCore import QPoint, Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QWidget

from SciQLopPlots import SciQLopPlotRange
from conftest import process_events


def _canvas(plot):
    return next(w for w in plot.findChildren(QWidget) if w.inherits("QCustomPlot"))


@pytest.fixture
def editable(ts_plot, qtbot):
    ts_plot.resize(400, 300)
    ts_plot.show()
    qtbot.waitExposed(ts_plot)
    tl = ts_plot.add_timeline()
    tl.set_intervals([10, 60], [40, 90], lane=["A", "B"], ids=[7, 8])
    ts_plot.x_axis().set_range(SciQLopPlotRange(0, 100))
    tl.editable = True
    process_events()
    return ts_plot, tl


def _at(tl, key, lane):
    p = tl.pixel_of(key, lane)
    return QPoint(round(p.x()), round(p.y()))


def _drag(canvas, a, b):
    QTest.mousePress(canvas, Qt.LeftButton, Qt.NoModifier, a)
    QTest.mouseMove(canvas, b)
    QTest.mouseRelease(canvas, Qt.LeftButton, Qt.NoModifier, b)


def test_move_reports_id_times_and_lane_name(editable):
    plot, tl = editable
    tl.edit_modes = {"move", "change_lane"}
    seen = []
    tl.intervals_changed.connect(seen.append)
    _drag(_canvas(plot), _at(tl, 25, "A"), _at(tl, 35, "B"))
    assert len(seen) == 1
    (id_, start, stop, lane), = seen[0]
    assert id_ == 7 and lane == "B"
    assert start == pytest.approx(20, abs=0.6) and stop == pytest.approx(50, abs=0.6)


def test_create_reports_lane_name(editable):
    plot, tl = editable
    tl.edit_modes = {"create"}
    seen = []
    tl.interval_created.connect(lambda *a: seen.append(a))
    _drag(_canvas(plot), _at(tl, 45, "A"), _at(tl, 55, "A"))
    assert seen and seen[0][2] == "A"


def test_delete_key_requests_ids(editable):
    plot, tl = editable
    tl.edit_modes = {"delete"}
    tl.select_ids([8])
    seen = []
    tl.delete_requested.connect(seen.append)
    QTest.keyClick(_canvas(plot), Qt.Key_Delete)
    assert seen == [[8]]


def test_not_editable_reports_nothing(editable):
    plot, tl = editable
    tl.editable = False
    seen = []
    tl.intervals_changed.connect(seen.append)
    _drag(_canvas(plot), _at(tl, 25, "A"), _at(tl, 35, "A"))
    assert seen == []


def test_edit_modes_round_trip_and_reject_unknown(editable):
    _, tl = editable
    tl.edit_modes = {"move", "resize", "change_lane", "create", "delete"}
    assert tl.edit_modes == {"move", "resize", "change_lane", "create", "delete"}
    with pytest.raises(ValueError, match="teleport"):
        tl.edit_modes = {"teleport"}


def test_snap_to_round_trips(editable):
    _, tl = editable
    assert tl.snap_to is None
    tl.snap_to = "edges"
    assert tl.snap_to == "edges"
    tl.snap_to = 60
    assert tl.snap_to == 60
    tl.snap_to = None
    assert tl.snap_to is None
    with pytest.raises(ValueError):
        tl.snap_to = "sometimes"


def test_snap_to_external_times(editable):
    """#125: snap to orbit events (periapsis, boundary crossings, ...)."""
    _, tl = editable
    tl.snap_to = [62.0, 5.0]
    assert tl.snap_to == [5.0, 62.0]
    tl.snap_to = np.array(["1970-01-01T00:01:00", "1970-01-01T00:00:30"], dtype="datetime64[ns]")
    assert tl.snap_to == [30.0, 60.0]
    with pytest.raises(ValueError):
        tl.snap_to = []


def test_cursor_shows_resize_over_an_edge(editable, qtbot):
    plot, tl = editable
    canvas = _canvas(plot)
    edge = _at(tl, 40, "A") - QPoint(2, 0)
    QTest.mouseMove(canvas, edge)
    qtbot.wait(40)  # hover updates are throttled to 60 Hz
    QTest.mouseMove(canvas, edge + QPoint(0, 1))
    qtbot.waitUntil(lambda: canvas.cursor().shape() == Qt.SizeHorCursor, timeout=2000)


def test_set_editable_from_a_worker_is_queued(editable, qtbot):
    _, tl = editable
    t = threading.Thread(target=lambda: tl.set_editable(False))
    t.start()
    t.join()
    qtbot.waitUntil(lambda: not tl.editable, timeout=2000)
