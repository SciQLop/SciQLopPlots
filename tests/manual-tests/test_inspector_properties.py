"""Integration tests for inspector property delegates.

Each TestCase exercises one delegate. Per control:
  - initial state reflects the model
  - widget edit -> model setter fires
  - model setter -> widget reflects new value
  - no recursive emit feedback loops

Run: meson test -C build inspector_properties
"""
import os
import sys
import unittest

os.environ['QT_API'] = 'PySide6'

from PySide6.QtWidgets import QApplication, QGroupBox, QSpinBox, QDoubleSpinBox, QComboBox, QCheckBox, QLineEdit

import numpy as np
from SciQLopPlots import (
    SciQLopMultiPlotPanel, SciQLopPlot, PlotType, GraphType,
)
from SciQLopPlots.SciQLopPlotsBindings import DelegateRegistry


# Shared QApplication for the whole module.
_app = QApplication.instance() or QApplication(sys.argv)


def find_child(parent, widget_type, predicate=None):
    """Find the first descendant matching widget_type and an optional predicate."""
    for child in parent.findChildren(widget_type):
        if predicate is None or predicate(child):
            return child
    return None


def find_group(parent, title):
    """Find a QGroupBox by title."""
    return find_child(parent, QGroupBox, lambda g: g.title() == title)


def make_delegate_for(obj):
    """Resolve and instantiate the registered delegate for obj."""
    return DelegateRegistry.instance().create_delegate(obj, None)


class TestHarnessSanity(unittest.TestCase):
    """Smoke check: harness imports and QApplication exists."""

    def test_app_exists(self):
        self.assertIsNotNone(QApplication.instance())


class TestHistogram2DDelegate(unittest.TestCase):
    """Histogram2D delegate: gradient (inherited), bins, normalization."""

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        rng = np.random.default_rng(0)
        x = rng.normal(size=10000)
        y = rng.normal(size=10000)
        self.plot, self.hist = self.panel.plot(
            x, y,
            plot_type=PlotType.BasicXY,
            graph_type=GraphType.Histogram2D,
        )
        self.delegate = make_delegate_for(self.hist)
        self.assertIsNotNone(self.delegate, "delegate should resolve")

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def _norm_combo(self):
        # The Histogram2D delegate has two QComboBox children: the inherited
        # ColorGradientDelegate combo (5+ items) and the normalization combo
        # (exactly 2 items: 'None' and 'Per-column'). Identify by item count
        # and first-item text rather than by findChild order.
        for combo in self.delegate.findChildren(QComboBox):
            if combo.count() == 2 and combo.itemText(0) == 'None':
                return combo
        return None

    def test_binning_group_present(self):
        box = find_group(self.delegate, 'Binning')
        self.assertIsNotNone(box, "Binning group should exist")
        spins = box.findChildren(QSpinBox)
        self.assertEqual(len(spins), 2, "Binning group should have 2 spinboxes")

    def test_initial_key_bins_reflects_model(self):
        box = find_group(self.delegate, 'Binning')
        spin = box.findChildren(QSpinBox)[0]
        self.assertEqual(spin.value(), self.hist.key_bins())

    def test_initial_value_bins_reflects_model(self):
        box = find_group(self.delegate, 'Binning')
        spin = box.findChildren(QSpinBox)[1]
        self.assertEqual(spin.value(), self.hist.value_bins())

    def test_widget_edit_pushes_to_model(self):
        box = find_group(self.delegate, 'Binning')
        spins = box.findChildren(QSpinBox)
        spins[0].setValue(50)
        spins[1].setValue(75)
        self.assertEqual(self.hist.key_bins(), 50)
        self.assertEqual(self.hist.value_bins(), 75)

    def test_model_change_updates_widgets(self):
        self.hist.set_bins(33, 44)
        box = find_group(self.delegate, 'Binning')
        spins = box.findChildren(QSpinBox)
        self.assertEqual(spins[0].value(), 33)
        self.assertEqual(spins[1].value(), 44)

    def test_no_emit_loop_on_bins(self):
        emitted = []
        self.hist.bins_changed.connect(lambda k, v: emitted.append((k, v)))
        box = find_group(self.delegate, 'Binning')
        spins = box.findChildren(QSpinBox)
        spins[0].setValue(spins[0].value() + 1)
        # Each spinbox edit pushes both bins via shared push_bins lambda.
        # Expect at most one emit (no recursive feedback).
        self.assertLessEqual(len(emitted), 1, f"runaway emits: {emitted}")

    def test_normalization_widget_to_model(self):
        combo = self._norm_combo()
        self.assertIsNotNone(combo, "normalization combo not found")
        combo.setCurrentIndex(1)
        self.assertEqual(self.hist.normalization(), 1)
        combo.setCurrentIndex(0)
        self.assertEqual(self.hist.normalization(), 0)

    def test_normalization_model_to_widget(self):
        self.hist.set_normalization(1)
        combo = self._norm_combo()
        self.assertEqual(combo.currentData(), 1)

    def test_normalization_idempotent_no_emit(self):
        self.hist.set_normalization(1)  # set baseline
        emitted = []
        self.hist.normalization_changed.connect(lambda n: emitted.append(n))
        self.hist.set_normalization(1)  # idempotent — must NOT emit
        self.assertEqual(len(emitted), 0, "idempotent set should not emit")

    def test_min_bin_edge(self):
        self.hist.set_bins(1, 1)
        box = find_group(self.delegate, 'Binning')
        spins = box.findChildren(QSpinBox)
        self.assertEqual(spins[0].value(), 1)
        self.assertEqual(spins[1].value(), 1)

    def test_multi_instance_isolation(self):
        panel2 = SciQLopMultiPlotPanel(synchronize_x=False)
        try:
            rng = np.random.default_rng(1)
            _, hist2 = panel2.plot(
                rng.normal(size=1000), rng.normal(size=1000),
                plot_type=PlotType.BasicXY, graph_type=GraphType.Histogram2D,
            )
            self.hist.set_bins(20, 20)
            self.assertNotEqual(hist2.key_bins(), 20)
        finally:
            panel2.deleteLater()


