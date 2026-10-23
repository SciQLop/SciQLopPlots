"""Every bound constructor with a `parent` argument gives the new object to that parent.

docs/plans/2026-10-23-widget-ownership-design.md, change 1. The walk reads the bound signatures
(`parent` is not always last), so a class bound later with a `parent` argument fails here until
it has a builder below and its <parent> markup in bindings.xml. Drawn objects (SciQLopDrawnObject
and its subclasses: items, graphs, the legend, panel-wide objects) are part B of the ownership
work and are left out.
"""
import pytest
import shiboken6
from PySide6.QtCore import QObject
from PySide6.QtWidgets import QWidget

import SciQLopPlots as sqp
from conftest import collect, on_destroyed
from SciQLopPlots import SciQLopDrawnObject, SciQLopPlotsBindings
from test_widget_ownership import WIDGETS
# After SciQLopPlots: importing it loads shiboken's signature support.
from shibokensupport.signature import get_signature

WIDGET_CLASSES = [name for name in WIDGETS if "," not in name]  # one row each, shared with the widget tests

_KEEP = []  # side objects a builder needs alive for the whole session


def _kept(obj):
    _KEEP.append(obj)
    return obj


def _subclass(cls):
    """A Python subclass, for a class shiboken refuses to build (C++ abstract)."""
    return type(f"{cls.__name__}ForTest", (cls,), {})


BUILDERS = {
    **{name: [WIDGETS[name]] for name in WIDGET_CLASSES},
    "SciQLopPlotAxisInterface": [lambda parent: sqp.SciQLopPlotAxisInterface(parent)],
    "SciQLopPlotDummyAxis": [lambda parent: sqp.SciQLopPlotDummyAxis(parent)],
    "SciQLopPlotCollectionBehavior": [lambda parent: sqp.SciQLopPlotCollectionBehavior(parent)],
    "VPlotsAlign": [lambda parent: sqp.VPlotsAlign(parent)],
    "DataProviderInterface": [lambda parent: sqp.DataProviderInterface(parent)],
    "RemoteDataPipeline": [lambda parent: sqp.RemoteDataPipeline(parent)],
    "DataProviderWorker": [lambda parent: sqp.DataProviderWorker(parent)],
    "PlotDragNDropCallback": [lambda parent: sqp.PlotDragNDropCallback("text/plain", False, parent)],
    "PlotsModel": [lambda parent: sqp.PlotsModel(parent)],
    "InspectorExtension": [lambda parent: _subclass(sqp.InspectorExtension)(parent)],
    "ProductsTreeFilterModel": [lambda parent: sqp.ProductsTreeFilterModel(parent)],
    "ProductsFlatFilterModel": [
        lambda parent: sqp.ProductsFlatFilterModel(_kept(sqp.ProductsModel()), parent)],
    "ProductsModel": [lambda parent: sqp.ProductsModel(parent)],
    "ProductsModelNode": [
        lambda parent: sqp.ProductsModelNode("node", {}, "", parent),
        lambda parent: sqp.ProductsModelNode("node", "provider", {},
                                             sqp.ProductsModelNodeType.PARAMETER,
                                             sqp.ParameterType.Scalar, "", parent),
    ],
    "Icons": [lambda parent: sqp.Icons(parent)],
}

CASES = [(name, index) for name, builders in BUILDERS.items() for index in range(len(builders))]


def _signatures(cls):
    # shiboken lists None for an overload it can't describe; a None here would stop the walk.
    found = get_signature(cls)
    found = found if isinstance(found, list) else [found]
    return [s for s in found if s is not None]


def _parent_constructor_counts():
    counts = {}
    for name, cls in vars(SciQLopPlotsBindings).items():
        if isinstance(cls, type) and issubclass(cls, QObject) and not issubclass(cls, SciQLopDrawnObject):
            count = sum("parent" in s.parameters for s in _signatures(cls))
            if count:
                counts[name] = count
    return counts


def test_every_parent_constructor_has_a_builder():
    assert {name: len(b) for name, b in BUILDERS.items()} == _parent_constructor_counts()


@pytest.mark.parametrize("name,index", CASES, ids=[f"{n}-{i}" for n, i in CASES])
def test_an_object_built_with_a_parent_belongs_to_it(qtbot, name, index):
    host = QWidget()
    obj = BUILDERS[name][index](host)
    obj.setObjectName("probe")
    gone = on_destroyed(obj)
    assert (obj.parent() is host, shiboken6.ownedByPython(obj)) == (True, False)
    del obj
    collect()
    assert (gone, host.findChild(QObject, "probe") is not None) == ([], True)
    shiboken6.delete(host)
    assert gone == [True]
