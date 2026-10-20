"""Phase 2 of the 2026-10-20 API review (docs/api-review-2026-10-20.md): footguns.
Wrong calls raise instead of doing something else quietly."""
import numpy as np
import pytest

from SciQLopPlots import (GraphType, SciQLopColorMap, SciQLopMultiPlotPanel, SciQLopNDProjectionPlot,
                          SciQLopPlot)

X = np.linspace(0, 1, 10)
Z = np.ones((10, 10))


class TestGraphTypeAndArity:  # task 8, finding 3.1
    @pytest.mark.parametrize("graph_type", [GraphType.Line, GraphType.Scatter,
                                            GraphType.ParametricCurve])
    def test_three_arrays_with_another_type_raise(self, plot, graph_type):
        with pytest.raises(ValueError, match="3"):
            plot.plot(X, X, Z, graph_type=graph_type)

    @pytest.mark.parametrize("graph_type", [None, GraphType.ColorMap])
    def test_three_arrays_make_a_colormap(self, plot, graph_type):
        assert isinstance(plot.plot(X, X, Z, graph_type=graph_type), SciQLopColorMap)

    def test_two_arrays_default_to_a_line(self, plot):
        assert type(plot.plot(X, X)).__name__ == "SciQLopLineGraph"


class TestOneColorMapPerPlot:  # found while writing task 8's tests
    def test_a_second_colormap_raises(self, plot):
        plot.plot(X, X, Z)
        with pytest.raises(RuntimeError, match="already has a colour map"):
            plot.plot(X, X, Z)

    def test_a_second_colormap_function_raises(self, plot):
        plot.plot(lambda start, stop: None, graph_type=GraphType.ColorMap)
        with pytest.raises(RuntimeError, match="already has a colour map"):
            plot.plot(lambda start, stop: None, graph_type=GraphType.ColorMap)


class TestProjectionAxes:  # task 10, finding 2.2: rejected
    def test_a_projection_has_no_panel_x_axis(self, qtbot):
        # None means "not on the panel's shared x axis": the panel's x sync and axis hiding
        # rely on it (test_projection_in_x_synced_panel). Answering a pane's axis would tie a
        # spatial axis to the panel's time range.
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        assert proj.x_axis() is None


class TestSaveRaises:  # task 13, finding 3.9
    @pytest.mark.parametrize("method", ["save", "save_png", "save_pdf", "save_jpg", "save_bmp"])
    def test_a_bad_path_raises(self, plot, method, tmp_path):
        ext = {"save": "png", "save_png": "png", "save_pdf": "pdf", "save_jpg": "jpg",
               "save_bmp": "bmp"}[method]
        with pytest.raises(OSError, match="no_such_dir"):
            getattr(plot, method)(str(tmp_path / "no_such_dir" / f"x.{ext}"))

    def test_an_unknown_format_raises(self, plot, tmp_path):
        with pytest.raises(ValueError, match="format"):
            plot.save(str(tmp_path / "x.tiff"))

    @pytest.mark.parametrize("make", ["panel", "projection"])
    def test_containers_raise_too(self, qtbot, make, tmp_path):
        target = SciQLopMultiPlotPanel() if make == "panel" else SciQLopNDProjectionPlot(3)
        qtbot.addWidget(target)
        if make == "panel":
            target.create_plot(-1)
        with pytest.raises(OSError, match="no_such_dir"):
            target.save_png(str(tmp_path / "no_such_dir" / "x.png"))
        with pytest.raises(ValueError, match="format"):
            target.save(str(tmp_path / "x.tiff"))

    def test_the_message_matches_sciqlop(self, plot, tmp_path):
        # SciQLop raises OSError("failed to save ...") itself on a False return; same words now.
        with pytest.raises(OSError, match="failed to save"):
            plot.save_png(str(tmp_path / "no_such_dir" / "x.png"))

    def test_a_good_save_still_returns_true(self, plot, tmp_path):
        assert plot.save_png(str(tmp_path / "ok.png"), 200, 100) is True


class TestDataCallbackErrors:  # task 9, finding 3.2
    @staticmethod
    def _fetch(qtbot, graph, start=0.0, stop=1.0):
        from SciQLopPlots import SciQLopPlotRange
        graph.set_range(SciQLopPlotRange(start, stop))

    def test_a_raising_callback_is_reported(self, qtbot, plot):
        def broken(start, stop):
            raise ValueError("instrument offline")
        graph = plot.plot(broken)
        failures = []
        graph.fetch_failed.connect(failures.append)
        self._fetch(qtbot, graph)
        qtbot.waitUntil(lambda: bool(failures), timeout=5000)
        assert "instrument offline" in failures[0]
        assert "ValueError" in failures[0]

    def test_bad_data_is_reported(self, qtbot, plot):
        graph = plot.plot(lambda start, stop: (np.arange(10.), np.arange(3.)))
        failures = []
        graph.fetch_failed.connect(failures.append)
        self._fetch(qtbot, graph)
        qtbot.waitUntil(lambda: bool(failures), timeout=5000)

    def test_a_failed_range_is_fetched_again(self, qtbot, plot):
        calls = []

        def flaky(start, stop):
            calls.append((start, stop))
            if len(calls) == 1:
                raise RuntimeError("timeout")
            return np.linspace(start, stop, 10), np.zeros(10)
        graph = plot.plot(flaky)
        # With a margin, a fetched range covers pans inside it: a failed one must not.
        graph.set_prefetch_margin(1.0)
        failures = []
        graph.fetch_failed.connect(failures.append)
        self._fetch(qtbot, graph, 0.0, 1.0)
        qtbot.waitUntil(lambda: bool(failures), timeout=5000)
        self._fetch(qtbot, graph, 0.1, 0.9)  # inside what the failed fetch asked for
        qtbot.waitUntil(lambda: len(calls) >= 2, timeout=5000)