class TestColorMapDelegate(unittest.TestCase):
    """ColorMap delegate: gradient (inherited), auto_scale_y, Contours group."""

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 10, 50)
        y = np.linspace(0, 10, 50)
        z = np.outer(np.sin(x), np.cos(y))
        self.plot, self.cmap = self.panel.plot(
            x, y, z,
            plot_type=PlotType.BasicXY,
            graph_type=GraphType.ColorMap,
        )
        self.delegate = make_delegate_for(self.cmap)
        self.assertIsNotNone(self.delegate)

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def _contours_box(self):
        return find_group(self.delegate, 'Contours')

    def _auto_scale_check(self):
        # Auto scale Y is the loose BooleanDelegate (a QCheckBox descendant)
        # OUTSIDE the Contours group. The Contours group also has a labels QCheckBox.
        contours = self._contours_box()
        in_contours = set(contours.findChildren(QCheckBox)) if contours else set()
        for cb in self.delegate.findChildren(QCheckBox):
            if cb not in in_contours:
                return cb
        return None

    def _labels_check(self):
        contours = self._contours_box()
        if not contours:
            return None
        checks = contours.findChildren(QCheckBox)
        return checks[-1] if checks else None

    def test_contours_group_present(self):
        box = self._contours_box()
        self.assertIsNotNone(box, "Contours group should exist")
        spins = box.findChildren(QSpinBox)
        dspins = box.findChildren(QDoubleSpinBox)
        self.assertEqual(len(spins), 1, "Contours group should have 1 SpinBox (level count)")
        self.assertEqual(len(dspins), 1, "Contours group should have 1 DoubleSpinBox (width)")

    def test_auto_scale_y_widget_to_model(self):
        check = self._auto_scale_check()
        self.assertIsNotNone(check, "auto_scale_y checkbox not found")
        initial = self.cmap.auto_scale_y()
        check.setChecked(not initial)
        self.assertEqual(self.cmap.auto_scale_y(), not initial)

    def test_auto_scale_y_model_to_widget(self):
        self.cmap.set_auto_scale_y(True)
        check = self._auto_scale_check()
        self.assertTrue(check.isChecked())
        self.cmap.set_auto_scale_y(False)
        self.assertFalse(check.isChecked())

    def test_contour_level_count_widget_to_model(self):
        spin = self._contours_box().findChildren(QSpinBox)[0]
        spin.setValue(7)
        self.assertEqual(self.cmap.auto_contour_level_count(), 7)

    def test_contour_level_count_model_to_widget(self):
        self.cmap.set_auto_contour_levels(12)
        spin = self._contours_box().findChildren(QSpinBox)[0]
        self.assertEqual(spin.value(), 12)

    def test_contour_width_widget_to_model(self):
        dspin = self._contours_box().findChildren(QDoubleSpinBox)[0]
        dspin.setValue(2.5)
        self.assertAlmostEqual(self.cmap.contour_width(), 2.5, places=2)

    def test_contour_width_model_to_widget(self):
        self.cmap.set_contour_width(3.0)
        dspin = self._contours_box().findChildren(QDoubleSpinBox)[0]
        self.assertAlmostEqual(dspin.value(), 3.0, places=2)

    def test_contour_labels_widget_to_model(self):
        check = self._labels_check()
        self.assertIsNotNone(check, "labels checkbox not found")
        check.setChecked(True)
        self.assertTrue(self.cmap.contour_labels_enabled())
        check.setChecked(False)
        self.assertFalse(self.cmap.contour_labels_enabled())

    def test_contour_labels_model_to_widget(self):
        self.cmap.set_contour_labels_enabled(True)
        check = self._labels_check()
        self.assertTrue(check.isChecked())
        self.cmap.set_contour_labels_enabled(False)
        self.assertFalse(check.isChecked())

    def test_zero_auto_levels_manual_mode(self):
        self.cmap.set_auto_contour_levels(0)
        spin = self._contours_box().findChildren(QSpinBox)[0]
        self.assertEqual(spin.value(), 0)


