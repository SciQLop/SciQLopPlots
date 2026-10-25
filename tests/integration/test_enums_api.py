"""API review 2.9: timeline state and histogram2d normalization are enums; the old values warn."""
import pytest

from SciQLopPlots import TimelineEditMode, TimelineStack, TimelineStyle


@pytest.fixture
def tl(ts_plot):
    return ts_plot.add_timeline()


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
