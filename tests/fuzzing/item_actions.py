from __future__ import annotations

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
