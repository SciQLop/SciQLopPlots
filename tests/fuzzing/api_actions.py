"""Story actions for the API the first actions never reached (2026-11-01 campaign): histograms, bands,
error bars, groups, items, legend, theme, save, observers, a failing provider, a projection plot, and a
'poke' that calls a random setter of a random live object with arguments from the API sweep's guesses."""
from __future__ import annotations

import os
import sys
import tempfile
import warnings

import numpy as np
from hypothesis import strategies as st
from PySide6.QtCore import QPointF, QRectF
from PySide6.QtGui import QColor, QPixmap

from tests.fuzzing.actions import settle, ui_action

X = np.linspace(0.0, 10.0, 200)
PICK = st.integers(min_value=0, max_value=10**6)

# What a setter may raise when the sweep's argument guess doesn't fit it: the contract's classes.
REFUSALS = (TypeError, ValueError, IndexError, OverflowError, NotImplementedError)


def _no_change(model, **_):
    return None


def _always(panel, model, **_):
    return True


def _plot(panel, plot_index):
    from SciQLopPlots import SciQLopPlot
    plot = panel.plot_at(plot_index)
    return plot if isinstance(plot, SciQLopPlot) else None


def _with_plot(narrate, **extra):
    return ui_action(precondition=lambda model: model.has_plots, bundles={"plot_index": "plot_indices"},
                     narrate=narrate, model_update=_no_change, verify=_always, **extra)


def _or_time_series_refusal(plot, add):
    """A time-series plot refuses histograms and bars (NotImplementedError naming it): refused loudly is fine."""
    from SciQLopPlots import SciQLopTimeSeriesPlot
    if not isinstance(plot, SciQLopTimeSeriesPlot):
        add()
        return
    try:
        add()
    except NotImplementedError as e:
        assert "SciQLopTimeSeriesPlot" in str(e), e
    else:
        raise AssertionError("a time-series plot took a histogram or bars")


@_with_plot("Added a histogram to plot {plot_index}")
def add_histogram(panel, model, plot_index):
    if plot := _plot(panel, plot_index):
        _or_time_series_refusal(plot, lambda: plot.histogram(np.random.default_rng().normal(size=500)))


@_with_plot("Added a 2-D histogram to plot {plot_index}")
def add_histogram2d(panel, model, plot_index):
    from tests.fuzzing.graph_actions import colormap_or_refusal
    if plot := _plot(panel, plot_index):
        colormap_or_refusal(plot, lambda: plot.histogram2d(X, np.sin(X)))


@_with_plot("Added bars to plot {plot_index}")
def add_bars(panel, model, plot_index):
    if plot := _plot(panel, plot_index):
        _or_time_series_refusal(plot, lambda: plot.bars(np.linspace(0.0, 1.0, 11), np.arange(10.0)))


@_with_plot("Added a band to plot {plot_index}")
def add_band(panel, model, plot_index):
    if plot := _plot(panel, plot_index):
        plot.band(X, np.sin(X) - 0.5, np.sin(X) + 0.5)


@_with_plot("Added error bars to plot {plot_index}")
def add_error_bars(panel, model, plot_index):
    if plot := _plot(panel, plot_index):
        plot.error_bars(X[::10], np.cos(X[::10]), 0.1)


@_with_plot("Grouped two graphs of plot {plot_index}")
def group_two_graphs(panel, model, plot_index):
    if plot := _plot(panel, plot_index):
        members = [plot.plot(X, np.sin(X), labels=["g1"]), plot.plot(X, np.cos(X), labels=["g2"])]
        plot.group("group", members)


ITEM_FACTORIES = [
    lambda p: p.text("t", QPointF(10, 10)),
    lambda p: p.ellipse(QRectF(10, 10, 20, 20)),
    lambda p: p.pixmap(QPixmap(4, 4), QRectF(1, 1, 2, 2)),
    lambda p: p.curved_line(QPointF(0, 0), QPointF(20, 20)),
    lambda p: p.horizontal_line(0.5),
    lambda p: p.vertical_line(p.x_axis().range().center()),
    lambda p: p.horizontal_span(p.y_axis().range()),
]


@_with_plot("Added item {pick} to plot {plot_index}", strategies={"pick": PICK})
def add_item(panel, model, plot_index, pick):
    if plot := _plot(panel, plot_index):
        ITEM_FACTORIES[pick % len(ITEM_FACTORIES)](plot)


@_with_plot("Toggled the legend of plot {plot_index}")
def toggle_legend(panel, model, plot_index):
    if plot := _plot(panel, plot_index):
        legend = plot.legend()
        legend.set_visible(not legend.visible())


@ui_action(narrate="Switched the panel theme ({pick})", strategies={"pick": PICK},
           model_update=_no_change, verify=_always)
