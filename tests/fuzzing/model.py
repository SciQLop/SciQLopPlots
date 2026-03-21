from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class PlotModel:
    plot_count: int = 0
    graph_counts: dict[int, int] = field(default_factory=dict)
    span_counts: dict[int, int] = field(default_factory=dict)
    log_scales: dict[int, bool] = field(default_factory=dict)
    panel_count: int = 0

    @property
    def has_plots(self) -> bool:
        return self.plot_count > 0

    @property
    def has_multiple_plots(self) -> bool:
        return self.plot_count > 1

    @property
    def total_graph_count(self) -> int:
        return sum(self.graph_counts.values())

    @property
    def has_graphs(self) -> bool:
        return self.total_graph_count > 0

    @property
    def has_panels(self) -> bool:
        return self.panel_count > 0

    def add_plot(self):
        idx = self.plot_count
        self.plot_count += 1
        self.graph_counts[idx] = 0
        self.span_counts[idx] = 0

    def remove_plot(self, index: int):
        if self.plot_count <= 0:
            return
        self.graph_counts.pop(index, None)
        self.span_counts.pop(index, None)
        self.log_scales.pop(index, None)
        # Shift higher indices down
        for i in range(index + 1, self.plot_count):
            for d in (self.graph_counts, self.span_counts, self.log_scales):
                if i in d:
                    d[i - 1] = d.pop(i)
        self.plot_count -= 1

    def remove_last_plot(self):
        if self.plot_count > 0:
            self.remove_plot(self.plot_count - 1)

    def add_panel(self):
        self.panel_count += 1

    def remove_panel(self):
        if self.panel_count > 0:
            self.panel_count -= 1

    def reset(self):
        self.plot_count = 0
        self.graph_counts.clear()
        self.span_counts.clear()
        self.log_scales.clear()
        self.panel_count = 0