class TestGraphComponentDelegate(unittest.TestCase):
    """Graph component delegate: existing LineDelegate + Marker group.

    Exercises Line and Curve plottable kinds — both use SciQLopGraphComponent
    under the hood but via different QCustomPlot variants (QCPGraph2 vs QCPCurve).
    """

    def _make_panel(self, graph_type):
        panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        plot, graph = panel.plot(
            x, y,
            plot_type=PlotType.BasicXY,
            graph_type=graph_type,
            labels=["sig"],
        )
        comps = graph.components()
        self.assertGreater(len(comps), 0, "graph should have at least one component")
        return panel, plot, graph, comps[0]

    def setUp(self):
        # Default fixture uses Line; per-test setUps may use other kinds.
        self.panel, self.plot, self.graph, self.component = self._make_panel(GraphType.Line)
        self.delegate = make_delegate_for(self.component)
        self.assertIsNotNone(self.delegate)

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def test_marker_group_present(self):
        box = find_group(self.delegate, 'Marker')
        self.assertIsNotNone(box)
        dspins = box.findChildren(QDoubleSpinBox)
        self.assertEqual(len(dspins), 1, "Marker group should have 1 size DoubleSpinBox")

    def test_marker_size_widget_to_model(self):
        box = find_group(self.delegate, 'Marker')
        size_spin = box.findChildren(QDoubleSpinBox)[0]
        size_spin.setValue(8.0)
        self.assertAlmostEqual(self.component.marker_size(), 8.0, places=1)

    def test_marker_size_model_to_widget(self):
        self.component.set_marker_size(12.5)
        box = find_group(self.delegate, 'Marker')
        size_spin = box.findChildren(QDoubleSpinBox)[0]
        self.assertAlmostEqual(size_spin.value(), 12.5, places=1)

    def test_marker_size_no_emit_loop(self):
        emitted = []
        self.component.marker_size_changed.connect(lambda s: emitted.append(s))
        box = find_group(self.delegate, 'Marker')
        size_spin = box.findChildren(QDoubleSpinBox)[0]
        size_spin.setValue(7.0)
        self.assertLessEqual(len(emitted), 1, f"runaway emits: {emitted}")

    def test_marker_size_round_trip_curve_plottable(self):
        # Curve plot exercises the QCPCurve variant branch in set_marker_size.
        panel, plot, graph, component = self._make_panel(GraphType.ParametricCurve)
        try:
            delegate = make_delegate_for(component)
            self.assertIsNotNone(delegate)
            box = find_group(delegate, 'Marker')
            self.assertIsNotNone(box, "Marker group should exist for curves too")
            size_spin = box.findChildren(QDoubleSpinBox)[0]
            size_spin.setValue(6.5)
            self.assertAlmostEqual(component.marker_size(), 6.5, places=1)
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_marker_group_disabled_when_no_marker(self):
        # Default Line plot has marker_shape == NoMarker -> Marker group inert.
        from SciQLopPlots import GraphMarkerShape
        self.assertEqual(self.component.marker_shape(), GraphMarkerShape.NoMarker)
        box = find_group(self.delegate, 'Marker')
        self.assertFalse(box.isEnabled(), "Marker group should be disabled when shape is NoMarker")

    def test_marker_group_enables_when_shape_picked(self):
        # Drive the LineDelegate's marker combo to pick a real shape and
        # confirm the Marker group enables.
        from SciQLopPlots import GraphMarkerShape
        # The MarkerDelegate (combobox) lives inside LineDelegate; identify it
        # by item count > 1 and presence of NoMarker as one option. Easier:
        # iterate combos and pick the one whose data values map to GraphMarkerShape.
        combos = self.delegate.findChildren(QComboBox)
        marker_combo = None
        for combo in combos:
            for i in range(combo.count()):
                if combo.itemData(i) == GraphMarkerShape.Cross:
                    marker_combo = combo
                    break
            if marker_combo is not None:
                break
        self.assertIsNotNone(marker_combo, "marker shape combo not found")
        # Switch to Cross.
        target_index = next(i for i in range(marker_combo.count())
                            if marker_combo.itemData(i) == GraphMarkerShape.Cross)
        marker_combo.setCurrentIndex(target_index)
        box = find_group(self.delegate, 'Marker')
        self.assertTrue(box.isEnabled(), "Marker group should enable after picking a shape")
        # Toggle back to NoMarker -> disabled again.
        no_marker_index = next(i for i in range(marker_combo.count())
                               if marker_combo.itemData(i) == GraphMarkerShape.NoMarker)
        marker_combo.setCurrentIndex(no_marker_index)
        self.assertFalse(box.isEnabled())


