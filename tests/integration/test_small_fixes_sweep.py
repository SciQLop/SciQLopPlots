"""Reproducers for the small-fix sweep (docs/backlog-2026-06-10.md M5 + M6).

M5: ``tracing.enable()`` silently ignored an unopenable output file — no
warning, no return value; ``is_enabled()`` was the only way to notice.

M6: ``SciQLopHistogram2D.set_bins(0, ...)`` was a silent no-op (NeoQCP's
setBins returns early on non-positive bins) and ``set_normalization`` cast any
int to the enum without range checking.
"""
import os

import numpy as np
import pytest

from SciQLopPlots import GraphType, tracing


@pytest.fixture
def hist(plot):
    x = np.random.rand(1000)
    y = np.random.rand(1000)
    return plot.plot(x, y, graph_type=GraphType.Histogram2D)


class TestTracingEnableFeedback:
    def test_enable_unopenable_path_reports_failure(self):
        assert tracing.enable("/nonexistent_dir_xyz/trace.json") is False
        assert not tracing.is_enabled()

    def test_enable_valid_path_reports_success(self, tmp_path):
        path = str(tmp_path / "trace.json")
        try:
            assert tracing.enable(path) is True
            assert tracing.is_enabled()
        finally:
            tracing.disable()
        assert os.path.exists(path)


class TestHistogramBinValidation:
    def test_zero_bins_raises(self, hist):
        with pytest.raises((ValueError, RuntimeError)):
            hist.set_bins(0, 100)

    def test_negative_bins_raises(self, hist):
        with pytest.raises((ValueError, RuntimeError)):
            hist.set_bins(10, -5)

    def test_valid_bins_still_work(self, hist):
        hist.set_bins(64, 64)
        assert hist.x_bins() == 64
        assert hist.y_bins() == 64

    def test_out_of_range_normalization_raises(self, hist):
        with pytest.raises((ValueError, RuntimeError)):
            hist.set_normalization(99)

    def test_valid_normalization_still_works(self, hist):
        hist.set_normalization(0)
        assert hist.normalization() == 0
