from SciQLopPlots import SciQLopPlot, QCP, QCPColorMap, QCPRange, QCPColorScale, QCPAxis, QCPColorGradient, QCPMarginGroup
from PySide6.QtWidgets import QMainWindow, QApplication
from PySide6.QtGui import QPen, QColorConstants, QColor, QBrush
import sys
import math
import numpy as np
from types import SimpleNamespace

from SciQLopPlots import SciQLopGraph , QCustomPlot, SciQLopVerticalSpan


if __name__ == '__main__':
    app = QApplication(sys.argv)
    p=SciQLopPlot()
    p.setInteractions(QCP.iRangeDrag|QCP.iRangeZoom|QCP.iSelectPlottables|QCP.iSelectItems)

    s1=SciQLopVerticalSpan(p, QCPRange(0., 1.))
    s2=SciQLopVerticalSpan(p, QCPRange(1.1, 2.1))

    p.show()
    app.exec()
