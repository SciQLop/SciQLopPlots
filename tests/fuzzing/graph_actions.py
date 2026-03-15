from __future__ import annotations

import numpy as np
from PySide6.QtGui import QColorConstants

from tests.fuzzing.actions import ui_action
from tests.fuzzing.introspect import graph_count_on


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added line graph to plot {plot_index}",
    model_update=lambda model, plot_index: model.graph_counts.__setitem__(
        plot_index, model.graph_counts.get(plot_index, 0) + 1
    ),
    verify=lambda panel, model, plot_index: (
        graph_count_on(panel, plot_index) == model.graph_counts.get(plot_index, 0)
    ),
)
def add_line_graph(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    x = np.linspace(0, 10, 200, dtype=np.float64)
    y = np.sin(x + np.random.uniform(0, 2 * np.pi)).astype(np.float64)
    plot.plot(x, y, labels=["line"], colors=[QColorConstants.Red])


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added colormap to plot {plot_index}",
    model_update=lambda model, plot_index: model.graph_counts.__setitem__(
        plot_index, model.graph_counts.get(plot_index, 0) + 1
    ),
    verify=lambda panel, model, plot_index: (
        graph_count_on(panel, plot_index) == model.graph_counts.get(plot_index, 0)
    ),
    settle_timeout_ms=100,
)
def add_colormap(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    x = np.linspace(0, 10, 50, dtype=np.float64)
    y = np.linspace(0, 5, 30, dtype=np.float64)
    z = np.random.rand(30, 50).astype(np.float64)
    plot.plot(x, y, z, name="cmap")


@ui_action(
    precondition=lambda model: model.has_graphs,
    bundles={"plot_index": "plot_indices"},
    narrate="Set new data on first graph of plot {plot_index}",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
)
def set_graph_data(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    plottables = plot.plottables()
    if plottables:
        x = np.linspace(0, 10, 200, dtype=np.float64)
        y = np.sin(x + np.random.uniform(0, 2 * np.pi)).astype(np.float64)
        plottables[0].set_data(x, y)
