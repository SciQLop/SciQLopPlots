"""Viewport torture actions — rapid zoom, pan, rescale without waiting for settle."""
from __future__ import annotations

import numpy as np
from hypothesis import strategies as st

from tests.fuzzing.actions import ui_action, settle


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Rapid zoom burst on plot {plot_index}",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    settle_timeout_ms=200,
    weight=3,
)
def rapid_zoom_burst(panel, model, plot_index):
    """Zoom in and out rapidly without settling between steps."""
    plot = panel.plot_at(plot_index)
    from SciQLopPlots import SciQLopPlotRange

    r = plot.x_axis().range()
    center = r.center()
    size = r.size()
    for factor in [0.5, 0.25, 0.1, 0.5, 2.0, 4.0, 1.0]:
        half = size * factor / 2
        plot.x_axis().set_range(SciQLopPlotRange(center - half, center + half))


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Rapid pan burst on plot {plot_index}",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    settle_timeout_ms=200,
    weight=3,
)
def rapid_pan_burst(panel, model, plot_index):
    """Pan across the data rapidly without settling."""
    plot = panel.plot_at(plot_index)
    from SciQLopPlots import SciQLopPlotRange

    r = plot.x_axis().range()
    size = r.size()
    step = size * 0.3
    start = r.start()
    for i in range(10):
        s = start + step * i
        plot.x_axis().set_range(SciQLopPlotRange(s, s + size))


@ui_action(
    precondition=lambda model: model.has_graphs,
    bundles={"plot_index": "plot_indices"},
    narrate="Rescaled Y axis on plot {plot_index}",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    settle_timeout_ms=100,
    weight=2,
)
def rescale_y(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    plot.y_axis().rescale()


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Rapid log toggle burst on plot {plot_index}",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    settle_timeout_ms=200,
    weight=2,
)
def rapid_log_toggle(panel, model, plot_index):
    """Toggle log scale rapidly without settling — known crash trigger."""
    plot = panel.plot_at(plot_index)
    for _ in range(5):
        plot.y_axis().set_log(True)
        plot.y_axis().set_log(False)


@ui_action(
    precondition=lambda model: model.has_plots,
    narrate="Set time range on panel",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    settle_timeout_ms=300,
    weight=3,
)
def set_panel_time_range(panel, model):
    """Set a time range on the panel — triggers all callback graphs."""
    from SciQLopPlots import SciQLopPlotRange

    # Pick a random epoch range (roughly 2020-01-01 ± some hours)
    base = 1.577836800e9
    offset = np.random.uniform(-3600 * 24, 3600 * 24)
    duration = np.random.uniform(60, 3600)
    for i in range(panel.plot_count()):
        plot = panel.plot_at(i)
        plot.x_axis().set_range(
            SciQLopPlotRange(base + offset, base + offset + duration)
        )


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Zoom-pan-zoom sequence on plot {plot_index}",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    settle_timeout_ms=300,
    weight=2,
)
def zoom_pan_zoom(panel, model, plot_index):
    """Combined zoom-pan-zoom without settling — stresses the async pipeline."""
    plot = panel.plot_at(plot_index)
    from SciQLopPlots import SciQLopPlotRange

    r = plot.x_axis().range()
    center = r.center()
    size = r.size()

    # Zoom in
    half = size * 0.1
    plot.x_axis().set_range(SciQLopPlotRange(center - half, center + half))
    # Pan right
    plot.x_axis().set_range(SciQLopPlotRange(center, center + half * 2))
    # Pan further
    plot.x_axis().set_range(SciQLopPlotRange(center + half, center + half * 3))
    # Zoom out
    plot.x_axis().set_range(SciQLopPlotRange(center - size, center + size))
    # Zoom in again somewhere else
    plot.x_axis().set_range(SciQLopPlotRange(center + size * 0.5, center + size * 0.7))


@ui_action(
    strategies={
        "width": st.integers(min_value=50, max_value=2000),
        "height": st.integers(min_value=50, max_value=2000),
    },
    narrate="Resized panel to {width}x{height}",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    settle_timeout_ms=200,
    weight=2,
)
def resize_panel(panel, model, width, height):
    """Resize the panel widget — triggers relayout, replot, texture reallocation."""
    from PySide6.QtCore import QSize
    panel.resize(QSize(width, height))


@ui_action(
    narrate="Rapid resize burst on panel",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    settle_timeout_ms=300,
)
def rapid_resize_burst(panel, model):
    """Resize rapidly through several sizes without settling — stresses paint buffers."""
    from PySide6.QtCore import QSize
    sizes = [(200, 100), (800, 600), (50, 50), (1920, 1080), (300, 900), (400, 300)]
    for w, h in sizes:
        panel.resize(QSize(w, h))
