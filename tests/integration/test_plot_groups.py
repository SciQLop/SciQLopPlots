"""Plot groups and legend exclusion (docs/plans/2026-10-19-plot-groups-design.md).

SciQLop's planetary scenes draw each boundary model as a centre curve plus dashed min/max
curves, in 3 panes. Without groups their legend has one row per curve and covers the pane.
"""
import gc

import numpy as np
import pytest

from SciQLopPlots import SciQLopNDProjectionPlot, SciQLopPlot
from conftest import process_events

X = np.linspace(0, 10, 50)


@pytest.fixture
def lines(plot):
    return [plot.plot(X, np.sin(X) + k, labels=[name]) for k, name in enumerate("abc")]


@pytest.fixture
def proj(qtbot):
    p = SciQLopNDProjectionPlot(3)
    p.set_shared_legend(True)
    qtbot.addWidget(p)
    return p


def pane_curves(proj, label):
    """One curve per pane, the way SciQLop's scene adds its boundaries."""
    t = np.linspace(0, 2 * np.pi, 50)
    return [proj.subplot(i).plot(np.cos(t) + i, np.sin(t), labels=[label],
                                 graph_type=_parametric())
            for i in range(proj.subplot_count())]


def _parametric():
    from SciQLopPlots import GraphType
    return GraphType.ParametricCurve


def shown_rows(proj):
    """Legend rows a user sees: only legends that are visible count."""
    rows = []
    for i in range(proj.subplot_count()):
        legend = proj.subplot(i).legend()
        if legend.is_visible():
            rows += legend.row_labels()
    return rows


class TestPlainPlot:
    def test_a_group_has_one_legend_row(self, plot, lines):
        assert plot.legend().row_labels() == ["a", "b", "c"]
        group = plot.group("abc", lines)
        assert plot.legend().row_labels() == ["abc"]
        assert group.name() == "abc"
        assert group.members() == lines

    def test_removing_the_group_gives_rows_back(self, plot, lines):
        group = plot.group("abc", lines)
        plot.remove(group)
        assert sorted(plot.legend().row_labels()) == ["a", "b", "c"]
        assert plot.groups() == []

    def test_set_visible_hides_every_member_and_signals(self, plot, lines, qtbot):
        group = plot.group("abc", lines)
        fired = []
        for g in lines:
            g.visible_changed.connect(fired.append)
        group.set_visible(False)
        assert not any(g.visible() for g in lines)
        assert fired == [False, False, False]
        assert not group.visible()

    def test_visible_while_any_member_is(self, plot, lines):
        group = plot.group("abc", lines)
        lines[0].set_visible(False)
        assert group.visible()
        for g in lines:
            g.set_visible(False)
        assert not group.visible()
        group.set_visible(True)
        assert all(g.visible() for g in lines)

    def test_add_and_remove(self, plot, lines):
        group = plot.group("ab", lines[:2])
        group.add(lines[2])
        assert group.members() == lines
        group.remove(lines[0])
        assert group.members() == lines[1:]
        assert sorted(plot.legend().row_labels()) == ["a", "ab"]

    def test_an_empty_group_stays(self, plot, lines):
        group = plot.group("a", lines[:1])
        group.remove(lines[0])
        assert group.members() == []
        assert plot.groups() == [group]
        assert sorted(plot.legend().row_labels()) == ["a", "b", "c"]

    def test_a_removed_member_leaves_the_group(self, plot, lines):
        group = plot.group("abc", lines)
        plot.remove(lines[0])
        process_events()
        assert len(group.members()) == 2
        assert plot.legend().row_labels() == ["abc"]

    def test_the_python_handle_does_not_own_the_group(self, plot, lines):
        plot.group("abc", lines)
        gc.collect()
        process_events()
        assert len(plot.groups()) == 1
        assert plot.legend().row_labels() == ["abc"]

    def test_a_member_of_another_plot_is_refused(self, qtbot, plot, lines):
        other = SciQLopPlot()
        qtbot.addWidget(other)
        foreign = other.plot(X, X, labels=["f"])
        with pytest.raises(ValueError):
            plot.group("bad", [foreign])
        group = plot.group("ok", lines[:1])
        with pytest.raises(ValueError):
            group.add(foreign)
        assert group.members() == lines[:1]

    def test_late_components_join_the_group(self, qtbot, plot):
        # A parametric curve makes one NeoQCP curve per column, after the data is resampled.
        curve = plot.plot(X, X, graph_type=_parametric())
        qtbot.waitUntil(lambda: curve.plottable_count() == 1, timeout=5000)
        plot.group("late", [curve])
        curve.set_data(X, np.column_stack([X, 2 * X, 3 * X]))
        qtbot.waitUntil(lambda: curve.plottable_count() == 3, timeout=5000)
        assert plot.legend().row_labels() == ["late"]


