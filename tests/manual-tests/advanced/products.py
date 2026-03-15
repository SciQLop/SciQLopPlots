"""Hierarchical product tree browser.

Showcases:
- ProductsModel for hierarchical data products
- ProductsView for browsing and drag-drop
"""
import sys
import os
import json
from PySide6.QtWidgets import QApplication, QMainWindow
from PySide6.QtCore import Qt

from SciQLopPlots import (
    ProductsModel, ProductsModelNode, ProductsModelNodeType,
    ProductsView, ParameterType,
)

__HERE__ = os.path.dirname(__file__)
PRODUCTS_JSON = os.path.join(__HERE__, "..", "Products", "products.json")


def filter_metadata(json_node):
    return {k: v for k, v in json_node.items() if not k.startswith("__spz_") and not isinstance(v, dict)}


def filter_children(json_node):
    return {k: v for k, v in json_node.items() if isinstance(v, dict)}


def load_node(path, json_node):
    if json_node["__spz_type__"] == "ParameterIndex":
        node = ProductsModelNode(
            json_node["__spz_uid__"], "", filter_metadata(json_node),
            ProductsModelNodeType.PARAMETER, ParameterType.Multicomponents, "", None,
        )
    else:
        node = ProductsModelNode(json_node["__spz_uid__"], filter_metadata(json_node), "folder_open")
    ProductsModel.instance().add_node(path, node)
    for k, v in filter_children(json_node).items():
        load_node(path + [k], v)


app = QApplication(sys.argv)

with open(PRODUCTS_JSON) as f:
    load_node([], json.load(f))

w = QMainWindow()
w.setWindowTitle("Products Browser Demo")
w.setCentralWidget(ProductsView(w))
w.resize(600, 500)
w.show()
sys.exit(app.exec())
