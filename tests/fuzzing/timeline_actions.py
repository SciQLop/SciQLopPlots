from __future__ import annotations

import hypothesis.strategies as st
import numpy as np

from tests.fuzzing.actions import ui_action


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    strategies={
        "n": st.integers(min_value=0, max_value=300),
        "lanes": st.integers(min_value=1, max_value=6),
        "editable": st.booleans(),
    },
    narrate="Added a timeline strip with {n} intervals on {lanes} lanes to plot {plot_index}",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
)
def add_timeline_strip(panel, model, plot_index, n, lanes, editable):
    plot = panel.plot_at(plot_index)
    if not hasattr(plot, "add_timeline"):
        return
    rng = np.random.default_rng(n)
    start = np.sort(rng.uniform(0, 1e6, n))
    tl = plot.add_timeline()
    tl.set_intervals(start, start + rng.uniform(0, 1e4, n),
                     lane=rng.integers(0, lanes, n), category=rng.integers(0, 3, n))
    tl.editable = editable
