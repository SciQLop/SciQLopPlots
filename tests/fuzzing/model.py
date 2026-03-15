from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class PlotModel:
    plot_count: int = 0
    graph_counts: dict[int, int] = field(default_factory=dict)
    span_counts: dict[int, int] = field(default_factory=dict)
    log_scales: dict[int, bool] = field(default_factory=dict)

    @property
    def has_plots(self) -> bool:
        return self.plot_count > 0

    @property
    def total_graph_count(self) -> int:
        return sum(self.graph_counts.values())

    @property
    def has_graphs(self) -> bool:
        return self.total_graph_count > 0

    def add_plot(self):
        idx = self.plot_count
        self.plot_count += 1
        self.graph_counts[idx] = 0
        self.span_counts[idx] = 0

    def remove_last_plot(self):
        if self.plot_count <= 0:
            return
        idx = self.plot_count - 1
        self.graph_counts.pop(idx, None)
        self.span_counts.pop(idx, None)
        self.log_scales.pop(idx, None)
        self.plot_count -= 1

    def reset(self):
        self.plot_count = 0
        self.graph_counts.clear()
        self.span_counts.clear()
        self.log_scales.clear()
