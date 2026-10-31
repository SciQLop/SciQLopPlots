"""A str given where a list of strings is expected is one item, not one item per letter."""
import numpy as np
from PySide6.QtCore import QStringListModel

X = np.arange(10, dtype=float)


def test_a_str_is_one_lane(ts_plot):
    tl = ts_plot.add_timeline()
    tl.set_lanes("MSA")
    assert tl.lanes() == ["MSA"]


def test_a_str_is_one_label(plot):
    graph = plot.plot(X, X, labels="Bx")
    assert graph.labels() == ["Bx"]


def test_sequences_still_work(ts_plot):
    tl = ts_plot.add_timeline()
    tl.set_lanes(("MSA", "MGF"))
    assert tl.lanes() == ["MSA", "MGF"]
    tl.set_lanes([])
    assert tl.lanes() == []


def test_pyside_itself_is_left_alone():
    model = QStringListModel()
    model.setStringList("ab")
    assert model.stringList() == ["a", "b"]
