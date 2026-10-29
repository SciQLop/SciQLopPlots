"""API review 5.9: `name()` / `set_name()` / `name_changed` on every named object."""
import inspect

import numpy as np

import SciQLopPlots
from conftest import record

X = np.linspace(0.0, 10.0, 50)


def _named_classes():
    return [cls for _, cls in inspect.getmembers(SciQLopPlots, inspect.isclass)
            if hasattr(cls, "set_name")]


def test_name_is_a_method_on_every_class_with_set_name():
    as_values = [cls.__name__ for cls in _named_classes()
                 if not callable(inspect.getattr_static(cls, "name", None))]
    assert as_values == []


def test_a_graph_name_is_a_method(plot):
    graph = plot.plot(X, np.sin(X), labels=["a"])
    graph.set_name("speed")
    assert graph.name() == "speed"


def test_a_plot_is_renamed_with_a_signal(plot):
    seen = record(plot.name_changed)
    plot.set_name("top")
    assert (plot.name(), seen) == ("top", ["top"])


def test_a_panel_is_renamed_with_a_signal(panel):
    seen = record(panel.name_changed)
    panel.set_name("main")
    assert (panel.name(), seen) == ("main", ["main"])


def test_a_plot_name_is_observable(plot, panel):
    plot.on.name >> panel.on.name
    plot.set_name("linked")
    assert panel.name() == "linked"
