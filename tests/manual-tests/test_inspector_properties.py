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


if __name__ == '__main__':
    unittest.main(verbosity=2)
