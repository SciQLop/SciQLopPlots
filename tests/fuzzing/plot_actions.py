from __future__ import annotations

import numpy as np

from tests.fuzzing.actions import ui_action, ActionRegistry
from tests.fuzzing.introspect import count_plots

registry = ActionRegistry()


@registry.register
@ui_action(
    target="plot_indices",
    narrate="Added plot at index {result}",
    model_update=lambda model: model.add_plot(),
    verify=lambda panel, model: count_plots(panel) == model.plot_count,
)
def add_plot(panel, model):
    from SciQLopPlots import PlotType
    panel.create_plot(plot_type=PlotType.TimeSeries)
    return len(panel.plots()) - 1


@registry.register
@ui_action(
    precondition=lambda model: model.has_plots,
    narrate="Removed last plot (was at index {result})",
    model_update=lambda model: model.remove_last_plot(),
    verify=lambda panel, model: count_plots(panel) == model.plot_count,
)
def remove_last_plot(panel, model):
    idx = len(panel.plots()) - 1
    plot = panel.plot_at(idx)
    panel.remove_plot(plot)
    return idx


@registry.register
@ui_action(
    precondition=lambda model: model.has_multiple_plots,
    narrate="Removed random plot at index {result}",
    model_update=lambda model, result: model.remove_plot(result),
    verify=lambda panel, model: count_plots(panel) == model.plot_count,
)
def remove_random_plot(panel, model):
    n = len(panel.plots())
    idx = int(np.random.randint(0, n))
    plot = panel.plot_at(idx)
    panel.remove_plot(plot)
    return idx


@registry.register
@ui_action(
    target="plot_indices",
    narrate="Inserted plot at index {result}",
    model_update=lambda model: model.add_plot(),
    verify=lambda panel, model: count_plots(panel) == model.plot_count,
)
def insert_plot_at_random(panel, model):
    from SciQLopPlots import PlotType
    n = len(panel.plots())
    idx = int(np.random.randint(0, max(n, 1) + 1))
    panel.create_plot(index=idx, plot_type=PlotType.TimeSeries)
    return idx
