from __future__ import annotations

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
    return panel.size() - 1


@registry.register
@ui_action(
    precondition=lambda model: model.has_plots,
    narrate="Removed last plot (was at index {result})",
    model_update=lambda model: model.remove_last_plot(),
    verify=lambda panel, model: count_plots(panel) == model.plot_count,
)
def remove_last_plot(panel, model):
    idx = panel.size() - 1
    plot = panel.plot_at(idx)
    panel.remove_plot(plot)
    return idx