class TestInLegend:
    def test_a_graph_can_leave_and_rejoin_the_legend(self, plot, lines):
        assert lines[1].in_legend()
        lines[1].set_in_legend(False)
        assert not lines[1].in_legend()
        assert plot.legend().row_labels() == ["a", "c"]
        lines[1].set_in_legend(True)
        assert sorted(plot.legend().row_labels()) == ["a", "b", "c"]

    def test_a_grouped_graph_stays_out_after_leaving(self, plot, lines):
        lines[0].set_in_legend(False)
        group = plot.group("ab", lines[:2])
        group.remove(lines[0])
        assert sorted(plot.legend().row_labels()) == ["ab", "c"]

    def test_a_projection_curve_leaves_every_pane(self, proj):
        t = np.linspace(0, 1, 20)
        traj = proj.parametric_curve([t, t, t, t], labels=["MMS1"] * 3)
        traj.set_in_legend(False)
        assert not traj.in_legend()
        for i in range(proj.subplot_count()):
            assert "MMS1" not in proj.subplot(i).legend().row_labels()


class TestProjection:
    def test_a_scene_boundary_shows_once(self, proj):
        centre, low, high = (pane_curves(proj, name) for name in ("Shue", "min", "max"))
        group = proj.group("Shue 1998", centre + low + high)
        assert shown_rows(proj) == ["Shue 1998"]
        assert len(group.members()) == 9

    def test_hiding_the_group_hides_every_pane(self, proj):
        curves = pane_curves(proj, "Shue")
        group = proj.group("Shue 1998", curves)
        group.set_visible(False)
        assert not any(c.visible() for c in curves)

    def test_a_trajectory_can_be_grouped(self, proj):
        t = np.linspace(0, 1, 20)
        traj = proj.parametric_curve([t, t, t, t], labels=["MMS1"] * 3)
        proj.group("MMS", [traj])
        assert shown_rows(proj) == ["MMS"]

    def test_a_pane_piece_of_a_trajectory_is_refused(self, proj):
        t = np.linspace(0, 1, 20)
        traj = proj.parametric_curve([t, t, t, t], labels=["MMS1"] * 3)
        piece = proj.subplot(0).plottables()[0]
        with pytest.raises(ValueError):
            proj.group("bad", [piece])

    def test_removing_the_group_gives_every_pane_its_rows(self, proj):
        curves = pane_curves(proj, "Shue")
        group = proj.group("Shue 1998", curves)
        proj.remove(group)
        for i in range(proj.subplot_count()):
            assert proj.subplot(i).legend().row_labels() == ["Shue"]


class TestFromTheReview:
    """Findings of the opencode code review of 14f5bac."""

    @staticmethod
    def _off_thread(call):
        import threading
        result = {}

        def work():
            try:
                call()
                result["error"] = None
            except Exception as e:  # noqa: BLE001 - the type is what we test
                result["error"] = e
        t = threading.Thread(target=work)
        t.start()
        t.join()
        return result["error"]

    def test_group_off_the_gui_thread_raises(self, plot, lines):
        error = self._off_thread(lambda: plot.group("x", lines))
        assert isinstance(error, RuntimeError)

    def test_add_off_the_gui_thread_raises(self, plot, lines):
        group = plot.group("x", lines[:1])
        error = self._off_thread(lambda: group.add(lines[1]))
        assert isinstance(error, RuntimeError)

    def test_bad_pane_names_off_the_gui_thread_raise(self, proj):
        error = self._off_thread(lambda: proj.set_pane_names(["XY"]))
        assert isinstance(error, ValueError)

    def test_a_timeline_row_reads_as_its_categories(self, plot):
        # A timeline's row draws one swatch and label per category, not a name.
        timeline = plot.add_timeline(0)
        timeline.set_intervals([0, 2], [1, 3], lane=["X", "X"], category=["BS", "MP"])
        process_events()
        assert plot.legend().row_labels() == ["BS\nMP"]

    def test_a_timeline_can_leave_the_legend(self, plot):
        timeline = plot.add_timeline(0)
        assert len(plot.legend().row_labels()) == 1
        timeline.set_in_legend(False)
        assert plot.legend().row_labels() == []

    def test_a_timeline_can_be_grouped(self, plot, lines):
        timeline = plot.add_timeline(0)
        plot.group("all", lines + [timeline])
        assert plot.legend().row_labels() == ["all"]

    def test_removing_the_group_from_a_visibility_slot(self, plot, lines):
        group = plot.group("abc", lines)
        lines[0].visible_changed.connect(lambda _: plot.remove(group))
        group.set_visible(False)  # must not touch the deleted group
        process_events()
        assert plot.groups() == []

    def test_groups_are_only_made_by_their_plot(self, plot):
        from SciQLopPlots import SciQLopPlotGroup
        with pytest.raises(TypeError):
            SciQLopPlotGroup("x", plot)

    def test_removing_another_plots_group_raises(self, qtbot, plot, lines):
        other = SciQLopPlot()
        qtbot.addWidget(other)
        foreign = other.group("f", [other.plot(X, X, labels=["f"])])
        with pytest.raises(ValueError):
            plot.remove(foreign)
        assert other.groups() == [foreign]


