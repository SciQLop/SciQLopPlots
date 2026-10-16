"""One-column lines are SciQLopLineGraphs since #113. The old class names stay as aliases, so
code that checks isinstance(graph, SciQLopSingleLineGraph) keeps working."""
import numpy as np
import SciQLopPlots
from SciQLopPlots import SciQLopLineGraph, SciQLopLineGraphFunction


def test_the_old_names_are_the_line_graph_classes():
    assert SciQLopPlots.SciQLopSingleLineGraph is SciQLopLineGraph
    assert SciQLopPlots.SciQLopSingleLineGraphFunction is SciQLopLineGraphFunction


def test_a_one_column_line_is_both(plot):
    graph = plot.plot(np.arange(10.0), np.arange(10.0))
    assert isinstance(graph, SciQLopPlots.SciQLopSingleLineGraph)
    assert isinstance(graph, SciQLopLineGraph)
