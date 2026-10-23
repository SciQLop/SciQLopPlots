"""Plot groups: visible is an aggregate of the members (rule 2)."""
import numpy as np
import pytest

from conftest import process_events, record

X = np.linspace(0.0, 10.0, 50)


@pytest.fixture
def pair(plot):
    a = plot.plot(X, np.sin(X), labels=["a"])
    b = plot.plot(X, np.cos(X), labels=["b"])
    return plot.group("ab", [a, b]), a, b


def test_hiding_the_last_visible_member_hides_the_group_once(pair):
    group, a, b = pair
    seen = record(group.visible_changed)
    a.set_visible(False)
    assert seen == []  # b still shows
    b.set_visible(False)
    assert (group.visible(), seen) == (False, [False])


def test_showing_a_group_with_one_hidden_member_shows_it_silently(pair):
    group, a, _ = pair
    a.set_visible(False)
    seen = record(group.visible_changed)
    group.set_visible(True)
    assert (a.visible(), seen) == (True, [])


def test_the_group_name_is_its_object_name(pair):
    group, _, _ = pair
    seen = record(group.name_changed)
    group.set_name("model")
    assert (group.objectName(), group.name(), seen) == ("model", "model", ["model"])


def test_a_slot_removing_the_group_while_it_hides_is_safe(plot, pair):
    group, a, _ = pair
    a.visible_changed.connect(lambda _: plot.remove_group(group))
    group.set_visible(False)  # the group is deleted while it applies; nothing reads it after
    process_events()
    assert plot.groups() == []


def test_a_slot_deleting_a_member_while_the_group_hides_is_safe(plot):
    lines = [plot.plot(X, np.sin(X + i), labels=[str(i)]) for i in range(3)]
    group = plot.group("g", lines)
    seen = record(group.visible_changed)
    lines[0].visible_changed.connect(lambda _: plot.remove_plottable(lines[2]))
    group.set_visible(False)
    process_events()
    assert (len(group.members()), group.visible(), seen) == (2, False, [False])


def test_showing_an_all_hidden_group_signals_after_every_member_shows(pair):
    group, a, b = pair
    group.set_visible(False)
    seen = []
    group.visible_changed.connect(lambda v: seen.append((v, [a.visible(), b.visible()])))
    group.set_visible(True)
    assert seen == [(True, [True, True])]
