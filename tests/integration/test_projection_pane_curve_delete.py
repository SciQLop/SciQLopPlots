"""Deleting one pane's curve of a projection must not crash the colour-scale controller."""
import numpy as np
import shiboken6

from conftest import process_events

X = np.linspace(0.0, 10.0, 50)


def test_deleting_the_last_panes_curve_leaves_a_working_projection(qtbot):
    from SciQLopPlots import SciQLopNDProjectionPlot
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    nd.show()  # a shown plot has live colour-scale controllers
    projection = nd.plot([X, X, np.sin(X), np.cos(X)])
    qtbot.waitUntil(lambda: len(projection.components()) == 3, timeout=3000)
    qtbot.wait(300)
    shiboken6.delete(nd.subplot(2).plottables()[0])
    process_events()
    assert len(projection.components()) == 2
