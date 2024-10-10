from SciQLopPlots import SciQLopPlot, SciQLopMultiPlotPanel, ProductsModel, Icons, ProductsModelNode, ProductsModelNodeType, ProductsView, ParameterType
from PySide6.QtWidgets import QMainWindow, QApplication
from PySide6.QtCore import Qt
import sys
import os
import json

os.environ['QT_API'] = 'PySide6'

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from common import MainWindow

__HERE__ = os.path.dirname(__file__)


def filter_metadata(json_node):
    meta = {}
    for k, v in json_node.items():
        if k.startswith("__spz_"):
            continue
        if not isinstance(v, dict):
            meta[k] = v
    return meta

def filter_children(json_node):
    children = {}
    for k, v in json_node.items():
        if isinstance(v, dict):
            children[k] = v
    return children

def load_node(path, json_node):
    if json_node["__spz_type__"] == "ParameterIndex":
        node = ProductsModelNode(json_node["__spz_uid__"],"", filter_metadata(json_node), ProductsModelNodeType.PARAMETER, ParameterType.Multicomponents, "", None)
    else:
        node = ProductsModelNode(json_node["__spz_uid__"], filter_metadata(json_node), "folder_open")

    ProductsModel.instance().add_node(path, node)
    for k, v in filter_children(json_node).items():
        load_node(path + [k], v)

def load_products():
    with open(os.path.join(__HERE__, "products.json")) as f:
        products = json.load(f)
    load_node([], products)


if __name__ == '__main__':
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.add_tab(ProductsView(w), "Products")
    load_products()

    w.show()
    app.exec()
