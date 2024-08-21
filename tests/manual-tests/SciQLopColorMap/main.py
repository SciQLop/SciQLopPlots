from SciQLopPlots import SciQLopPlot, SciQLopMultiPlotPanel
from PySide6.QtWidgets import QMainWindow, QApplication
from PySide6.QtCore import Qt
import sys
import os
import math
import numpy as np
from types import SimpleNamespace

os.environ['QT_API'] = 'PySide6'

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from common import MainWindow

def make_uniform_colormap(nx, ny):

    z = np.empty((nx,ny))
    x_vect= np.arange(nx)*8./nx -4.
    y_vect= np.arange(ny)*8./ny -4.

    z[:,:] = np.cos(np.sqrt((x_vect.reshape(-1,1)+2)**2 @ y_vect.reshape(-1,1).T**2))
    return x_vect, y_vect, z


class UniformColormap(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        self.plot(*make_uniform_colormap(200, 200))


class UniformColormap2DY(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        x, y, z = make_uniform_colormap(300, 200)
        y = np.array(np.repeat(y, 300).reshape(200, 300).T)
        self.plot(x, y, z)


class ColormapWithNonUniformXAxis(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self, parent, synchronize_x=False, synchronize_time=True)
        _x, y, _z = make_uniform_colormap(2010, 200)
        x = np.linspace(-4, 4, 210)
        z = np.empty((210, 200))
        x[:100] = _x[:1000:10]
        x[100:110] = _x[1000:1010]
        x[110:] = _x[1010::10]
        z[:100, :] = _z[:1000:10, :]
        z[100:110, :] = _z[1000:1010, :]
        z[110:, :] = _z[1010::10, :]
        self.plot(x, y, z)


if __name__ == '__main__':
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.add_tab(UniformColormap(w), "UniformColormap")
    w.add_tab(UniformColormap2DY(w), "UniformColormap2DY")
    w.add_tab(ColormapWithNonUniformXAxis(w), "ColormapWithNonUniformXAxis")
    w.show()
    app.exec()
