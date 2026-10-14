"""A projection plot's panes show chosen dimension pairs. The default stays cyclic
(XY, YZ, ZX); SciQLop's scene plots want the magnetospheric XY, XZ, YZ, with X
horizontal in both X panes."""
import numpy as np
import pytest

from SciQLopPlots import SciQLopNDProjectionPlot
from conftest import process_events

MAGNETOSPHERIC = [(0, 1), (0, 2), (1, 2)]
N = 200
t = np.linspace(0, 1, N)
# Each dimension in its own decade, so a pane's ranges say which pair it shows.
DIMS = [t, 10 + t, 100 + t]


@pytest.fixture
def proj(qtbot):
    p = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(p)
    return p


def _pane_dims(proj, i):
    pane = proj.subplot(i)
    pane.rescale_axes()
    return [int(np.log10(max(ax.range().center(), 1))) for ax in (pane.x_axis(), pane.y_axis())]


def _plot(proj):
    g = proj.parametric_curve([t, *DIMS])
    proj.window().resize(900, 300)
    for _ in range(50):
        process_events()
        if not g.busy():
            break
    process_events()
    return g


def test_default_pairs_are_cyclic(proj):
    assert proj.dimension_pairs() == [(0, 1), (1, 2), (2, 0)]


def test_default_panes_show_the_cyclic_pairs(proj):
    _plot(proj)
    assert [_pane_dims(proj, i) for i in range(3)] == [[0, 1], [1, 2], [2, 0]]


def test_chosen_pairs_put_the_dimensions_on_the_panes(proj):
    proj.set_dimension_pairs(MAGNETOSPHERIC)
    _plot(proj)
    assert proj.dimension_pairs() == MAGNETOSPHERIC
    assert [_pane_dims(proj, i) for i in range(3)] == [[0, 1], [0, 2], [1, 2]]


def test_chosen_pairs_label_the_axes(proj):
    proj.set_dimension_pairs(MAGNETOSPHERIC)
    proj.set_axis_labels(["X", "Y", "Z"])
    labels = [(proj.subplot(i).x_axis().label(), proj.subplot(i).y_axis().label()) for i in range(3)]
    assert labels == [("X", "Y"), ("X", "Z"), ("Y", "Z")]


def test_axes_showing_the_same_dimension_move_together(proj):
    proj.set_dimension_pairs(MAGNETOSPHERIC)
    proj.subplot(0).x_axis().set_range(3, 4)       # X
    proj.subplot(2).y_axis().set_range(5, 6)       # Z
    x1, z1 = proj.subplot(1).x_axis().range(), proj.subplot(1).y_axis().range()
    assert (x1.start(), x1.stop()) == (3, 4)
    assert (z1.start(), z1.stop()) == (5, 6)
    y2 = proj.subplot(2).x_axis().range()
    assert (y2.start(), y2.stop()) != (3, 4)       # the old cyclic link x2 <-> y1 is gone


def test_reference_curve_follows_the_pairs(proj, qtbot):
    proj.set_dimension_pairs(MAGNETOSPHERIC)
    proj.add_reference_curve(DIMS)
    # The curves get their data from their resampler threads, some events later.
    qtbot.waitUntil(lambda: [_pane_dims(proj, i) for i in range(3)] == [[0, 1], [0, 2], [1, 2]],
                    timeout=2000)


@pytest.mark.parametrize("pairs", [[(0, 1), (0, 2)], [(0, 1), (0, 0), (1, 2)], [(0, 1), (0, 3), (1, 2)]],
                         ids=["too-few", "same-dimension", "out-of-range"])
def test_invalid_pairs_raise_and_change_nothing(proj, pairs):
    with pytest.raises(ValueError):
        proj.set_dimension_pairs(pairs)
    assert proj.dimension_pairs() == [(0, 1), (1, 2), (2, 0)]


def test_linked_by_default_and_unlinking_works(proj):
    assert proj.linked_axes()
    proj.set_linked_axes(False)
    proj.subplot(0).y_axis().set_range(3, 4)
    x1 = proj.subplot(1).x_axis().range()
    assert (x1.start(), x1.stop()) != (3, 4)
