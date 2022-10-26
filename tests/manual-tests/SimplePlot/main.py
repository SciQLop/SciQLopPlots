from PySide6 import QtWidgets, QtCore, QtGui
import numpy as np
from SciQLopPlotsBindings import *
from datetime import datetime
import speasy as spz

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
        QtCore.QObject.__init__(self, graph)
        graph.xRangeChanged.connect(self.get_data)
        self.amp = amp
        self.freq = freq
        self.graph = graph
        self._th = QtCore.QThread()
        self.moveToThread(self._th)
        self._th.start()

    def __del__(self):
        self._th.quit()
        self._th.wait()


    def get_data(self, new_range):
        x=np.arange(new_range.first, new_range.second)*1.
        y=np.cos(x/100.*self.freq)*self.amp
        self.graph.plot(x,y)

class SpzWorker(QtCore.QObject):
    def __init__(self, graph, product):
        QtCore.QObject.__init__(self, graph)
        graph.xRangeChanged.connect(self.get_data)
        self.graph = graph
        self.product = product
        self._th = QtCore.QThread()
        self.moveToThread(self._th)
        self._th.start()

    def __del__(self):
        self._th.quit()
        self._th.wait()

    def get_data(self, new_range):
        beta=spz.get_data(self.product, datetime.utcfromtimestamp(new_range.first), datetime.utcfromtimestamp(new_range.second))
        if beta is not None:
            beta.replace_fillval_by_nan(inplace=True)
            self.graph.plot(beta.time.astype(np.timedelta64) / np.timedelta64(1, 's'),beta.values.astype(np.float))


app=QtWidgets.QApplication()

s=SciQLopPlots.SyncPanel()

p=SciQLopPlots.PlotWidget()
s.addPlot(p)

graphs = []
workers = []
for i in range(5):
    g=p.addLineGraph(colors[i])
    w=Worker(g, 1+i/5, i+1)
    graphs.append(g)
    workers.append(w)

i=0
p2=SciQLopPlots.PlotWidget()
ts=SciQLopPlots.TimeSpan(p2, SciQLopPlots.axis.range(datetime(2020,10,10).timestamp(), datetime(2020,10,10,1).timestamp()))
s.addPlot(p2)
prods=[spz.inventories.tree.cda.OMNI_Combined_1AU_IP_Data__Magnetic_and_Solar_Indices.OMNI_1AU_IP_Data.IMF_and_Plasma_data.OMNI_HRO_1MIN.BX_GSE,
spz.inventories.tree.cda.OMNI_Combined_1AU_IP_Data__Magnetic_and_Solar_Indices.OMNI_1AU_IP_Data.IMF_and_Plasma_data.OMNI_HRO_1MIN.BY_GSE,
spz.inventories.tree.cda.OMNI_Combined_1AU_IP_Data__Magnetic_and_Solar_Indices.OMNI_1AU_IP_Data.IMF_and_Plasma_data.OMNI_HRO_1MIN.BZ_GSE,
spz.inventories.tree.cda.OMNI_Combined_1AU_IP_Data__Magnetic_and_Solar_Indices.OMNI_1AU_IP_Data.IMF_and_Plasma_data.OMNI_HRO_1MIN.Beta
]
for prod in prods:
    g=p2.addLineGraph(colors[i])
    i+=1
    w=SpzWorker(g, prod)
    graphs.append(g)
    workers.append(w)



s.setXRange(SciQLopPlots.axis.range(datetime(2020,10,10).timestamp(), datetime(2020,10,10,1).timestamp()))
s.show()
app.exec()
