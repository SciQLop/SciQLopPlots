import pytest
import gc
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


def force_gc():
    """Force garbage collection and process pending Qt events."""
    gc.collect()
    QApplication.processEvents()
    gc.collect()
