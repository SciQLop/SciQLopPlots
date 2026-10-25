import pytest
import gc
import threading
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QTimer


@pytest.fixture
def plot(qtbot):
    """Fresh SciQLopPlot for each test."""
    from SciQLopPlots import SciQLopPlot
    p = SciQLopPlot()
    qtbot.addWidget(p)
    return p


@pytest.fixture
def ts_plot(qtbot):
    """Fresh SciQLopTimeSeriesPlot for each test."""
    from SciQLopPlots import SciQLopTimeSeriesPlot
    p = SciQLopTimeSeriesPlot()
    qtbot.addWidget(p)
    return p


@pytest.fixture
def panel(qtbot):
    """Fresh SciQLopMultiPlotPanel for each test."""
    from SciQLopPlots import SciQLopMultiPlotPanel
    p = SciQLopMultiPlotPanel()
    qtbot.addWidget(p)
    return p


@pytest.fixture
def sample_data():
    """Returns (x, y) numpy arrays for testing."""
    x = np.linspace(0, 10, 100).astype(np.float64)
    y = np.sin(x).astype(np.float64)
    return x, y


@pytest.fixture
def sample_multicomponent_data():
    """Returns (x, y) where y has 3 columns."""
    x = np.linspace(0, 10, 100).astype(np.float64)
    y = np.column_stack([np.sin(x), np.cos(x), np.sin(2 * x)]).astype(np.float64)
    return x, y


@pytest.fixture
def sample_colormap_data():
    """Returns (x, y, z) numpy arrays for colormap testing."""
    x = np.linspace(0, 10, 50).astype(np.float64)
    y = np.linspace(0, 5, 30).astype(np.float64)
    z = np.random.rand(30, 50).astype(np.float64)
    return x, y, z


def process_events():
    """Process pending Qt events."""
    QApplication.processEvents()


def old(cls, *args, **kwargs):
    """An old-name object, through its deprecated constructor: the factories no longer make one."""
    with pytest.warns(DeprecationWarning, match=r"is deprecated"):
        return cls(*args, **kwargs)


def force_gc():
    """Force garbage collection and process pending Qt events."""
    gc.collect()
    QApplication.processEvents()
    gc.collect()


def collect():
    """Garbage-collects and delivers deferred deletes, which processEvents() alone does not."""
    from PySide6.QtCore import QCoreApplication, QEvent
    gc.collect()
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
    QApplication.processEvents()
    gc.collect()


def on_destroyed(obj):
    """A list that gets True once obj's C++ object is deleted; the slot holds no reference to obj."""
    gone = []
    obj.destroyed.connect(lambda *_: gone.append(True))
    return gone


def record(signal):
    """Every value `signal` emits, in order."""
    seen = []
    signal.connect(seen.append)
    return seen


def off_thread(call):
    """Runs `call` on a plain Python thread and waits; the call itself must not block."""
    thread = threading.Thread(target=call)
    thread.start()
    thread.join(timeout=10)
    assert not thread.is_alive(), "the off-thread call blocked"


def plots_of(host):
    """The plots of a plot (itself) or of a panel, in order."""
    from SciQLopPlots import SciQLopPlot
    if isinstance(host, SciQLopPlot):
        return [host]
    return [host.plot_at(i) for i in range(host.size())]


def canvas_of(plot):
    """The QCustomPlot widget of a plot: where mouse and key events go."""
    from PySide6.QtWidgets import QWidget
    return next(w for w in plot.findChildren(QWidget) if w.inherits("QCustomPlot"))


def scoped_host(scope, qtbot):
    """A plot ("plot") or a two-plot panel ("panel"), x over 0..10 and y over -1..1."""
    from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlot, SciQLopPlotRange
    x = np.linspace(0.0, 10.0, 200)
    if scope == "plot":
        host = SciQLopPlot()
        host.plot(x, np.sin(x))
        host.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    else:
        host = SciQLopMultiPlotPanel()
        host.plot(x, np.sin(x))
        host.plot(x, np.cos(x))
        host.set_time_axis_range(SciQLopPlotRange(0.0, 10.0))
    qtbot.addWidget(host)
    QApplication.processEvents()
    for plot in plots_of(host):
        plot.y_axis().set_range(SciQLopPlotRange(-1.0, 1.0))
    return host
