from PySide6.QtWidgets import QMainWindow, QApplication, QScrollArea,QWidget, QVBoxLayout, QTabWidget, QDockWidget
from PySide6.QtCore import Qt
import sys, os
import numpy as np
from datetime import datetime, timezone
import threading
from typing import Dict, Any
import platform

from SciQLopPlots import PropertiesPanel

from qtconsole.rich_jupyter_widget import RichJupyterWidget
from qtconsole.inprocess import QtInProcessKernelManager

os.environ['QT_API'] = 'PySide6'

if platform.system() == 'Linux':
    os.environ['QT_QPA_PLATFORM'] = os.environ.get("QT_QPA_PLATFORM", 'xcb')


def fix_name(name):
    return name.replace(" ", "_").replace(":", "_").replace("/", "_")

class Tabs(QTabWidget):
    def __init__(self,parent):
        QTabWidget.__init__(self,parent)
        self.setMouseTracking(True)
        self.setTabPosition(QTabWidget.TabPosition.North)
        self.setTabShape(QTabWidget.TabShape.Rounded)
        self.setMovable(True)

    def add_tab(self, widget, title):
        self.addTab(widget, title)
        widget.setObjectName(title)



class MainWindow(QMainWindow):
    def __init__(self):
        QMainWindow.__init__(self)
        self.setMouseTracking(True)
        self.axisRects=[]
        self.graphs = []
        self._setup_kernel()
        self._setup_ui()
        self.setGeometry(400, 250, 542, 390);
        self.setWindowTitle("SciQLopPlots Test")
        self.add_to_kernel_namespace('main_window', self)

    def add_tab(self, widget, title):
        self.tabs.add_tab(widget, title)
        self.add_to_kernel_namespace(fix_name(title), widget)

    def _setup_kernel(self):
        self.kernel_manager = QtInProcessKernelManager()
        self.kernel_manager.start_kernel(show_banner=False)
        self.kernel = self.kernel_manager.kernel
        self.kernel.gui = 'qt'

        self.kernel_client = self.kernel_manager.client()
        self.kernel_client.start_channels()

    def add_to_kernel_namespace(self, name, obj):
        self.kernel.shell.push({name: obj})

    def _setup_ui(self):
        self.tabs = Tabs(self)
        self.setCentralWidget(self.tabs)
        self.ipython_widget = RichJupyterWidget(self)
        dock = QDockWidget("IPython Console", self)
        dock.setWidget(self.ipython_widget)
        self.addDockWidget(Qt.BottomDockWidgetArea, dock)
        self.ipython_widget.kernel_manager = self.kernel_manager
        self.ipython_widget.kernel_client = self.kernel_client

        self.properties_panel = PropertiesPanel(self)
        dock = QDockWidget("Properties panel", self)
        dock.setWidget(self.properties_panel)
        self.addDockWidget(Qt.RightDockWidgetArea, dock)



