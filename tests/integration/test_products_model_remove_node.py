"""``ProductsModel::remove_node``: SciQLop publishes virtual products with ``add_node``
and had no way to take them out again.

Removal must be announced to the filter models before the node is freed, and a call
from a non-GUI thread must be applied on the model thread without blocking.
"""
import threading
import uuid

import pytest
from PySide6.QtCore import QCoreApplication

from SciQLopPlots import (
    ParameterType,
    ProductsFlatFilterModel,
    ProductsModel,
    ProductsModelNode,
    ProductsModelNodeType,
    ProductsTreeFilterModel,
    QueryParser,
)


def _flush(n=10):
    for _ in range(n):
        QCoreApplication.processEvents()


def _unique(prefix):
    return f"{prefix}_{uuid.uuid4().hex[:8]}"


def _leaf(name, provider="rm_provider"):
    return ProductsModelNode(
        name, provider,
        {"uid": name, "start_date": "2020-01-01T00:00:00Z", "stop_date": "2024-12-31T23:59:59Z"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)


def _names(model, parent=None):
    from PySide6.QtCore import QModelIndex, Qt
    parent = QModelIndex() if parent is None else parent
    out = []
    for row in range(model.rowCount(parent)):
        idx = model.index(row, 0, parent)
        out.append(model.data(idx, Qt.DisplayRole))
        out.extend(_names(model, idx))
    return out


def _run_in_worker(fn):
    worker = threading.Thread(target=fn)
    worker.start()
    worker.join(timeout=5)
    assert not worker.is_alive(), "remove_node from a worker thread blocked"


@pytest.fixture
def model(qtbot):
    return ProductsModel.instance()


class TestRemoveOnTheModelThread:
    def test_a_leaf_goes_away(self, model):
        name = _unique("rm_leaf")
        model.add_node([], _leaf(name))
        n0 = model.rowCount()
        assert ProductsModel.node([name]) is not None
        model.remove_node([name])
        assert ProductsModel.node([name]) is None
        assert model.rowCount() == n0 - 1

    def test_a_folder_takes_its_subtree_with_it(self, model):
        folder, leaf = _unique("rm_folder"), _unique("rm_child")
        model.add_node([folder], _leaf(leaf))
        assert ProductsModel.node([folder, leaf]) is not None
        model.remove_node([folder])
        assert ProductsModel.node([folder]) is None
        assert ProductsModel.node([folder, leaf]) is None

    def test_a_nested_leaf_leaves_its_folder_in_place(self, model):
        folder, leaf = _unique("rm_keep"), _unique("rm_only")
        model.add_node([folder], _leaf(leaf))
        model.remove_node([folder, leaf])
        assert ProductsModel.node([folder, leaf]) is None
        assert ProductsModel.node([folder]) is not None

    def test_a_path_read_from_a_node_removes_it(self, model):
        name = _unique("rm_roundtrip")
        model.add_node([], _leaf(name))
        model.remove_node(ProductsModel.node([name]).path())
        assert ProductsModel.node([name]) is None

    def test_missing_empty_and_root_paths_change_nothing(self, model):
        name = _unique("rm_untouched")
        model.add_node([], _leaf(name))
        n0 = model.rowCount()
        for path in ([], [""], ["root"], [_unique("nothing_here")], [name, "no_child"]):
            model.remove_node(path)
        assert model.rowCount() == n0
        assert ProductsModel.node([name]) is not None

    def test_removing_twice_is_harmless(self, model):
        name = _unique("rm_twice")
        model.add_node([], _leaf(name))
        model.remove_node([name])
        model.remove_node([name])
        assert ProductsModel.node([name]) is None

    def test_the_wrapper_of_a_removed_node_is_invalidated(self, model):
        name = _unique("rm_wrapper")
        model.add_node([], _leaf(name))
        node = ProductsModel.node([name])
        model.remove_node([name])
        with pytest.raises(RuntimeError):
            node.name()


class TestFilterModelsFollow:
    def test_the_flat_filter_can_be_read_right_after_a_removal(self, model):
        name = _unique("RmFlat")
        fm = ProductsFlatFilterModel(model)
        model.add_node([], _leaf(name))
        _flush()
        assert name in _names(fm)
        model.remove_node([name])
        _names(fm)  # no event-loop turn yet: its results must not name the freed node
        _flush()
        assert name not in _names(fm)

    def test_the_tree_filter_does_not_keep_scores_of_a_freed_node(self, model):
        provider, name = _unique("rm_prov"), _unique("RmTree")
        root = ProductsModelNode(provider)
        root.add_child(_leaf(name, provider))
        model.add_node([], root)
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_query(QueryParser.parse(name))
        _flush()
        assert fm.rowCount() > 0
        model.remove_node([provider])
        from PySide6.QtCore import QByteArray
        _churn = [QByteArray(b"\x41" * 128) for _ in range(2000)]
        fm.set_max_score_tiers(3)
        fm.set_max_score_tiers(2)
        _flush()
        assert name not in _names(fm)


class TestRemoveFromAWorkerThread:
    def test_the_removal_lands_on_the_model_thread(self, qtbot, model):
        name = _unique("rm_worker")
        model.add_node([], _leaf(name))
        _run_in_worker(lambda: model.remove_node([name]))
        qtbot.waitUntil(lambda: (_flush(2), ProductsModel.node([name]) is None)[1], timeout=5000)

    def test_add_then_remove_from_one_thread_keeps_their_order(self, qtbot, model):
        name = _unique("rm_order")

        def add_then_remove():
            model.add_node([], _leaf(name))
            model.remove_node([name])

        _run_in_worker(add_then_remove)
        qtbot.wait(200)
        _flush()
        assert ProductsModel.node([name]) is None

    def test_remove_then_add_from_one_thread_keeps_their_order(self, qtbot, model):
        name = _unique("rm_order2")
        model.add_node([], _leaf(name))

        def remove_then_add():
            model.remove_node([name])
            model.add_node([], _leaf(name))

        _run_in_worker(remove_then_add)
        qtbot.waitUntil(lambda: (_flush(2), ProductsModel.node([name]) is not None)[1], timeout=5000)
