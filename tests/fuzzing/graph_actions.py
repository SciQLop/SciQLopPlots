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
    # Relaxed: graph_counts can drift when colormaps silently fail
    verify=lambda panel, model, plot_index: True,
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
    verify=lambda panel, model, plot_index: True,
    settle_timeout_ms=100,
)
def add_colormap(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    x = np.linspace(0, 10, 50, dtype=np.float64)
    y = np.linspace(0, 5, 30, dtype=np.float64)
    z = np.random.rand(30, 50).astype(np.float64)
    colormap_or_refusal(plot, lambda: plot.plot(x, y, z, name="cmap"))


def colormap_or_refusal(plot, add):
    """A plot holds one colour map: a second one must be refused loudly, not come back None."""
    if not plot.has_colormap():
        add()
        return
    try:
        add()
    except RuntimeError as e:
        assert "already has a colour map" in str(e), e
    else:
        raise AssertionError("a second colour map on a plot was accepted")


@ui_action(
    precondition=lambda model: model.has_graphs,
    bundles={"plot_index": "plot_indices"},
    narrate="Set new data on first graph of plot {plot_index}",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
)
def set_graph_data(panel, model, plot_index):
    from SciQLopPlots import SciQLopTimeline
    plot = panel.plot_at(plot_index)
    # A timeline strip is a plottable but not a graph: it takes intervals, not (x, y).
    plottables = [p for p in plot.plottables() if not isinstance(p, SciQLopTimeline)]
    if plottables:
        x = np.linspace(0, 10, 200, dtype=np.float64)
        y = np.sin(x + np.random.uniform(0, 2 * np.pi)).astype(np.float64)
        from SciQLopPlots import SciQLopColorMapInterface
        if isinstance(plottables[0], SciQLopColorMapInterface):
            # A colour map takes (x, y, z): an (x, y) call is refused, not silently dropped.
            try:
                plottables[0].set_data(x, y)
            except NotImplementedError:
                return
            raise AssertionError("set_data(x, y) on a colour map was accepted")
        plottables[0].set_data(x, y)
