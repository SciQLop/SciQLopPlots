from __future__ import annotations


def count_plots(panel) -> int:
    return panel.size()


def graph_count_on(panel, plot_index: int) -> int:
    if plot_index >= panel.size():
        return 0
    plot = panel.plot_at(plot_index)
    return len(plot.plottables())


def axis_range(panel, plot_index: int) -> tuple[float, float] | None:
    if plot_index >= panel.size():
        return None
    plot = panel.plot_at(plot_index)
    r = plot.x_axis().range()
    return (r.start(), r.stop())
