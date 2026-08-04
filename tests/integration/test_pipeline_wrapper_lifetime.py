"""Regression: pipelines built in the fluent style must not die silently.

ObservableProperty held a weakref to the shiboken wrapper. For temporaries
like ``plot.x_axis().on.range >> sink`` the wrapper's last strong reference
drops as soon as the expression completes; the Qt signal connection stays
attached (it lives on the C++ side), but the dispatch closure then found a
dead weakref and silently swallowed the resulting RuntimeError — the pipeline
looked connected yet never fired.
"""
import gc

import pytest

from SciQLopPlots import SciQLopPlotRange
from conftest import process_events


class TestPipelineWrapperLifetime:

    def test_fluent_pipeline_survives_wrapper_gc(self, qtbot, plot):
        """plot.x_axis().on.range >> sink keeps working after the temporary
        axis wrapper is garbage collected."""
        received = []
        plot.x_axis().on.range >> (lambda event: received.append(event.value))
        gc.collect()
        plot.x_axis().set_range(SciQLopPlotRange(1.0, 2.0))
        process_events()
        gc.collect()
        plot.x_axis().set_range(SciQLopPlotRange(3.0, 4.0))
        process_events()
        stops = [r.stop() for r in received]
        assert 4.0 in stops

    def test_pipeline_to_axis_survives_wrapper_gc(self, qtbot, plot):
        """The fluent two-axis sync keeps working without holding the wrappers."""
        plot.x_axis().on.range >> plot.y_axis().on.range
        gc.collect()
        plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
        process_events()
        got = plot.y_axis().range()
        assert abs(got.start() - 10.0) < 1e-9
        assert abs(got.stop() - 20.0) < 1e-9
