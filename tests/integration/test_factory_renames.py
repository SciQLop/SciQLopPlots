"""API review 1.7 / 1.16 / 2.8: x(data or callback) builds, add_x(...) makes an empty one; old names warn."""
import warnings

import numpy as np
import pytest

from SciQLopPlots import SciQLopNDProjectionPlot

T = np.linspace(0.0, 10.0, 50)
CURVE = [T, np.cos(T), np.sin(T), T]


@pytest.fixture
def proj(qtbot):
    p = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(p)
    return p


def _silently(call):
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        return call()


def test_empty_factories_do_not_warn(plot):
    _silently(lambda: plot.add_waterfall("w", labels=["a"]))
    _silently(lambda: plot.add_histogram2d("h", 20, 20))


def test_projection_reference_and_model_curves(proj):
    assert _silently(lambda: proj.reference_curve(CURVE, label="orbit")) is not None
    assert _silently(lambda: proj.model_curve(lambda a, b: CURVE, label="model")) is not None


def test_auto_contour_levels_getter(plot):
    cmap = plot.colormap(T, T, np.outer(T, T))
    cmap.set_auto_contour_levels(3)
    assert _silently(cmap.auto_contour_levels) == 3


OLD = [
    ("add_reference_curve", lambda h: h.proj.add_reference_curve(CURVE), r"use projection\.reference_curve\("),
    ("add_model_curve", lambda h: h.proj.add_model_curve(lambda a, b: CURVE), r"use projection\.model_curve\("),
    ("add_waterfall", lambda h: h.plot.add_waterfall(lambda a, b: [T, np.sin(T)], "w"), r"use plot\.waterfall\("),
    ("enable_cursor", lambda h: h.plot.enable_cursor(False) or not h.plot.crosshair_enabled(), r"use plot\.set_crosshair_enabled\("),
    ("auto_contour_level_count", lambda h: h.plot.colormap(T, T, np.outer(T, T)).auto_contour_level_count(),
     r"use colormap\.auto_contour_levels\("),
]


class Hosts:
    def __init__(self, plot, proj):
        self.plot, self.proj = plot, proj


@pytest.mark.parametrize("call, hint", [o[1:] for o in OLD], ids=[o[0] for o in OLD])
def test_the_old_name_warns_at_the_caller_and_still_works(plot, proj, call, hint):
    with pytest.warns(DeprecationWarning, match=hint) as record:
        result = call(Hosts(plot, proj))
    assert record[0].filename == __file__
    assert result is not None