def switch_theme(panel, model, pick):
    from SciQLopPlots import SciQLopTheme
    panel.set_theme(SciQLopTheme.dark() if pick % 2 else SciQLopTheme.light())


@ui_action(precondition=lambda model: model.has_plots, narrate="Saved the panel as PNG",
           model_update=_no_change, verify=_always)
def save_panel_png(panel, model):
    path = os.path.join(tempfile.gettempdir(), "sciqlopplots_story.png")
    panel.save(path, width=200, height=150)


@_with_plot("Observed the x range of plot {plot_index}")
def observe_x_range(panel, model, plot_index):
    if plot := _plot(panel, plot_index):
        seen = []
        plot._story_observer = plot.x_axis().on.range >> (lambda event: seen.append(event))


@_with_plot("Added a provider that raises to plot {plot_index}, then gave it data", settle_timeout_ms=200)
def failing_provider(panel, model, plot_index):
    """J1: the error is reported (excepthook + fetch_failed) and the graph stays usable."""
    from SciQLopPlots import SciQLopPlotRange
    if not (plot := _plot(panel, plot_index)):
        return
    failures = []

    def broken(start, stop):
        raise RuntimeError("story: provider offline")

    hook, sys.excepthook = sys.excepthook, lambda *args: None
    try:
        graph = plot.plot(broken)
        graph.fetch_failed.connect(failures.append)
        r = plot.x_axis().range()
        plot.x_axis().set_range(SciQLopPlotRange(r.start() - 1.0, r.stop() + 1.0))
        settle(150)
        # Usable after the failure: a working callback takes over (and later fetches don't raise).
        graph.set_callable(lambda start, stop: (X, np.sin(X)))
        settle(50)
    finally:
        sys.excepthook = hook


@ui_action(narrate="Built a projection plot, fed it, then deleted it", strategies={"pick": PICK},
           model_update=_no_change, verify=_always, settle_timeout_ms=100)
def projection_plot_round_trip(panel, model, pick):
    import shiboken6
    from SciQLopPlots import SciQLopNDProjectionPlot
    panes = 2 + pick % 3
    nd = SciQLopNDProjectionPlot(panes)
    nd.plot([X] + [np.sin(X * (k + 1)) for k in range(panes)])
    nd.set_dimension_pairs([(k, (k + 1) % panes) for k in range(panes)])
    settle(50)
    shiboken6.delete(nd)


def _live_objects(plot):
    objects = [plot, plot.x_axis(), plot.y_axis(), plot.legend()]
    for plottable in plot.plottables():
        objects.append(plottable)
        objects += list(getattr(plottable, "components", lambda: [])())
    return [o for o in objects if o is not None]


_HOSTS = None


def _hosts():
    """The sweep's helper hosts, for argument guesses that need another plot or axis."""
    global _HOSTS
    if _HOSTS is None:
        sweep_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "sweeps", "api_fuzz")
        sys.path.insert(0, sweep_dir)
        import child
        _HOSTS = (child, child.Hosts())
    return _HOSTS


# Calls that change what the story's model counts (plots, graphs) or tear objects down: other actions do these.
DESTRUCTIVE = ("remove", "delete", "clear", "take", "close", "destroy", "deleteLater", "setParent", "move",
               "insert", "add", "plot", "line", "scatter", "colormap", "histogram", "waterfall", "parametric_curve",
               "projection", "bars", "band", "error_bars", "group", "span", "text", "ellipse", "pixmap",
               "curved_line", "straight_line", "vertical", "horizontal", "save", "register", "observe",
               "model_curve", "reference_curve", "set_callable")


def _pokeable(obj):
    from tests.sweeps.api_fuzz.catalogue import SKIP
    return sorted(n for n in dir(obj)
                  if not n.startswith("_") and n not in SKIP and not n.startswith(DESTRUCTIVE)
                  and callable(getattr(obj, n, None)) and not isinstance(getattr(type(obj), n, None), type))


@ui_action(precondition=lambda model: model.has_plots, bundles={"plot_index": "plot_indices"},
           narrate="Poked method {pick} of an object of plot {plot_index}", strategies={"pick": PICK},
           model_update=_no_change, verify=_always, weight=5)
def poke_random_method(panel, model, plot_index, pick):
    """Any method, with an argument the API sweep found valid somewhere, must work or refuse cleanly."""
    if not (plot := _plot(panel, plot_index)):
        return
    objects = _live_objects(plot)
    obj = objects[pick % len(objects)]
    names = _pokeable(obj)
    if not names:
        return
    name = names[(pick // len(objects)) % len(names)]
    child, hosts = _hosts()
    candidates = child.candidate_args("", obj, name, hosts)
    if not candidates:
        return
    args = candidates[(pick // 7) % len(candidates)]
    try:
        # Deprecated aliases are API too: call them, without the warning the test config turns into an error.
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", DeprecationWarning)
            getattr(obj, name)(*args)
    except REFUSALS:
        pass
