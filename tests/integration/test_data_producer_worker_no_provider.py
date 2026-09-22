"""``DataProviderWorker::m_data_provider`` had no initialiser in
include/SciQLopPlots/DataProducer/DataProducer.hpp — it was only ever set by
``set_data_provider()``. DataProviderWorker is exposed to Python
(``parent-management="yes"``, default-constructible), so a script that builds
one directly and calls a slot before ``set_data_provider`` dereferences
garbage memory. Observed on this machine as a hang (a QMutexLocker spinning
on a corrupted QMutex inside the garbage-pointed DataProviderInterface), not
a clean segfault — run in a subprocess with a timeout so an unfixed build
fails the test instead of hanging the whole suite.
"""
import os
import subprocess
import sys
import tempfile
import textwrap


def _run_script(script, timeout=15):
    return subprocess.run(
        [sys.executable, "-c", script],
        env=dict(os.environ),
        capture_output=True,
        text=True,
        timeout=timeout,
        cwd=tempfile.gettempdir(),
    )


_SCRIPT = textwrap.dedent(
    """
    import sys
    from PySide6.QtWidgets import QApplication
    from SciQLopPlots import DataProviderWorker, SciQLopPlotRange
    import numpy as np

    app = QApplication(sys.argv)
    worker = DataProviderWorker()
    worker.set_range(SciQLopPlotRange(0.0, 1.0))
    worker.set_data(np.array([1.0]), np.array([2.0]))
    print("OK")
    """
)


class TestDataProviderWorkerWithoutProvider:
    def test_set_range_and_set_data_without_a_provider_do_not_hang_or_crash(self):
        try:
            result = _run_script(_SCRIPT)
        except subprocess.TimeoutExpired:
            assert False, "DataProviderWorker.set_range/set_data hung with no provider set"
        assert result.returncode == 0, (
            f"crashed (rc={result.returncode}): "
            f"stderr={result.stderr[-800:]}"
        )
        assert "OK" in result.stdout, result.stdout
