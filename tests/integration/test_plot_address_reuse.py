"""A plot made where a destroyed plot was must survive the container's clean-up of that
plot. The container queued remove_plot(old address) on destroyed; a new plot at the same
address was then removed and deleted. The browser library hit it at once (WebAssembly's
allocator reuses addresses eagerly): a page's new graph vanished under it."""
import pytest
import shiboken6
from PySide6.QtCore import QCoreApplication, QEvent
from SciQLopPlots import PlotType


def _plot_at_a_destroyed_plots_address(panel):
    for _ in range(50):
        old = panel.create_plot(-1, PlotType.TimeSeries)
        address = shiboken6.getCppPointer(old)[0]
        panel.remove_plot(old)
        # Only the deferred delete: old is destroyed, the clean-up it queues has not run yet.
        QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
        new = panel.create_plot(-1, PlotType.TimeSeries)
        if shiboken6.getCppPointer(new)[0] == address:
            return new
    pytest.skip("the allocator never gave a new plot the old address")


def test_new_plot_at_a_destroyed_plots_address_survives(panel, qtbot):
    new = _plot_at_a_destroyed_plots_address(panel)
    count = len(panel.plots())
    qtbot.wait(50)
    assert shiboken6.isValid(new)
    assert len(panel.plots()) == count