class TestPlotAxisDelegate(unittest.TestCase):
    """PlotAxis delegate: label + Range group with type guards.

    Range group present for numeric axes; absent for time and color-scale axes.
    Label edit present for all axis types.
    """

    def _make_xy_panel(self):
        panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        plot, _g = panel.plot(
            x, y, plot_type=PlotType.BasicXY, graph_type=GraphType.Line, labels=["s"],
        )
        return panel, plot

    def _make_time_panel(self):
        panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
        t = np.arange(0, 100, dtype=np.float64)
        plot, _g = panel.plot(t, np.sin(t / 5), plot_type=PlotType.TimeSeries, labels=["t"])
        return panel, plot

    def _delegate_label_edit(self, delegate):
        # Scope the search to the Label group so the helper is robust against
        # other QLineEdits introduced by sibling widgets (e.g. FontDelegate's
        # QFontComboBox / QSpinBox both embed internal QLineEdits).
        label_box = find_group(delegate, 'Label')
        if label_box is None:
            return None
        return label_box.findChild(QLineEdit)

    # --- Numeric axis ---

    def test_numeric_axis_has_range_group(self):
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            self.assertFalse(ax.is_time_axis())
            delegate = make_delegate_for(ax)
            self.assertIsNotNone(find_group(delegate, 'Range'))
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_numeric_axis_label_edit(self):
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            edit = self._delegate_label_edit(delegate)
            self.assertIsNotNone(edit, "label QLineEdit not found")
            edit.setText("My Axis")
            edit.editingFinished.emit()
            self.assertEqual(ax.label(), "My Axis")
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_numeric_axis_range_widget_to_model(self):
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Range')
            spins = box.findChildren(QDoubleSpinBox)
            spins[0].setValue(0.1)
            spins[0].editingFinished.emit()
            spins[1].setValue(0.9)
            spins[1].editingFinished.emit()
            r = ax.range()
            self.assertAlmostEqual(r.start(), 0.1, places=4)
            self.assertAlmostEqual(r.stop(), 0.9, places=4)
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_numeric_axis_range_model_to_widget(self):
        from SciQLopPlots import SciQLopPlotRange
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Range')
            spins = box.findChildren(QDoubleSpinBox)
            ax.set_range(SciQLopPlotRange(0.2, 0.8))
            self.assertAlmostEqual(spins[0].value(), 0.2, places=4)
            self.assertAlmostEqual(spins[1].value(), 0.8, places=4)
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    # --- Time axis ---

    def test_time_axis_no_range_group(self):
        panel, plot = self._make_time_panel()
        try:
            tax = plot.x_axis()
            self.assertTrue(tax.is_time_axis())
            delegate = make_delegate_for(tax)
            self.assertIsNone(find_group(delegate, 'Range'),
                              "time axis must NOT show Range group")
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_time_axis_has_label_edit(self):
        panel, plot = self._make_time_panel()
        try:
            tax = plot.x_axis()
            delegate = make_delegate_for(tax)
            edit = self._delegate_label_edit(delegate)
            self.assertIsNotNone(edit, "time axis must show Label edit")
            edit.setText("Time")
            edit.editingFinished.emit()
            self.assertEqual(tax.label(), "Time")
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_label_group_present_with_font_delegate(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        panel, plot = self._make_xy_panel()
        try:
            delegate = make_delegate_for(plot.x_axis())
            box = find_group(delegate, 'Label')
            self.assertIsNotNone(box, "Label group should exist")
            self.assertIsNotNone(box.findChild(QLineEdit),
                                 "Label group should contain the QLineEdit for label text")
            self.assertIsNotNone(box.findChild(FontDelegate),
                                 "Label group should contain a FontDelegate")
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_label_font_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Label')
            font_widget = box.findChild(FontDelegate)
            font_widget.setFont(QFont("Courier New", 13))
            self.assertEqual(ax.label_font().family(), "Courier New")
            self.assertEqual(ax.label_font().pointSize(), 13)
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_label_font_model_to_widget(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Label')
            font_widget = box.findChild(FontDelegate)
            ax.set_label_font(QFont("Arial", 17))
            self.assertEqual(font_widget.font().family(), "Arial")
            self.assertEqual(font_widget.font().pointSize(), 17)
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_label_color_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QColor
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Label')
            font_widget = box.findChild(FontDelegate)
            font_widget.setColor(QColor("#abcdef"))
            self.assertEqual(ax.label_color().name(), "#abcdef")
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_tick_labels_group_present(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        panel, plot = self._make_xy_panel()
        try:
            delegate = make_delegate_for(plot.x_axis())
            box = find_group(delegate, 'Tick labels')
            self.assertIsNotNone(box, "Tick labels group should exist")
            self.assertIsNotNone(box.findChild(QCheckBox),
                                 "Tick labels group should contain visibility QCheckBox")
            self.assertIsNotNone(box.findChild(FontDelegate),
                                 "Tick labels group should contain a FontDelegate")
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_tick_label_font_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Tick labels')
            fd = box.findChild(FontDelegate)
            fd.setFont(QFont("Courier New", 8))
            self.assertEqual(ax.tick_label_font().family(), "Courier New")
            self.assertEqual(ax.tick_label_font().pointSize(), 8)
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_tick_label_font_model_to_widget(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Tick labels')
            fd = box.findChild(FontDelegate)
            ax.set_tick_label_font(QFont("Arial", 11))
            self.assertEqual(fd.font().family(), "Arial")
            self.assertEqual(fd.font().pointSize(), 11)
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_label_and_tick_label_font_delegates_are_distinct(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            ax.set_label_font(QFont("Times", 18))
            ax.set_tick_label_font(QFont("Arial", 9))
            label_box = find_group(delegate, 'Label')
            tick_box = find_group(delegate, 'Tick labels')
            self.assertEqual(label_box.findChild(FontDelegate).font().family(), "Times")
            self.assertEqual(tick_box.findChild(FontDelegate).font().family(), "Arial")
            delegate.deleteLater()
        finally:
            panel.deleteLater()


class TestAxisFontControls(unittest.TestCase):
    """Axis label and tick-label font/color setters + signals.

    Exercises the wrapper surface directly (no delegate). The inspector
    delegate is exercised in TestPlotAxisDelegate's added cases.
    """

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        self.plot, _g = self.panel.plot(
            x, y,
            plot_type=PlotType.BasicXY,
            graph_type=GraphType.Line,
            labels=["s"],
        )
        self.axis = self.plot.x_axis()

    def tearDown(self):
        self.panel.deleteLater()

    def test_label_font_round_trip(self):
        from PySide6.QtGui import QFont
        f = QFont("Courier New", 14)
        f.setBold(True)
        self.axis.set_label_font(f)
        got = self.axis.label_font()
        self.assertEqual(got.family(), "Courier New")
        self.assertEqual(got.pointSize(), 14)
        self.assertTrue(got.bold())

    def test_label_color_round_trip(self):
        from PySide6.QtGui import QColor
        self.axis.set_label_color(QColor("red"))
        self.assertEqual(self.axis.label_color().name(), "#ff0000")

    def test_tick_label_font_round_trip(self):
        from PySide6.QtGui import QFont
        f = QFont("Arial", 9)
        f.setItalic(True)
        self.axis.set_tick_label_font(f)
        got = self.axis.tick_label_font()
        self.assertEqual(got.family(), "Arial")
        self.assertEqual(got.pointSize(), 9)
        self.assertTrue(got.italic())

    def test_tick_label_color_round_trip(self):
        from PySide6.QtGui import QColor
        self.axis.set_tick_label_color(QColor("#00aa00"))
        self.assertEqual(self.axis.tick_label_color().name(), "#00aa00")

    def test_label_font_signal_fires_once(self):
        from PySide6.QtGui import QFont
        emitted = []
        self.axis.label_font_changed.connect(lambda f: emitted.append(f))
        self.axis.set_label_font(QFont("Courier New", 11))
        self.assertEqual(len(emitted), 1)

    def test_label_font_idempotent_no_emit(self):
        from PySide6.QtGui import QFont
        f = QFont("Courier New", 11)
        self.axis.set_label_font(f)  # baseline
        emitted = []
        self.axis.label_font_changed.connect(lambda x: emitted.append(x))
        self.axis.set_label_font(f)  # same value
        self.assertEqual(len(emitted), 0)

    def test_label_color_change_does_not_fire_font_signal(self):
        from PySide6.QtGui import QColor
        emitted = []
        self.axis.label_font_changed.connect(lambda f: emitted.append(f))
        self.axis.set_label_color(QColor("blue"))
        self.assertEqual(len(emitted), 0,
                         "label_color edit must not fire label_font_changed")


class TestLegendFontControls(unittest.TestCase):
    """Legend font + color round-trip and signal isolation."""

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        self.plot, _g = self.panel.plot(
            x, y, plot_type=PlotType.BasicXY,
            graph_type=GraphType.Line, labels=["s"],
        )
        self.legend = self.plot.legend()

    def tearDown(self):
        self.panel.deleteLater()

    def test_font_round_trip(self):
        from PySide6.QtGui import QFont
        f = QFont("Courier New", 12)
        f.setItalic(True)
        self.legend.set_font(f)
        got = self.legend.font()
        self.assertEqual(got.family(), "Courier New")
        self.assertEqual(got.pointSize(), 12)
        self.assertTrue(got.italic())

    def test_color_round_trip(self):
        from PySide6.QtGui import QColor
        self.legend.set_color(QColor("#0080ff"))
        self.assertEqual(self.legend.color().name(), "#0080ff")

    def test_font_signal_fires_once(self):
        from PySide6.QtGui import QFont
        emitted = []
        self.legend.font_changed.connect(lambda f: emitted.append(f))
        self.legend.set_font(QFont("Arial", 13))
        self.assertEqual(len(emitted), 1)

    def test_font_idempotent_no_emit(self):
        from PySide6.QtGui import QFont
        f = QFont("Arial", 13)
        self.legend.set_font(f)
        emitted = []
        self.legend.font_changed.connect(lambda x: emitted.append(x))
        self.legend.set_font(f)
        self.assertEqual(len(emitted), 0)

    def test_color_change_does_not_fire_font_signal(self):
        from PySide6.QtGui import QColor
        emitted = []
        self.legend.font_changed.connect(lambda f: emitted.append(f))
        self.legend.set_color(QColor("magenta"))
        self.assertEqual(len(emitted), 0)

    def test_font_change_does_not_fire_color_signal(self):
        from PySide6.QtGui import QFont
        emitted = []
        self.legend.color_changed.connect(lambda c: emitted.append(c))
        self.legend.set_font(QFont("Courier New", 17))
        self.assertEqual(len(emitted), 0)


class TestPlotDelegate(unittest.TestCase):
    """SciQLopPlot delegate: legend, auto_scale, crosshair, equal aspect, scroll factor.

    Crosshair defaults to True after plot construction — tests that wire the signal
    must normalize to a known state before connecting.
    """

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 50)
        self.plot, _g = self.panel.plot(
            x, np.sin(x * 6), plot_type=PlotType.BasicXY, graph_type=GraphType.Line,
            labels=["s"],
        )
        self.delegate = make_delegate_for(self.plot)
        self.assertIsNotNone(self.delegate)

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def _scroll_spin(self):
        # The scroll factor spin is the only direct-child QDoubleSpinBox of the
        # delegate (range 1.0..10.0). Range axes have their own spins but they
        # live inside the axis delegate, not the plot delegate.
        for s in self.delegate.findChildren(QDoubleSpinBox):
            if s.minimum() == 1.0 and s.maximum() == 10.0:
                return s
        return None

    def test_scroll_factor_widget_to_model(self):
        spin = self._scroll_spin()
        self.assertIsNotNone(spin)
        spin.setValue(2.5)
        self.assertAlmostEqual(self.plot.scroll_factor(), 2.5, places=2)

    def test_scroll_factor_model_to_widget(self):
        self.plot.set_scroll_factor(3.0)
        spin = self._scroll_spin()
        self.assertAlmostEqual(spin.value(), 3.0, places=2)

    def test_scroll_factor_clamps_below_min(self):
        spin = self._scroll_spin()
        spin.setValue(0.5)  # below min, should clamp to 1.0
        self.assertGreaterEqual(spin.value(), 1.0)

    def test_scroll_factor_clamps_above_max(self):
        spin = self._scroll_spin()
        spin.setValue(50.0)  # above max, should clamp to 10.0
        self.assertLessEqual(spin.value(), 10.0)

    def test_crosshair_round_trip_through_model(self):
        # Default crosshair state is True. Normalize to a known state, then toggle.
        self.plot.set_crosshair_enabled(False)
        self.assertFalse(self.plot.crosshair_enabled())
        self.plot.set_crosshair_enabled(True)
        self.assertTrue(self.plot.crosshair_enabled())

    def test_crosshair_idempotent_set(self):
        self.plot.set_crosshair_enabled(False)
        emitted = []
        self.plot.crosshair_enabled_changed.connect(lambda b: emitted.append(b))
        self.plot.set_crosshair_enabled(False)  # idempotent — must not emit
        self.assertEqual(len(emitted), 0)

    def test_equal_aspect_round_trip_through_model(self):
        self.plot.set_equal_aspect_ratio(True)
        self.assertTrue(self.plot.equal_aspect_ratio())
        self.plot.set_equal_aspect_ratio(False)
        self.assertFalse(self.plot.equal_aspect_ratio())

    def test_equal_aspect_idempotent_set(self):
        self.plot.set_equal_aspect_ratio(True)  # set baseline
        emitted = []
        self.plot.equal_aspect_ratio_changed.connect(lambda b: emitted.append(b))
        self.plot.set_equal_aspect_ratio(True)  # idempotent — must not emit
        self.assertEqual(len(emitted), 0)

    def test_delegate_has_at_least_three_checkboxes(self):
        # legend, auto_scale, crosshair, equal_aspect = 4 boolean-ish widgets
        # (legend is a custom LegendDelegate; the rest are BooleanDelegate which
        # contains a QCheckBox). Expect at least 3 QCheckBox descendants.
        checks = self.delegate.findChildren(QCheckBox)
        self.assertGreaterEqual(len(checks), 3,
            "expected at least 3 BooleanDelegate checkboxes")


class TestWaterfallDelegateRegression(unittest.TestCase):
    """Regression: after wrapping mode/spacing/offsets in an Offsets QGroupBox,
    the waterfall delegate must still correctly drive the model.

    Constructed via SciQLopPlot directly because SciQLopMultiPlotPanel.plot(...)
    routes waterfall through a non-existent cls.waterfall accessor (pre-existing
    panel-API gap, unrelated to this task).
    """

    def setUp(self):
        self.plot = SciQLopPlot()
        x = np.linspace(0, 10, 200)
        y = np.column_stack([np.sin(x * k) for k in range(1, 4)])
        self.wf = self.plot.plot(
            x, y,
            graph_type=GraphType.Waterfall,
            labels=["a", "b", "c"],
        )
        self.delegate = make_delegate_for(self.wf)
        self.assertIsNotNone(self.delegate)

    def tearDown(self):
        self.delegate.deleteLater()
        self.plot.deleteLater()

    def test_offsets_group_present(self):
        box = find_group(self.delegate, 'Offsets')
        self.assertIsNotNone(box)

    def test_offsets_group_contains_mode_combo(self):
        box = find_group(self.delegate, 'Offsets')
        combos = box.findChildren(QComboBox)
        # Mode combo: exactly 2 items ("Uniform", "Custom")
        mode = next((c for c in combos if c.count() == 2 and c.itemText(0) == 'Uniform'), None)
        self.assertIsNotNone(mode, "Mode combo not found inside Offsets group")

    def test_uniform_spacing_widget_to_model(self):
        box = find_group(self.delegate, 'Offsets')
        # Spacing is the first DoubleSpinBox inside the Offsets group.
        spin = box.findChildren(QDoubleSpinBox)[0]
        spin.setValue(2.5)
        self.assertAlmostEqual(self.wf.uniform_spacing(), 2.5, places=2)

    def test_uniform_spacing_model_to_widget(self):
        self.wf.set_uniform_spacing(3.0)
        box = find_group(self.delegate, 'Offsets')
        spin = box.findChildren(QDoubleSpinBox)[0]
        self.assertAlmostEqual(spin.value(), 3.0, places=2)

    def test_gain_spinbox_loose_outside_group(self):
        # Gain is loose (outside any QGroupBox). It's the only DoubleSpinBox in
        # the delegate that is NOT a child of the Offsets group.
        box = find_group(self.delegate, 'Offsets')
        all_dspins = self.delegate.findChildren(QDoubleSpinBox)
        in_box = set(box.findChildren(QDoubleSpinBox))
        loose = [s for s in all_dspins if s not in in_box]
        self.assertGreaterEqual(len(loose), 1, "gain spinbox should be loose")
        loose[-1].setValue(1.5)
        self.assertAlmostEqual(self.wf.gain(), 1.5, places=2)

    def test_normalize_loose_outside_group(self):
        box = find_group(self.delegate, 'Offsets')
        all_checks = self.delegate.findChildren(QCheckBox)
        in_box = set(box.findChildren(QCheckBox))
        loose = [c for c in all_checks if c not in in_box]
        self.assertGreaterEqual(len(loose), 1, "normalize checkbox should be loose")


class TestPanelWaterfallRoute(unittest.TestCase):
    """Regression: SciQLopMultiPlotPanel.plot(graph_type=Waterfall) returns (plot, wf)
    and applies waterfall kwargs (offsets/normalize/gain) post-hoc."""

    def test_panel_waterfall_basic(self):
        panel = SciQLopMultiPlotPanel(synchronize_x=False)
        try:
            x = np.linspace(0, 10, 100)
            y = np.column_stack([np.sin(x * k) for k in range(1, 4)])
            result = panel.plot(
                x, y,
                plot_type=PlotType.BasicXY,
                graph_type=GraphType.Waterfall,
                labels=["a", "b", "c"],
            )
            self.assertIsInstance(result, tuple, "panel waterfall must return (plot, wf)")
            self.assertEqual(len(result), 2)
            plot, wf = result
            self.assertEqual(type(wf).__name__, 'SciQLopWaterfallGraph')
            # Defaults applied via _apply_waterfall_kwargs.
            self.assertAlmostEqual(wf.uniform_spacing(), 1.0, places=2)
            self.assertAlmostEqual(wf.gain(), 1.0, places=2)
        finally:
            panel.deleteLater()

    def test_panel_waterfall_with_kwargs(self):
        panel = SciQLopMultiPlotPanel(synchronize_x=False)
        try:
            x = np.linspace(0, 10, 100)
            y = np.column_stack([np.sin(x * k) for k in range(1, 4)])
            _, wf = panel.plot(
                x, y,
                plot_type=PlotType.BasicXY,
                graph_type=GraphType.Waterfall,
                labels=["a", "b", "c"],
                offsets=2.5,
                normalize=False,
                gain=3.0,
            )
            self.assertAlmostEqual(wf.uniform_spacing(), 2.5, places=2)
            self.assertAlmostEqual(wf.gain(), 3.0, places=2)
            self.assertFalse(wf.normalize())
        finally:
            panel.deleteLater()


class TestTextItemFontControls(unittest.TestCase):
    """SciQLopTextItem font + font family setters."""

    def setUp(self):
        from SciQLopPlots import SciQLopMultiPlotPanel
        from PySide6.QtCore import QPointF
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        self.plot, _g = self.panel.plot(
            x, y, plot_type=PlotType.BasicXY,
            graph_type=GraphType.Line, labels=["s"],
        )
        # SciQLopTextItem(plot, text, position, movable=False, coordinates=Pixels)
        from SciQLopPlots import SciQLopTextItem, Coordinates
        self.item = SciQLopTextItem(self.plot, "hello",
                                    QPointF(0.5, 0.5), False,
                                    Coordinates.Data)

    def tearDown(self):
        self.panel.deleteLater()

    def test_set_font_round_trip(self):
        from PySide6.QtGui import QFont
        f = QFont("Courier New", 16)
        f.setBold(True)
        self.item.set_font(f)
        got = self.item.font()
        self.assertEqual(got.family(), "Courier New")
        self.assertEqual(got.pointSize(), 16)
        self.assertTrue(got.bold())

    def test_set_font_family_preserves_size(self):
        # Set initial size via existing setter
        self.item.set_font_size(20.0)
        self.item.set_font_family("Arial")
        self.assertEqual(self.item.font().family(), "Arial")
        self.assertAlmostEqual(self.item.font().pointSizeF(), 20.0, places=1)

    def test_set_font_size_still_works_after_set_font(self):
        # Backward compat: set_font_size keeps working after set_font has been used.
        from PySide6.QtGui import QFont
        self.item.set_font(QFont("Arial", 10))
        self.item.set_font_size(18.0)
        self.assertAlmostEqual(self.item.font_size(), 18.0, places=1)
        self.assertEqual(self.item.font().family(), "Arial")


class TestFontDelegate(unittest.TestCase):
    """FontDelegate widget surface — getter, setter, signal isolation.

    Driven via the public API (setFont/setColor/font/color + the two
    signals). Internal popup widgets are not exercised here; they're
    indirectly covered by the inspector tests that wire FontDelegate
    to model setters.
    """

    def setUp(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont, QColor
        self.delegate = FontDelegate(QFont("Arial", 10), QColor("black"), None)

    def tearDown(self):
        self.delegate.deleteLater()

    def test_initial_font_and_color(self):
        self.assertEqual(self.delegate.font().family(), "Arial")
        self.assertEqual(self.delegate.font().pointSize(), 10)
        self.assertEqual(self.delegate.color().name(), "#000000")

    def test_set_font_emits_font_changed_only(self):
        from PySide6.QtGui import QFont
        font_emits, color_emits = [], []
        self.delegate.fontChanged.connect(lambda f: font_emits.append(f))
        self.delegate.colorChanged.connect(lambda c: color_emits.append(c))
        self.delegate.setFont(QFont("Courier New", 12))
        self.assertEqual(len(font_emits), 1)
        self.assertEqual(len(color_emits), 0)

    def test_set_color_emits_color_changed_only(self):
        from PySide6.QtGui import QColor
        font_emits, color_emits = [], []
        self.delegate.fontChanged.connect(lambda f: font_emits.append(f))
        self.delegate.colorChanged.connect(lambda c: color_emits.append(c))
        self.delegate.setColor(QColor("#ff00ff"))
        self.assertEqual(len(color_emits), 1)
        self.assertEqual(len(font_emits), 0)

    def test_set_font_idempotent_no_emit(self):
        from PySide6.QtGui import QFont
        f = QFont("Arial", 10)
        emits = []
        self.delegate.fontChanged.connect(lambda x: emits.append(x))
        self.delegate.setFont(f)  # same as initial
        self.assertEqual(len(emits), 0)

    def test_set_color_idempotent_no_emit(self):
        from PySide6.QtGui import QColor
        emits = []
        self.delegate.colorChanged.connect(lambda x: emits.append(x))
        self.delegate.setColor(QColor("black"))  # same as initial
        self.assertEqual(len(emits), 0)


class TestPlotDelegateLegendFont(unittest.TestCase):
    """Plot delegate's Legend group: visibility + FontDelegate."""

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        self.plot, _g = self.panel.plot(
            x, y, plot_type=PlotType.BasicXY,
            graph_type=GraphType.Line, labels=["s"],
        )
        self.delegate = make_delegate_for(self.plot)

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def test_legend_group_present(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        box = find_group(self.delegate, 'Legend')
        self.assertIsNotNone(box, "Legend group should exist on plot delegate")
        self.assertIsNotNone(box.findChild(FontDelegate),
                             "Legend group should contain a FontDelegate")

    def test_legend_font_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        box = find_group(self.delegate, 'Legend')
        fd = box.findChild(FontDelegate)
        fd.setFont(QFont("Courier New", 11))
        self.assertEqual(self.plot.legend().font().family(), "Courier New")
        self.assertEqual(self.plot.legend().font().pointSize(), 11)

    def test_legend_font_model_to_widget(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        box = find_group(self.delegate, 'Legend')
        fd = box.findChild(FontDelegate)
        self.plot.legend().set_font(QFont("Arial", 14))
        self.assertEqual(fd.font().family(), "Arial")
        self.assertEqual(fd.font().pointSize(), 14)

    def test_legend_color_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QColor
        box = find_group(self.delegate, 'Legend')
        fd = box.findChild(FontDelegate)
        fd.setColor(QColor("#123456"))
        self.assertEqual(self.plot.legend().color().name(), "#123456")


class TestTextItemDelegate(unittest.TestCase):
    """Inspector delegate for SciQLopTextItem: text + font controls."""

    def setUp(self):
        from SciQLopPlots import SciQLopTextItem, Coordinates
        from PySide6.QtCore import QPointF
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        self.plot, _g = self.panel.plot(
            x, y, plot_type=PlotType.BasicXY,
            graph_type=GraphType.Line, labels=["s"],
        )
        self.item = SciQLopTextItem(self.plot, "label",
                                    QPointF(0.5, 0.5), False,
                                    Coordinates.Data)
        self.delegate = make_delegate_for(self.item)
        self.assertIsNotNone(self.delegate, "TextItem delegate should resolve")

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def test_text_edit_present(self):
        line_edit = self.delegate.findChild(QLineEdit)
        self.assertIsNotNone(line_edit)
        self.assertEqual(line_edit.text(), "label")

    def test_font_delegate_present(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        fd = self.delegate.findChild(FontDelegate)
        self.assertIsNotNone(fd, "TextItem delegate should embed a FontDelegate")

    def test_font_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        fd = self.delegate.findChild(FontDelegate)
        fd.setFont(QFont("Courier New", 22))
        self.assertEqual(self.item.font().family(), "Courier New")
        self.assertEqual(self.item.font().pointSize(), 22)

    def test_color_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QColor
        fd = self.delegate.findChild(FontDelegate)
        fd.setColor(QColor("#22aabb"))
        self.assertEqual(self.item.color().name(), "#22aabb")

    def test_text_widget_to_model(self):
        line_edit = self.delegate.findChild(QLineEdit)
        line_edit.setText("hi")
        line_edit.editingFinished.emit()
        self.assertEqual(self.item.text(), "hi")


if __name__ == '__main__':
    unittest.main(verbosity=2)
