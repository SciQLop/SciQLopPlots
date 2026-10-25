"""API review 2.9: timeline state and histogram2d normalization are enums; the old values warn."""
import pytest
from PySide6.QtWidgets import QApplication

from SciQLopPlots import (Histogram2DNormalization, HistogramNormalization, SciQLopPlot, TimelineEditMode, TimelineStack,
                          TimelineStyle)


@pytest.fixture
def tl(ts_plot):
    return ts_plot.add_timeline()


@pytest.fixture
def hist(qtbot):
    QApplication.instance() or QApplication([])
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    yield plot.add_histogram2d("h")
    del plot


def test_new_enums_are_int_enums():
    assert [m.name for m in TimelineStyle] == ["Wave", "Bars"]
    assert [m.name for m in TimelineStack] == ["Off", "Time", "Category"]
    assert [m.name for m in TimelineEditMode] == ["Move", "Resize", "ChangeLane", "Create", "Delete"]


@pytest.mark.parametrize("member", list(TimelineStyle))
def test_style_round_trip(tl, member):
    tl.style = member
    assert tl.style is member


@pytest.mark.parametrize("member", list(TimelineStack))
def test_stack_round_trip(tl, member):
    tl.stack = member
    assert tl.stack is member


def test_stack_reads_off_by_default(tl):
    assert tl.stack is TimelineStack.Off


def test_edit_modes_round_trip_as_a_set_of_members(tl):
    tl.edit_modes = set(TimelineEditMode)
    assert tl.edit_modes == set(TimelineEditMode)
    tl.edit_modes = {TimelineEditMode.Create}
    assert tl.edit_modes == {TimelineEditMode.Create}


def test_edit_modes_default_is_move_and_resize(tl):
    assert tl.edit_modes == {TimelineEditMode.Move, TimelineEditMode.Resize}


@pytest.mark.parametrize("prop, old, new", [
    ("style", "bars", TimelineStyle.Bars),
    ("stack", "time", TimelineStack.Time),
    ("stack", "category", TimelineStack.Category),
    ("stack", None, TimelineStack.Off),
])
def test_old_values_warn_and_map(tl, prop, old, new):
    with pytest.warns(DeprecationWarning, match=rf"{prop}={old!r} is deprecated: use {type(new).__name__}\.{new.name}"):
        setattr(tl, prop, old)
    assert getattr(tl, prop) is new


def test_old_edit_mode_names_warn_and_map(tl):
    with pytest.warns(DeprecationWarning, match=r"is deprecated: use TimelineEditMode\.ChangeLane"):
        tl.edit_modes = {"change_lane"}
    assert tl.edit_modes == {TimelineEditMode.ChangeLane}


@pytest.mark.parametrize("prop, value", [("style", "gantt"), ("stack", "merge"), ("edit_modes", {"teleport"})])
def test_unknown_values_raise(tl, prop, value):
    with pytest.raises(ValueError):
        setattr(tl, prop, value)


def test_histogram2d_normalization_is_an_enum(hist):
    assert hist.normalization() is Histogram2DNormalization.Count
    hist.set_normalization(Histogram2DNormalization.PerColumn)
    assert hist.normalization() is Histogram2DNormalization.PerColumn
    assert hist.normalization() == 1


@pytest.mark.parametrize("old, new", [(0, Histogram2DNormalization.Count), (1, Histogram2DNormalization.PerColumn)])
def test_histogram2d_old_ints_warn_and_map(hist, old, new):
    with pytest.warns(DeprecationWarning, match=rf"set_normalization\({old}\) is deprecated: use Histogram2DNormalization\.{new.name}"):
        hist.set_normalization(old)
    assert hist.normalization() is new


@pytest.mark.parametrize("old, name", [(HistogramNormalization.Count, "Count"), (HistogramNormalization.Density, "Density")])
def test_histogram2d_old_enum_members_are_named_in_the_warning(hist, old, name):
    with pytest.warns(DeprecationWarning, match=rf"set_normalization\(HistogramNormalization\.{name}\) is deprecated"):
        hist.set_normalization(old)


@pytest.mark.parametrize("value", [99, 2, -1, True, [1], "1", None])
def test_histogram2d_unknown_normalization_raises_value_error(hist, value):
    with pytest.raises(ValueError, match="Histogram2DNormalization"):
        hist.set_normalization(value)


@pytest.mark.parametrize("prop", ["style", "stack"])
def test_unhashable_timeline_values_raise_value_error(tl, prop):
    with pytest.raises(ValueError):
        setattr(tl, prop, [])


def test_histogram2d_signal_still_carries_an_int(hist):
    seen = []
    hist.normalization_changed.connect(seen.append)
    hist.set_normalization(Histogram2DNormalization.PerColumn)
    assert seen == [1]


def test_pyproject_filter_turns_the_new_messages_into_errors(tl, hist):
    """Pins the filterwarnings entry in pyproject.toml: no pytest.warns wrapper here."""
    with pytest.raises(DeprecationWarning, match=r"is deprecated: use TimelineStyle\.Bars"):
        tl.style = "bars"
    with pytest.raises(DeprecationWarning, match=r"use Histogram2DNormalization\.PerColumn"):
        hist.set_normalization(1)
