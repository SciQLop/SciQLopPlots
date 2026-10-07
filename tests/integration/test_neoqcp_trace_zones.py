"""NeoQCP's own profiling zones (draw, resampling) reach the SciQLopPlots tracer.

NeoQCP's PROFILE_* macros used to map to Tracy or nothing, so a trace taken from
SciQLop held only SciQLopPlots zones: a freeze inside a graph's draw was invisible.
"""
import json

import numpy as np

from SciQLopPlots import tracing


def _zone_names(trace_path):
    with open(trace_path) as f:
        return {e.get("name") for e in json.load(f)["traceEvents"]}


def test_neoqcp_draw_and_resample_zones_are_traced(plot, qtbot, tmp_path):
    x = np.linspace(0.0, 1000.0, 300_000)
    trace_path = str(tmp_path / "trace.json")
    assert tracing.enable(trace_path) is True
    try:
        plot.plot(x, np.sin(x))
        plot.show()
        qtbot.wait(500)  # async L1 build, then the draws it queues
        plot.replot(True)
        qtbot.wait(100)
    finally:
        tracing.flush()
        tracing.disable()

    names = _zone_names(trace_path)
    assert "buildL1CacheMulti" in names
    assert "QCPMultiGraph::draw" in names
