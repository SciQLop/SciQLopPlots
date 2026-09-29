"""Runs an interactive example as a smoke test: it must start, show its plots and quit cleanly.

The examples end in app.exec(), which waits for a human to close the window, so
under `meson test` they would only ever time out. Here exec() quits by itself.
Usage: run_example.py <example.py>
"""
import os
import runpy
import sys

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

SHOW_FOR_MS = 2000
_exec = QApplication.exec


def _exec_briefly(*_):
    QTimer.singleShot(SHOW_FOR_MS, QApplication.quit)
    return _exec()


QApplication.exec = _exec_briefly
example = sys.argv[1]
sys.argv = sys.argv[1:]
sys.path.insert(0, os.path.dirname(os.path.abspath(example)))
runpy.run_path(example, run_name="__main__")
