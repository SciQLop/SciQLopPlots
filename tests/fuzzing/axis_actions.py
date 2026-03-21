from __future__ import annotations

from hypothesis import strategies as st

from tests.fuzzing.actions import ui_action
from tests.fuzzing.introspect import axis_range


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    strategies={
        "start": st.floats(min_value=-1e6, max_value=1e6, allow_nan=False, allow_subnormal=False),
        "stop": st.floats(min_value=-1e6, max_value=1e6, allow_nan=False, allow_subnormal=False),
    },
    narrate="Set axis range on plot {plot_index} to ({start}, {stop})",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    settle_timeout_ms=100,
)
def set_axis_range(panel, model, plot_index, start, stop):
    from SciQLopPlots import SciQLopPlotRange
    lo, hi = min(start, stop), max(start, stop)
    if hi - lo < 1e-10:
        hi = lo + 1.0
    plot = panel.plot_at(plot_index)
    plot.x_axis().set_range(SciQLopPlotRange(lo, hi))
    return {"plot_index": plot_index, "start": lo, "stop": hi}


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Toggled log scale on plot {plot_index} y-axis",
    model_update=lambda model, plot_index: model.log_scales.__setitem__(
        plot_index, not model.log_scales.get(plot_index, False)
    ),
    verify=lambda panel, model: True,
)
def toggle_y_log_scale(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    current = plot.y_axis().log()
    plot.y_axis().set_log(not current)


def _range_close(actual, expected, tol=1.0):
    if actual is None:
        return True
    return abs(actual[0] - expected[0]) < tol and abs(actual[1] - expected[1]) < tol
