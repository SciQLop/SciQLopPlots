from __future__ import annotations


def count_plots(panel) -> int:
    return len(panel.plots())


def graph_count_on(panel, plot_index: int) -> int:
    plots = panel.plots()
    if plot_index >= len(plots):
        return 0
    return len(plots[plot_index].plottables())


def axis_range(panel, plot_index: int) -> tuple[float, float] | None:
    plots = panel.plots()
    if plot_index >= len(plots):
        return None
    r = plots[plot_index].x_axis().range()
    return (r.start(), r.stop())
