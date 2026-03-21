"""Actions for creating, manipulating, and removing spans (catalog-like items)."""
from __future__ import annotations

import numpy as np
from PySide6.QtGui import QColor

from tests.fuzzing.actions import ui_action


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added vertical span to plot {plot_index}",
    model_update=lambda model, plot_index: model.span_counts.__setitem__(
        plot_index, model.span_counts.get(plot_index, 0) + 1
    ),
    verify=lambda panel, model: True,
)
def add_vertical_span(panel, model, plot_index):
    from SciQLopPlots import SciQLopVerticalSpan, SciQLopPlotRange

    plot = panel.plot_at(plot_index)
    r = plot.x_axis().range()
    center = r.center()
    span_range = SciQLopPlotRange(center - r.size() * 0.1, center + r.size() * 0.1)
    SciQLopVerticalSpan(
        plot, span_range,
        QColor(100, 100, 200, 80),
        read_only=False, visible=True,
        tool_tip="test span",
    )


@ui_action(
    precondition=lambda model: model.has_plots,
    narrate="Added multi-plot vertical span (catalog event)",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    weight=2,
)
def add_multi_plot_span(panel, model):
    """Add a MultiPlotsVerticalSpan across the whole panel — mimics SciQLop catalog events."""
    from SciQLopPlots import MultiPlotsVerticalSpan, SciQLopPlotRange

    # Use the first plot's x range as reference
    plot = panel.plot_at(0)
    r = plot.x_axis().range()
    offset = np.random.uniform(-0.4, 0.4) * r.size()
    width = np.random.uniform(0.02, 0.2) * r.size()
    center = r.center() + offset
    span_range = SciQLopPlotRange(center - width / 2, center + width / 2)
    color = QColor(
        int(np.random.randint(50, 255)),
        int(np.random.randint(50, 255)),
        int(np.random.randint(50, 255)),
        80,
    )
    MultiPlotsVerticalSpan(
        panel, span_range, color,
        read_only=False, visible=True,
        tool_tip="catalog event",
    )


@ui_action(
    precondition=lambda model: model.has_plots,
    narrate="Added burst of {count} multi-plot spans (catalog load)",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    settle_timeout_ms=300,
    weight=2,
)
def add_span_burst(panel, model):
    """Add many spans at once — mimics loading a catalog with dozens of events."""
    from SciQLopPlots import MultiPlotsVerticalSpan, SciQLopPlotRange

    plot = panel.plot_at(0)
    r = plot.x_axis().range()
    count = int(np.random.randint(10, 50))
    for i in range(count):
        pos = r.start() + r.size() * (i + 0.5) / count
        width = r.size() * 0.005
        span_range = SciQLopPlotRange(pos - width, pos + width)
        MultiPlotsVerticalSpan(
            panel, span_range,
            QColor(200, 100, 100, 60),
            read_only=False, visible=True,
            tool_tip=f"event_{i}",
        )


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added burst of per-plot spans to plot {plot_index}",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
    settle_timeout_ms=200,
)
def add_per_plot_span_burst(panel, model, plot_index):
    """Add many per-plot vertical spans — stresses per-plot item management."""
    from SciQLopPlots import SciQLopVerticalSpan, SciQLopPlotRange

    plot = panel.plot_at(plot_index)
    r = plot.x_axis().range()
    count = int(np.random.randint(5, 20))
    for i in range(count):
        pos = r.start() + r.size() * (i + 0.5) / count
        width = r.size() * 0.01
        SciQLopVerticalSpan(
            plot, SciQLopPlotRange(pos - width, pos + width),
            QColor(100, 200, 100, 60),
            read_only=False, visible=True,
            tool_tip=f"span_{i}",
        )
