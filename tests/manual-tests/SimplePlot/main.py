from PySide6 import QtWidgets, QtCore, QtGui
import numpy as np
from SciQLopPlots import *
from datetime import datetime

colors = [
    QtGui.QColorConstants.Black,
    QtGui.QColorConstants.DarkGray,
    QtGui.QColorConstants.Red,
    QtGui.QColorConstants.Green,
    QtGui.QColorConstants.Blue,
    QtGui.QColorConstants.Cyan,
    QtGui.QColorConstants.Magenta,
    QtGui.QColorConstants.DarkRed,
    QtGui.QColorConstants.DarkGreen
]

class Worker(QtCore.QObject):
    def __init__(self,graph, amp=1., freq=1.):
        QtCore.QObject.__init__(self)
        self.amp = amp
        self.freq = freq
        self.graph = graph
        self._th = QtCore.QThread()
        self.moveToThread(self._th)
        self._th.start()
        graph.xRangeChanged.connect(self.get_data)

    def __del__(self):
        self._th.quit()
        self._th.wait()


    def get_data(self, new_range):
        x=np.arange(new_range.first, new_range.second)*1.
        y=np.cos(x/100.*self.freq)*self.amp
        self.graph.plot(x,y)

class Worker2(QtCore.QObject):
    def __init__(self,graph, amp=1., freq=1.):
        QtCore.QObject.__init__(self)
        graph.xRangeChanged.connect(self.get_data)
        self.amp = amp
        self.freq = freq
        self.lines= 3
        self.graph = graph
        self._th = QtCore.QThread()
        self.moveToThread(self._th)
        self._th.start()

    def __del__(self):
        self._th.quit()
        self._th.wait()


    def get_data(self, new_range):
        x=np.arange(new_range.first, new_range.second, 0.1)*1.
        y=np.cos([(x+l*100)/100.*self.freq for l in range(self.lines)])*self.amp
        self.graph.plot(x,y.T, enums.DataOrder.y_first)


app=QtWidgets.QApplication()

s=SyncPanel()

p=PlotWidget()
s.addPlot(p)

graphs = []
workers = []
for i in range(5):
    g=p.addLineGraph(colors[i])
    w=Worker(g, 1+i/5, i+1)
    graphs.append(g)
    workers.append(w)

i=0
p2=PlotWidget()
s.addPlot(p2,0)
g=p2.addMultiLineGraph(colors[:3])
w=Worker2(g, 1+i/5, i+1)
graphs.append(g)
workers.append(w)

ts=TimeSpan(p, axis.range(datetime(2020,10,10).timestamp(), datetime(2020,10,10,1).timestamp()))
s.setXRange(axis.range(datetime(2020,10,10).timestamp(), datetime(2020,10,10,1).timestamp()))
s.show()
app.exec()