class TestDoubleClick:
    """A real double-click on the group's row, through Qt's event path."""

    @staticmethod
    def _double_click_row(qtbot, plot, row):
        from PySide6.QtCore import Qt
        from PySide6.QtTest import QTest
        plot.resize(600, 400)
        plot.show()
        qtbot.waitExposed(plot)
        plot.replot(True)
        process_events()
        rect = plot.legend().row_rect(row)
        assert rect.isValid()
        target = plot.childAt(rect.center())
        QTest.mouseDClick(target, Qt.LeftButton, Qt.NoModifier, target.mapFrom(plot, rect.center()))
        process_events()

    def test_double_click_toggles_every_member(self, qtbot, plot, lines):
        group = plot.group("abc", lines)
        self._double_click_row(qtbot, plot, 0)
        assert not any(g.visible() for g in lines)
        self._double_click_row(qtbot, plot, 0)
        assert all(g.visible() for g in lines)
        assert group.visible()

    def test_a_row_rect_is_empty_past_the_last_row(self, plot, lines):
        assert not plot.legend().row_rect(len(lines)).isValid()


class TestSecondReview:
    """Findings of the second opencode review (6d236bd..bb2770a)."""

    def test_a_visibility_slot_may_remove_a_later_member(self, plot, lines):
        group = plot.group("abc", lines)
        lines[0].visible_changed.connect(lambda _: plot.remove(lines[2]))
        group.set_visible(False)  # must not touch the deleted third line
        process_events()
        assert len(group.members()) == 2

    def test_moving_survives_a_slot_deleting_the_new_group(self, plot, lines):
        first = plot.group("first", lines[:1])
        second = plot.group("second", lines[1:2])
        first.members_changed.connect(lambda: plot.remove(second))
        second.add(lines[0])  # first tells its slot mid-move; second is deleted there
        process_events()
        assert plot.groups() == [first]
        assert first.members() == []

    def test_a_member_moves_between_groups(self, plot, lines):
        first = plot.group("first", lines[:2])
        second = plot.group("second", lines[2:])
        changed = []
        first.members_changed.connect(lambda: changed.append("first"))
        second.add(lines[0])
        assert first.members() == lines[1:2]
        assert second.members() == [lines[2], lines[0]]
        assert sorted(plot.legend().row_labels()) == ["first", "second"]
        assert changed == ["first"]

    def test_renaming_a_group_renames_its_row(self, plot, lines):
        group = plot.group("abc", lines)
        group.set_name("model")
        assert plot.legend().row_labels() == ["model"]

    def test_the_last_member_removed_leaves_an_empty_group(self, plot, lines):
        group = plot.group("a", lines[:1])
        plot.remove(lines[0])
        process_events()
        assert group.members() == []
        assert sorted(plot.legend().row_labels()) == ["b", "c"]
        group.add(lines[1])
        assert sorted(plot.legend().row_labels()) == ["a", "c"]

    def test_set_in_legend_off_the_gui_thread_is_queued(self, plot, lines):
        import threading
        t = threading.Thread(target=lambda: lines[0].set_in_legend(False))
        t.start()
        t.join()
        process_events()
        assert plot.legend().row_labels() == ["b", "c"]

    def test_good_pane_names_off_the_gui_thread_are_queued(self, proj):
        import threading
        t = threading.Thread(target=lambda: proj.set_pane_names(["XY", "YZ", "ZX"]))
        t.start()
        t.join()
        process_events()
        assert proj.subplot(1).objectName() == "YZ"
