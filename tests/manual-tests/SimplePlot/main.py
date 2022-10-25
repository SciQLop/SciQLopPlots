from PySide6 import QtWidgets, QtCore, QtGui
import numpy as np
from SciQLopPlotsBindings import *

class Worker(QtCore.QObject):
    def __init__(self,graph):
        QtCore.QObject.__init__(self)
        self.graph = graph
        self._th = QtCore.QThread()
        self.moveToThread(self._th)
        self._th.start()

    def get_data(self, new_range):
        x=np.arange(new_range.first, new_range.second, 0.1)*1.
        y=np.cos(x)
        self.graph.plot(x,y)



app=QtWidgets.QApplication()


p=SciQLopPlots.PlotWidget()
g=p.addLineGraph(QtGui.QColorConstants.Blue)
g.plot(np.arange(10000)*1., np.cos(np.arange(10000)/10))


def update_plot(new_range):
    x=np.arange(new_range.first, new_range.second, 0.1)*1.
    y=np.cos(x)
    g.plot(x,y)

w=Worker(g)

g.xRangeChanged.connect(w.get_data)

p.show()
app.exec()
