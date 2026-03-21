"""Actions that plot Python callbacks — the way SciQLop actually uses the library."""
from __future__ import annotations

from tests.fuzzing.actions import ui_action, settle
from tests.fuzzing.introspect import graph_count_on
from tests.fuzzing.data_providers import (
    make_timeseries,
    make_spectrogram,
    make_slow_timeseries,
    make_flaky_timeseries,
    make_flaky_spectrogram,
)


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added 3-component callback line graph to plot {plot_index}",
    model_update=lambda model, plot_index: model.graph_counts.__setitem__(
        plot_index, model.graph_counts.get(plot_index, 0) + 1
    ),
    verify=lambda panel, model, plot_index: True,
    settle_timeout_ms=200,
)
def add_callback_line_3c(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    plot.plot(make_timeseries(3), labels=["bx", "by", "bz"])


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added 1-component callback line graph to plot {plot_index}",
    model_update=lambda model, plot_index: model.graph_counts.__setitem__(
        plot_index, model.graph_counts.get(plot_index, 0) + 1
    ),
    verify=lambda panel, model, plot_index: True,
    settle_timeout_ms=200,
)
def add_callback_line_1c(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    plot.plot(make_timeseries(1), labels=["density"])


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added callback spectrogram to plot {plot_index}",
    # Only one colormap per plot is supported; don't track count
    model_update=lambda model, plot_index: None,
    verify=lambda panel, model, plot_index: True,
    settle_timeout_ms=300,
)
def add_callback_spectrogram(panel, model, plot_index):
    from SciQLopPlots import GraphType
    plot = panel.plot_at(plot_index)
    plot.plot(
        make_spectrogram(32),
        graph_type=GraphType.ColorMap,
        name="ion_spectra",
        y_log_scale=True,
        z_log_scale=True,
    )


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added slow callback line graph to plot {plot_index}",
    model_update=lambda model, plot_index: model.graph_counts.__setitem__(
        plot_index, model.graph_counts.get(plot_index, 0) + 1
    ),
    verify=lambda panel, model, plot_index: True,
    settle_timeout_ms=300,
)
def add_slow_callback_line(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    plot.plot(make_slow_timeseries(delay_s=0.05, n_components=3), labels=["x", "y", "z"])


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added flaky callback line graph to plot {plot_index}",
    model_update=lambda model, plot_index: model.graph_counts.__setitem__(
        plot_index, model.graph_counts.get(plot_index, 0) + 1
    ),
    verify=lambda panel, model, plot_index: True,
    settle_timeout_ms=200,
)
def add_flaky_callback_line(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    plot.plot(make_flaky_timeseries(failure_rate=0.3, n_components=2), labels=["a", "b"])


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added flaky callback spectrogram to plot {plot_index}",
    # Only one colormap per plot is supported; don't track count
    model_update=lambda model, plot_index: None,
    verify=lambda panel, model, plot_index: True,
    settle_timeout_ms=300,
)
def add_flaky_callback_spectrogram(panel, model, plot_index):
    from SciQLopPlots import GraphType
    plot = panel.plot_at(plot_index)
    plot.plot(
        make_flaky_spectrogram(failure_rate=0.3, n_channels=16),
        graph_type=GraphType.ColorMap,
        name="flaky_spectra",
        y_log_scale=True,
    )
