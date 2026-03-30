"""
Perf benchmarks for SciQLopPlots hot paths.

Run:
    pytest tests/perf/test_benchmarks.py -v
    pytest tests/perf/test_benchmarks.py -v --save-baseline
    pytest tests/perf/test_benchmarks.py -v --perf-threshold 1.3

Ported from multiplot-perf.py into pytest so they run in the normal dev loop.
"""

from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlotRange, PlotType

from perfutils import N_PLOTS, N_COLS, COLORS, make_static_data, make_data, wait_for_render


def test_synced_pan(static_panel, perf_check):
    panel = static_panel
    r = panel.plot_at(0).x_axis().range()
    step = r.size() * 0.005
    n = 100

    with perf_check("synced_pan", n):
        current = r
        for _ in range(n):
            current = SciQLopPlotRange(current.start() + step, current.stop() + step)
            panel.set_x_axis_range(current)
            for i in range(N_PLOTS):
                panel.plot_at(i).replot(True)


def test_y_zoom(static_panel, perf_check):
    panel = static_panel
    n = 50

    with perf_check("y_zoom", n):
        for i in range(n):
            zoom = 1.0 + 0.02 * (1.0 if i % 2 == 0 else -1.0)
            for p in range(N_PLOTS):
                plot = panel.plot_at(p)
                yr = plot.y_axis().range()
                center = (yr.start() + yr.stop()) * 0.5
                half = yr.size() * 0.5 * zoom
                plot.y_axis().set_range(SciQLopPlotRange(center - half, center + half))
                plot.replot(True)


def test_full_replot(static_panel, perf_check):
    panel = static_panel
    n = 20

    with perf_check("full_replot", n):
        for _ in range(n):
            for p in range(N_PLOTS):
                panel.plot_at(p).replot(True)


def test_panel_setup(qtbot, perf_check):
    n = 3
    labels = [f"ch{i}" for i in range(N_COLS)]
    colors = COLORS[:N_COLS]

    with perf_check("panel_setup", n):
        for _ in range(n):
            panel = SciQLopMultiPlotPanel(synchronize_x=True)
            qtbot.addWidget(panel)
            panel.resize(1920, 1080)
            for _ in range(N_PLOTS):
                x, y = make_static_data()
                panel.plot(x, y, labels=labels, colors=colors, plot_type=PlotType.BasicXY)
            panel.show()
            wait_for_render(panel)
            panel.close()
            panel.deleteLater()
            QApplication.processEvents()


def test_callable_pan(callable_panel, perf_check):
    panel = callable_panel
    r = panel.plot_at(0).x_axis().range()
    step = r.size() * 0.005
    n = 50

    with perf_check("callable_pan", n):
        current = r
        for _ in range(n):
            current = SciQLopPlotRange(current.start() + step, current.stop() + step)
            panel.set_time_axis_range(current)
            for p in range(N_PLOTS):
                panel.plot_at(p).replot(False)
            QApplication.processEvents()
