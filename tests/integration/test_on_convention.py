"""`.on.x` is derived from the x / set_x / x_changed convention (API review 5.1)."""
import numpy as np
import pytest
from PySide6.QtCore import QPointF

import SciQLopPlots as sp
import SciQLopPlots.properties


def _read(obj, name):
    attr = getattr(obj, name)
    return attr() if callable(attr) else attr


@pytest.fixture
def waterfall(plot):
    wf = plot.add_waterfall("w", labels=["c0", "c1", "c2"])
    x = np.linspace(0, 1, 20)
    wf.set_data(x, np.column_stack([x, 2 * x, 3 * x]))
    return wf


@pytest.fixture
def line_graph(plot, sample_data):
    return plot.line(*sample_data, name="a")


def _witness(request, name):
    return request.getfixturevalue(name)


WITNESSES = [
    ("plot", "title", "hello"),
    ("plot", "auto_scale", False),
    ("x_axis", "log", True),
    ("x_axis", "label", "lbl"),
    ("line_graph", "visible", False),
    ("line_graph", "name", "renamed"),
    ("vline", "position", 3.0),
    ("legend", "position", QPointF(0.3, 0.3)),
    ("waterfall", "gain", 2.5),
]


@pytest.fixture
def x_axis(plot):
    return plot.x_axis()


@pytest.fixture
def vline(plot):
    return plot.vertical_line(1.0)


@pytest.fixture
def legend(plot, line_graph):
    return plot.legend()


ALL_WITNESSES = ["plot", "panel", "x_axis", "line_graph", "vline", "legend", "waterfall", "span"]


def _convention_names(obj):
    names = set(dir(obj))
    return sorted(n for n in names
                  if not n.startswith("_") and (f"{n}_changed" in names or f"set_{n}" in names))


@pytest.mark.parametrize("fixture", ALL_WITNESSES)
def test_every_resolved_name_reads_without_type_error(request, fixture):
    obj = _witness(request, fixture)
    resolved = 0
    for name in _convention_names(obj):
        try:
            prop = getattr(obj.on, name)
        except AttributeError:
            continue
        resolved += 1
        try:
            prop.value
        except NotImplementedError:
            pass
    assert resolved


@pytest.mark.parametrize("fixture,name", [
    ("plot", "axis_synchronized"), ("panel", "plot_stretch"), ("line_graph", "column_in_legend"),
    ("waterfall", "column_in_legend"),
])
def test_getter_needing_an_argument_is_not_exposed(request, fixture, name):
    with pytest.raises(AttributeError):
        getattr(_witness(request, fixture).on, name)


@pytest.mark.parametrize("fixture,name,value", WITNESSES, ids=[f"{f}.{n}" for f, n, _ in WITNESSES])
def test_witness_read_write_signal(request, fixture, name, value):
    obj = _witness(request, fixture)
    prop = getattr(obj.on, name)
    assert prop.value == _read(obj, name)
    seen = []
    pipeline = prop >> (lambda event: seen.append(event.value))
    prop.value = value
    assert _read(obj, name) == value
    assert prop.value == value
    assert seen, "signal did not fire"
    pipeline.disconnect()


def test_exception_data(line_graph):
    prop = line_graph.on.data
    assert prop.property_type == "data"
    x = np.arange(4.0)
    prop.value = (x, x * 2)
    assert list(line_graph.data()[1]) == list(x * 2)


def test_exception_range_axis_splats(plot):
    plot.x_axis().on.range.value = (1.0, 5.0)
    r = plot.x_axis().range()
    assert (r.start(), r.stop()) == (1.0, 5.0)


def test_exception_selected(line_graph):
    prop = line_graph.on.selected
    prop.value = True
    assert _read(line_graph, "selected") is True
    assert prop.signal is not None


def test_exception_spacing_maps_to_uniform_spacing(waterfall):
    waterfall.on.spacing.value = 0.25
    assert waterfall.uniform_spacing() == 0.25
    assert waterfall.on.spacing.signal is not None


@pytest.fixture
def span(plot):
    return plot.vertical_span(sp.SciQLopPlotRange(0.0, 1.0))


@pytest.fixture
def old_span(plot):
    with pytest.warns(DeprecationWarning):
        return sp.SciQLopVerticalSpan(plot, sp.SciQLopPlotRange(0.0, 1.0))


def test_exception_tooltip_has_a_signal(span):
    span.on.tooltip.value = "tip"
    assert span.on.tooltip.value == span.tool_tip() == "tip"
    assert span.on.tooltip.signal is not None


def test_exception_tooltip_has_no_signal_on_the_old_span(old_span):
    old_span.on.tooltip.value = "tip"
    assert old_span.on.tooltip.value == old_span.tool_tip() == "tip"
    assert old_span.on.tooltip.signal is None


def test_span_range_through_convention(span):
    seen = []
    pipeline = span.on.range >> (lambda event: seen.append(event.value))
    span.on.range.value = sp.SciQLopPlotRange(2.0, 3.0)
    assert span.range().start() == 2.0
    assert seen
    pipeline.disconnect()


DELETED_REGISTRATIONS = [
    ("waterfall", "offset_mode", "offset_mode_changed", "offset_mode", "set_offset_mode"),
    ("waterfall", "offsets", "offsets_changed", "offsets", "set_offsets"),
    ("waterfall", "normalize", "normalize_changed", "normalize", "set_normalize"),
    ("waterfall", "gain", "gain_changed", "gain", "set_gain"),
    ("span", "range", "range_changed", "range", "set_range"),
]


@pytest.mark.parametrize("fixture,name,signal,getter,setter", DELETED_REGISTRATIONS,
                         ids=[f"{f}.{n}" for f, n, *_ in DELETED_REGISTRATIONS])
def test_convention_resolves_like_the_old_registration(request, fixture, name, signal, getter, setter):
    spec = sp.properties._convention_spec(_witness(request, fixture), name)
    assert (spec["signal_name"], spec["getter_name"], spec["setter_name"]) == (signal, getter, setter)
    assert (spec["splat"], spec["signal_args"]) == (False, None)


def test_tooltip_cannot_start_a_pipeline_on_the_old_span(old_span):
    with pytest.raises(ValueError, match="no signal"):
        old_span.on.tooltip >> (lambda event: None)


@pytest.mark.parametrize("name", ["no_such_property", "_private"])
def test_unknown_or_private_name_raises(plot, name):
    with pytest.raises(AttributeError):
        getattr(plot.on, name)


def test_error_names_the_class(plot):
    with pytest.raises(AttributeError, match="SciQLopPlot"):
        plot.on.no_such_property


def test_name_without_getter_is_not_exposed(line_graph):
    assert not hasattr(line_graph, "selection")
    with pytest.raises(AttributeError):
        line_graph.on.selection
