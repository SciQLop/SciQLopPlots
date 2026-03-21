"""Actions for creating and removing nested panels within the root panel."""
from __future__ import annotations

import numpy as np

from tests.fuzzing.actions import ui_action


@ui_action(
    narrate="Added nested panel",
    model_update=lambda model: model.add_panel(),
    verify=lambda panel, model: True,
    settle_timeout_ms=100,
)
def add_nested_panel(panel, model):
    from SciQLopPlots import SciQLopMultiPlotPanel
    from PySide6.QtCore import Qt
    child = SciQLopMultiPlotPanel(
        synchronize_x=False, synchronize_time=True,
    )
    panel.add_panel(child)


@ui_action(
    precondition=lambda model: model.has_panels,
    narrate="Removed a nested panel",
    model_update=lambda model: model.remove_panel(),
    verify=lambda panel, model: True,
    settle_timeout_ms=100,
)
def remove_nested_panel(panel, model):
    # panels() isn't exposed; walk children to find nested panels
    from SciQLopPlots import SciQLopMultiPlotPanel
    children = panel.findChildren(SciQLopMultiPlotPanel)
    # Filter to direct children only (not the panel itself)
    children = [c for c in children if c is not panel and c.parent() is not None]
    if children:
        victim = children[int(np.random.randint(0, len(children)))]
        panel.remove_panel(victim)
