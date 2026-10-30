"""A setter called off the GUI thread raises in the caller for mistakes visible in its arguments.

Queued, it can only warn on the GUI thread: the caller's mistake would vanish into a log line.
Only checks that read nothing but the call's own arguments run before the hop. A check that
reads the object's state (a column count, whether another component is a sibling, log bins)
runs on the GUI thread, after the hop, and warns there: read from the caller's thread it
would race with the GUI thread.
"""
import threading

import numpy as np
import pytest
from PySide6.QtCore import qInstallMessageHandler
from PySide6.QtGui import QColor

from SciQLopPlots import GraphLineStyle, GraphMarkerShape, SciQLopPlot

X = np.linspace(0.0, 10.0, 50)
THREE = np.column_stack([np.sin(X), np.sin(X) - 1, np.sin(X) + 1])


def _raised(call):
    try:
        call()
    except Exception as error:  # noqa: BLE001 - the test compares it
        return error
    return None


def _raised_off_thread(call):
    out = []
    worker = threading.Thread(target=lambda: out.append(_raised(call)))
    worker.start()
    worker.join(timeout=10)
    assert not worker.is_alive(), "the off-thread call blocked"
    return out[0]


@pytest.fixture
def qt_messages():
    messages = []
    previous = qInstallMessageHandler(lambda mode, ctx, msg: messages.append(msg))
    yield messages
    qInstallMessageHandler(previous)


_PLOTS = []


@pytest.fixture(autouse=True)
def _release_plots():
    yield
    _PLOTS.clear()


def _plot(qtbot, cls=SciQLopPlot):
    plot = cls()
    qtbot.addWidget(plot)
    _PLOTS.append(plot)
    return plot


def _graph(qtbot):
    return _plot(qtbot).plot(X, THREE, labels=["a", "lo", "hi"])


def _components(qtbot):
    return _graph(qtbot).components()


def _two_graphs_components(qtbot):
    return _components(qtbot), _components(qtbot)


CASES = {
    "column fill against itself": (_graph, lambda g: g.set_fill_between(0, 0)),
    "column fill negative": (_graph, lambda g: g.set_fill_between(-1, 1)),
    "column fill to value negative": (_graph, lambda g: g.set_fill_to_value(-1, 0.0)),
    "column fill colour negative": (_graph, lambda g: g.set_fill_color(-1, QColor("red"))),
    "column error bars same bound twice": (_graph, lambda g: g.set_error_bars(0, 1, 1)),
    "column error bars on its own upper bound": (_graph, lambda g: g.set_error_bars(0, 1, 0)),
    "column error bars on its own lower bound": (_graph, lambda g: g.set_error_bars(0, 0, 2)),
    "column error bars negative cap": (_graph, lambda g: g.set_error_bars(0, 1, 2, -1.0)),
    "column clear fill negative": (_graph, lambda g: g.clear_fill(-1)),
    "column clear error bars negative": (_graph, lambda g: g.clear_error_bars(-1)),
    "column line style negative": (_graph, lambda g: g.set_column_line_style(-1, GraphLineStyle.NoLine)),
    "column marker shape negative": (
        _graph, lambda g: g.set_column_marker_shape(-1, GraphMarkerShape.Circle)),
    "column in legend negative": (_graph, lambda g: g.set_column_in_legend(-1, False)),
    "component fill against itself": (_components, lambda cs: cs[0].set_fill_between(cs[0])),
    "component error bars same bound twice": (
        _components, lambda cs: cs[0].set_error_bars(cs[1], cs[1])),
    "component error bars on itself": (_components, lambda cs: cs[0].set_error_bars(cs[0], cs[2])),
    "histogram bins": (lambda qtbot: _plot(qtbot).histogram(np.arange(10.0)),
                       lambda h: h.set_bins(0)),  # guard: already validated before the hop
}


@pytest.mark.parametrize("name", sorted(CASES))
def test_a_bad_call_from_a_worker_raises_there_as_on_the_gui_thread(qtbot, name):
    build, call = CASES[name]
    target = build(qtbot)
    on_gui = _raised(lambda: call(target))
    assert on_gui is not None, "pick a call the GUI thread rejects"
    off = _raised_off_thread(lambda: call(target))
    assert (type(off), str(off)) == (type(on_gui), str(on_gui))


STATE_CASES = {
    "labels count": (_graph, lambda g: g.set_labels(["only one"]), "Invalid number of labels"),
    "colors count": (_graph, lambda g: g.set_colors([QColor("red")]),
                     "Invalid number of colors"),
    "column past the last": (_graph, lambda g: g.set_fill_between(0, 9), "invalid columns"),
    "component of another graph": (
        _two_graphs_components, lambda pair: pair[0][0].set_fill_between(pair[1][1]),
        "same graph"),
}


@pytest.mark.parametrize("name", sorted(STATE_CASES))
def test_a_check_on_object_state_from_a_worker_warns_on_the_gui_thread(
        qtbot, qt_messages, name):
    build, call, expected = STATE_CASES[name]
    target = build(qtbot)
    assert _raised_off_thread(lambda: call(target)) is None
    qtbot.wait(50)
    assert any(expected in m for m in qt_messages), qt_messages


def test_a_log_bins_range_error_from_a_worker_warns_on_the_gui_thread(qtbot, qt_messages):
    histogram = _plot(qtbot).histogram(np.arange(1.0, 100.0), log_bins=True)
    assert _raised_off_thread(lambda: histogram.set_bin_range(0.0, 10.0)) is None
    qtbot.wait(50)
    assert any("log bins need a positive lower bound" in m for m in qt_messages), qt_messages
