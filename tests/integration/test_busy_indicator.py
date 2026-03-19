"""Integration tests for busy indicator API."""
import pytest
import numpy as np
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopPlot, GraphType


class TestBusyAPIBase:
    """Tests that the busy API exists on the interface."""

    def test_busy_exists(self, plot, sample_data):
        x, y = sample_data
        graph = plot.plot(x, y, labels=["sig"], graph_type=GraphType.ParametricCurve)
        QApplication.processEvents()
        # busy() may be True (resampler running) or False — just verify callable
        assert isinstance(graph.busy(), bool)

    def test_set_busy_callable(self, plot, sample_data):
        x, y = sample_data
        graph = plot.plot(x, y, labels=["sig"], graph_type=GraphType.ParametricCurve)
        QApplication.processEvents()
        graph.set_busy(True)
        graph.set_busy(False)


class TestLineGraphBusy:
    def test_set_busy_delegates(self, plot, sample_multicomponent_data):
        x, y = sample_multicomponent_data
        graph = plot.plot(x, y, labels=["a", "b", "c"])
        QApplication.processEvents()
        graph.set_busy(True)
        assert graph.busy() is True

    def test_set_busy_false_clears(self, plot, sample_multicomponent_data):
        x, y = sample_multicomponent_data
        graph = plot.plot(x, y, labels=["a", "b", "c"])
        QApplication.processEvents()
        graph.set_busy(True)
        graph.set_busy(False)
        assert graph.busy() is False


class TestSingleLineGraphBusy:
    def test_set_busy_delegates(self, plot, sample_data):
        x, y = sample_data
        graph = plot.plot(x, y, labels=["sig"])
        QApplication.processEvents()
        graph.set_busy(True)
        assert graph.busy() is True

    def test_busy_changed_signal_true(self, plot, sample_data):
        x, y = sample_data
        graph = plot.plot(x, y, labels=["sig"])
        QApplication.processEvents()
        received = []
        graph.busy_changed.connect(lambda v: received.append(v))
        graph.set_busy(True)
        QApplication.processEvents()
        assert True in received

    def test_busy_changed_signal_false(self, plot, sample_data):
        x, y = sample_data
        graph = plot.plot(x, y, labels=["sig"])
        QApplication.processEvents()
        graph.set_busy(True)
        received = []
        graph.busy_changed.connect(lambda v: received.append(v))
        graph.set_busy(False)
        QApplication.processEvents()
        assert False in received


class TestColorMapBusy:
    def test_set_busy(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.plot(x, y, z)
        QApplication.processEvents()
        cmap.set_busy(True)
        assert cmap.busy() is True

    def test_set_busy_on_empty_colormap(self, plot):
        """Colormap without data has no pipeline activity."""
        cmap = plot.plot(
            np.array([0.0, 1.0]),
            np.array([0.0, 1.0]),
            np.zeros((2, 2)),
        )
        # Let the pipeline settle
        for _ in range(10):
            QApplication.processEvents()
        cmap.set_busy(True)
        assert cmap.busy() is True
        cmap.set_busy(False)
        assert cmap.busy() is False


class TestCurveBusy:
    def test_set_busy_external(self, plot, sample_data):
        x, y = sample_data
        graph = plot.plot(x, y, labels=["sig"], graph_type=GraphType.ParametricCurve)
        # Wait for resampler to finish
        import time
        for _ in range(50):
            QApplication.processEvents()
            time.sleep(0.01)
        graph.set_busy(True)
        assert graph.busy() is True
        graph.set_busy(False)
        assert graph.busy() is False

    def test_busy_true_after_set_data(self, plot, sample_data):
        """Curve should be busy immediately after set_data (resampler working)."""
        x, y = sample_data
        graph = plot.plot(x, y, labels=["sig"], graph_type=GraphType.ParametricCurve)
        # Right after data is set, curve should be busy
        assert graph.busy() is True


class TestLineGraphBusySignal:
    def test_busy_changed_signal(self, plot, sample_multicomponent_data):
        x, y = sample_multicomponent_data
        graph = plot.plot(x, y, labels=["a", "b", "c"])
        QApplication.processEvents()
        received = []
        graph.busy_changed.connect(lambda v: received.append(v))
        graph.set_busy(True)
        QApplication.processEvents()
        assert True in received


class TestBusyPythonBindings:
    def test_set_busy_from_python(self, plot, sample_data):
        """Simulates SciQLop setting busy during data download."""
        x, y = sample_data
        graph = plot.plot(x, y, labels=["sig"])
        QApplication.processEvents()
        graph.set_busy(True)
        assert graph.busy() is True
        graph.set_busy(False)
        assert graph.busy() is False

    def test_busy_signal_connectable_from_python(self, plot, sample_data):
        """Python can connect to busy_changed signal."""
        x, y = sample_data
        graph = plot.plot(x, y, labels=["sig"])
        QApplication.processEvents()
        received = []
        graph.busy_changed.connect(lambda v: received.append(v))
        graph.set_busy(True)
        QApplication.processEvents()
        graph.set_busy(False)
        QApplication.processEvents()
        assert received == [True, False]


class TestHistogram2DBusy:
    def test_set_busy(self, plot):
        hist = plot.add_histogram2d("test", 20, 20)
        QApplication.processEvents()
        hist.set_busy(True)
        assert hist.busy() is True

    def test_busy_changed_signal(self, plot):
        hist = plot.add_histogram2d("test", 20, 20)
        QApplication.processEvents()
        received = []
        hist.busy_changed.connect(lambda v: received.append(v))
        hist.set_busy(True)
        QApplication.processEvents()
        assert True in received
