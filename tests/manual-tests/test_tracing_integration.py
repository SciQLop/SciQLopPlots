"""End-to-end integration test: real plot + tracer + validation of zones.

Validates that C++-instrumented call sites (replot, setdata, resample) emit
zones into the trace file when running through a real SciQLopPlots widget.
"""
import json
import os
import sys
import tempfile

import numpy as np
from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopMultiPlotPanel, PlotType, tracing


def make_data(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.column_stack([np.cos(x / 6) * 100, np.sin(x / 6) * 100])
    return x, y


def main():
    fd, path = tempfile.mkstemp(suffix=".json", prefix="sciqlop_trace_")
    os.close(fd)

    app = QApplication.instance() or QApplication(sys.argv)
    tracing.enable(path)
    tracing.set_thread_name("gui")

    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    panel.plot(make_data, labels=["A", "B"], plot_type=PlotType.TimeSeries)
    panel.resize(800, 500)
    panel.show()

    QTimer.singleShot(800, app.quit)
    app.exec()

    tracing.disable()

    try:
        with open(path) as f:
            doc = json.load(f)
        events = doc["traceEvents"]
        names = {e.get("name") for e in events}
        phases = {e.get("ph") for e in events}

        print(f"trace file: {path}")
        print(f"event count: {len(events)}")
        print(f"phases: {sorted(phases)}")
        print(f"unique names (first 30): {sorted(names)[:30]}")

        assert "process_name" in names, "missing process metadata"
        assert "thread_name" in names, "missing thread metadata"

        replot_zones = [e for e in events if e.get("ph") == "X"
                        and (e.get("name") or "").startswith("plot.replot")]
        assert len(replot_zones) > 0, "no plot.replot zones recorded"
        print(f"plot.replot zones: {len(replot_zones)}")

        gui_tids = {e["tid"] for e in events
                    if e.get("ph") == "M" and e.get("name") == "thread_name"
                    and e.get("args", {}).get("name") == "gui"}
        assert gui_tids, "gui thread name not recorded"

        print("OK")
    finally:
        os.unlink(path)


if __name__ == "__main__":
    main()
