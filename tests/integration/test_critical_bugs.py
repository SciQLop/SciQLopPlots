"""Reproducer tests for critical bugs found in 2026-03-24 code review.

Bug 1: Heap corruption — PyTuple_New(2) for 3-arg get_data (PythonInterface.cpp:568)
Bug 2+3: Data races in resampler — _plot_info read without correct mutex
        (AbstractResampler.hpp:107,111,133). Not reliably reproducible without
        TSAN; included as a stress test that exercises the race window.
Bug 4: CurveResampler leak — deleteLater on dead thread (SciQLopCurve.cpp:48-57).
        Not reliably reproducible without ASAN; included as a lifecycle test.
Bug 5: Iterator invalidation — removeOne inside range-for in updatePlotList
        (SciQLopMultiPlotObject.cpp:63-70).
"""
import pytest
import numpy as np
import gc
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QTimer

from SciQLopPlots import (
    SciQLopPlot,
    SciQLopMultiPlotPanel,
    SciQLopPlotRange,
    SciQLopGraphInterface,
    GraphType,
)
from conftest import process_events, force_gc


class TestBug1_HeapCorruptionColormapCallback:
    """PyTuple_New(2) allocates 2 slots but get_data(x, y, z) writes index 2.

    The 3-arg overload is used when a colormap callback receives (x, y, z)
    from a range update. This causes a heap buffer overflow.
    """

    def test_colormap_callback_with_xyz_no_crash(self, qtbot):
        """Colormap with callable must not crash on range change.

        The callback fires on a worker thread after a 20 ms rate-limit timer
        (DataProviderInterface), so wait for ``call_count`` to advance rather
        than relying on ``processEvents``.
        """
        plot = SciQLopPlot()
        qtbot.addWidget(plot)

        call_count = 0

        def make_colormap_data(start, stop):
            nonlocal call_count
            call_count += 1
            x = np.linspace(start, stop, 20, dtype=np.float64)
            y = np.linspace(0, 5, 10, dtype=np.float64)
            z = np.random.rand(10, 20).astype(np.float64)
            return x, y, z

        cmap = plot.colormap(make_colormap_data)
        assert cmap is not None
        process_events()

        # Trigger range change — this calls get_data(x, y, z) internally.
        # Heap overflow from PyTuple_New(2) would crash here if the bug returned.
        plot.x_axis().set_range(SciQLopPlotRange(1.0, 5.0))
        qtbot.wait(200)
        plot.x_axis().set_range(SciQLopPlotRange(2.0, 8.0))
        qtbot.wait(200)

        assert call_count >= 1, "Callback was never invoked"


class TestBug5_IteratorInvalidationUpdatePlotList:
    """SciQLopMultiPlotObject.updatePlotList removes from m_plots while
    iterating it with a range-for. This causes skipped elements or crashes.

    Reproducer: add 3 plots to a panel, then remove 2 in a single operation
    (by replacing the panel content). The removal loop should handle all plots.
    """

    def test_remove_multiple_plots_no_crash(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)

        # Add 3 plots
        panel.add_plot(SciQLopPlot())
        panel.add_plot(SciQLopPlot())
        panel.add_plot(SciQLopPlot())
        process_events()
        assert panel.size() == 3

        # Remove all plots one by one — this triggers updatePlotList
        # which has the iterator invalidation bug
        while panel.size() > 0:
            panel.remove_plot(panel.plot_at(0))
            process_events()

        assert panel.size() == 0

    def test_remove_middle_plot_others_survive(self, qtbot):
        """Removing a middle plot should not skip or corrupt neighbors."""
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)

        p1 = SciQLopPlot()
        p2 = SciQLopPlot()
        p3 = SciQLopPlot()
        panel.add_plot(p1)
        panel.add_plot(p2)
        panel.add_plot(p3)
        process_events()

        panel.remove_plot(p2)
        process_events()

        assert panel.size() == 2
        plots = panel.plots()
        assert len(plots) == 2


class TestBug2_3_ResamplerRaceConditions:
    """Data race: _plot_info read without _plot_info_mutex in _async_resample_callback
    and _resample(). Not reliably reproducible without TSAN.

    This stress test rapidly alternates between data updates and range changes
    to maximize the race window. Under TSAN this should report the race.
    """

    def test_rapid_range_and_data_updates_no_crash(self, qtbot, sample_data):
        """Stress test: interleave range changes and data updates."""
        x, y = sample_data
        plot = SciQLopPlot()
        qtbot.addWidget(plot)

        g = plot.line(x, y)
        process_events()

        for i in range(50):
            plot.x_axis().set_range(SciQLopPlotRange(float(i % 5), float(5 + i % 5)))
            if i % 3 == 0:
                new_y = np.sin(x + i * 0.1).astype(np.float64)
                g.set_data(x, new_y)
            process_events()


class TestBug4_CurveResamplerLeak:
    """CurveResampler leaked in clear_resampler: deleteLater posted to a dead
    thread's event loop never fires.

    This test creates and destroys a curve to exercise the leak path.
    Under ASAN/valgrind this should report the leak.
    """

    def test_curve_create_destroy_no_crash(self, qtbot):
        plot = SciQLopPlot()
        qtbot.addWidget(plot)

        x = np.linspace(0, 10, 100, dtype=np.float64)
        t = np.linspace(0, 2 * np.pi, 100, dtype=np.float64)
        px = np.cos(t).astype(np.float64)
        py = np.sin(t).astype(np.float64)

        curve = plot.parametric_curve(px, py)
        assert curve is not None
        process_events()

        # Deleting the plot destroys the curve, which calls clear_resampler
        del curve
        force_gc()
