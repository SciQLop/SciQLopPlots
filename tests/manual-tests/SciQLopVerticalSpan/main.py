import os
os.environ['QT_QPA_PLATFORM'] = "xcb"
from SciQLopPlots import SciQLopPlot, QCP, QCPColorMap, QCPItemRect, QCPRange, QCPColorScale, QCPAxis, QCPColorGradient, QCPMarginGroup
from PySide6.QtWidgets import QMainWindow, QApplication
from PySide6.QtGui import QPen, QColorConstants, QColor, QBrush, Qt
import sys
import math
import numpy as np
from types import SimpleNamespace

from SciQLopPlots import SciQLopGraph , QCustomPlot, SciQLopVerticalSpan


if __name__ == '__main__':
    app = QApplication(sys.argv)
    p=SciQLopPlot()
    p.setOpenGl(True)

    p.setInteractions(QCP.iRangeDrag|QCP.iRangeZoom|QCP.iSelectPlottables|QCP.iSelectItems)
    graph = p.addSciQLopGraph(p.xAxis, p.yAxis, ["X","Y","Z"])
    x=np.arange(3e7)*1.
    y=np.ones((3,len(x)))
    y[0]=np.cos(x/60)
    y[1]=np.cos(x/600)*1.3
    y[2]=np.cos(x/6000)*1.7
    graph.setData(x,y)

    graph.graphAt(0).setPen(QPen(QColorConstants.Red))
    graph.graphAt(1).setPen(QPen(QColorConstants.Blue))
    graph.graphAt(2).setPen(QPen(QColorConstants.Green))

    p.legend.setVisible(True)

    p.rescaleAxes()


    spans = [SciQLopVerticalSpan(p, QCPRange(i*20., i*20.+10.)) for i in range(5)]
    for i in range(5):
        spans[i].setToolTip(f"Vertical span {i}")


    p.show()
    app.exec()
