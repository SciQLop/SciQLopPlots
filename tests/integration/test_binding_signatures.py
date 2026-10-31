"""Every binding signature parses cleanly, and the defaults behind them stay the same.

Shiboken's signature parser warns (RuntimeWarning "pyside_type_init ... UNRECOGNIZED") on a
default or type it cannot spell in Python. Each signature is parsed once per process and then
cached, so the scan runs in a fresh interpreter: an earlier test may already have parsed them.
"""
import inspect
import json
import subprocess
import tempfile
import sys
import textwrap

import numpy as np
import pytest
from PySide6.QtGui import QColor

from SciQLopPlots import (MultiPlotsVerticalLine, MultiPlotsVerticalSpan, MultiPlotsVSpanCollection,
                          SciQLopHorizontalSpan, SciQLopMultiPlotPanel, SciQLopPlotRange,
                          SciQLopRectangularSpan, SciQLopVerticalSpan)

_SCAN = textwrap.dedent("""
    import json
    import warnings
    from SciQLopPlots import SciQLopPlotsBindings as bindings
    from shibokensupport.signature import get_signature

    def scan(obj, depth=0):
        get_signature(obj)
        if isinstance(obj, type) and depth < 2:
            for member in list(vars(obj).values()):
                scan(member, depth + 1)

    muted = [f for f in warnings.filters if f[0] == "ignore" and f[1] and f[1].match("pyside_type_init")]
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        for name in dir(bindings):
            scan(getattr(bindings, name))
    print(json.dumps({"muted": len(muted), "warnings": [str(w.message) for w in caught]}))
""")


@pytest.fixture(scope="module")
def scan():
        # Neutral cwd: `python -c` puts the cwd on sys.path, and from the repo root the
    # source package (no compiled bindings) would shadow the built one.
    out = subprocess.run([sys.executable, "-c", _SCAN], capture_output=True, text=True, check=True,
                         cwd=tempfile.gettempdir())
    return json.loads(out.stdout.strip().splitlines()[-1])


def test_no_signature_warnings(scan):
    assert scan["warnings"] == []


def test_import_does_not_mute_signature_warnings(scan):
    assert scan["muted"] == 0


# A type_map entry that stops matching shiboken's spelling goes silent, it does not warn.
@pytest.mark.parametrize("func, expected", [
    (lambda: MultiPlotsVSpanCollection.spans, "List[SciQLopPlotsBindings.MultiPlotsVerticalSpanPtr]"),
    (lambda: SciQLopMultiPlotPanel.plots, "List[SciQLopPlotsBindings.SciQLopPlotInterfacePtr]"),
    (lambda: inspect.unwrap(MultiPlotsVerticalSpan.__init__), "*, range: SciQLopPlotsBindings.SciQLopPlotRange"),
])
def test_type_map_entries_resolve(func, expected):
    from shibokensupport.signature import get_signature
    assert expected in str(get_signature(func()))


X = np.linspace(0.0, 1.0, 10)
Z = np.zeros((10, 10))


def _xyz(start, stop):
    return X, X, Z


def _xy(start, stop):
    return X, X


def _values(start, stop):
    return X


_DEFAULT_NAMES = [
    ("colormap", lambda host: host.colormap(X, X, Z), "ColorMap"),
    ("colormap callback", lambda host: host.colormap(_xyz), "ColorMap"),
    ("histogram2d", lambda host: host.histogram2d(X, X), "Histogram2D"),
    ("histogram2d callback", lambda host: host.histogram2d(_xy), "Histogram2D"),
    ("histogram", lambda host: host.histogram(X), "Histogram"),
    ("histogram callback", lambda host: host.histogram(_values), "Histogram"),
    ("bars", lambda host: host.bars(X, X[:-1]), "Bars"),
]


def _graph(result):
    return result[1] if isinstance(result, tuple) else result


@pytest.mark.parametrize("host_name", ["plot", "panel"])
@pytest.mark.parametrize("label,make,expected", _DEFAULT_NAMES, ids=[d[0] for d in _DEFAULT_NAMES])
def test_default_graph_names(request, host_name, label, make, expected):
    host = request.getfixturevalue(host_name)
    assert _graph(make(host)).name() == expected


def test_time_series_colormap_default_name(ts_plot):
    assert _graph(ts_plot.colormap(X, X, Z)).name() == "ColorMap"


_RANGE = SciQLopPlotRange(0.0, 1.0)
_SPAN_GREY = QColor(100, 100, 100, 100)
_LINE_GREY = QColor(100, 100, 100)


@pytest.mark.parametrize("make", [
    lambda plot: plot.vertical_span(_RANGE),
    lambda plot: plot.horizontal_span(_RANGE),
    lambda plot: plot.span(key_range=_RANGE, value_range=_RANGE),
], ids=["vertical", "horizontal", "rectangular"])
def test_default_span_color(plot, make):
    assert make(plot).color() == _SPAN_GREY


def test_default_deprecated_rectangular_span_color(plot):
    with pytest.warns(DeprecationWarning):
        span = plot.rectangular_span(_RANGE, _RANGE)
    assert span.color() == _SPAN_GREY


@pytest.mark.parametrize("make, expected", [
    (lambda panel: panel.vertical_span(_RANGE).color(), _SPAN_GREY),
    (lambda panel: panel.vertical_line(0.5).color(), QColor(0, 0, 0)),
], ids=["span", "line"])
def test_default_panel_item_color(panel, make, expected):
    assert make(panel) == expected


def test_default_collection_span_color(panel):
    collection = MultiPlotsVSpanCollection(panel)
    assert collection.create_span(_RANGE).color == _LINE_GREY
