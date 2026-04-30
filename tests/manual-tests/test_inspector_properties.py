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
    SciQLopMultiPlotPanel, PlotType, GraphType,
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


if __name__ == '__main__':
    unittest.main(verbosity=2)
