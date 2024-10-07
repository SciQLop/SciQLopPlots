from SciQLopPlots import SciQLopPlot, SciQLopMultiPlotPanel, ProductsModel, ProductsModelNode, ProductsModelNodeType, ProductsView
from PySide6.QtWidgets import QMainWindow, QApplication
from PySide6.QtCore import Qt
import sys
import os

os.environ['QT_API'] = 'PySide6'

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from common import MainWindow


if __name__ == '__main__':
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.add_tab(ProductsView(w), "Products")
    for i in range(3):
        node=ProductsModelNode(f"hello_{i}",{"UNITS":'nT', 'test':i}, ProductsModelNodeType.PRODUCT)
        ProductsModel.instance().add_node("test/some/path".split("/"),node)

    for i in range(3):
        node=ProductsModelNode(f"hello_{i}",{"UNITS":'nT', 'test':i}, ProductsModelNodeType.PRODUCT)
        ProductsModel.instance().add_node("test/some/path2".split("/"),node)

    w.show()
    app.exec()
