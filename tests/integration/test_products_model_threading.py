"""Reproducers for SciQLop issue #138: ``ProductsModel::add_node`` called from a
non-GUI thread (kernel thread) mutated the model there. Its signals then reached
the filter models as queued calls, *after* a replaced node had been deleted, and
a later query change dereferenced the freed node.

The model must apply every mutation on its own thread, without blocking the caller.
"""
import threading
import uuid

from PySide6.QtCore import QCoreApplication, QThread, Qt

from SciQLopPlots import (
    ParameterType,
    ProductsModel,
    ProductsModelNode,
    ProductsModelNodeType,
    ProductsTreeFilterModel,
    QueryParser,
    ScoreMergeStrategy,
)


def _flush(n=10):
    for _ in range(n):
        QCoreApplication.processEvents()


def _run_in_worker(fn):
    worker = threading.Thread(target=fn)
    worker.start()
    worker.join(timeout=5)
    assert not worker.is_alive(), "add_node from a worker thread blocked"


def _record_signal_threads(model):
    threads = []
    record = lambda *_: threads.append(QThread.currentThread())
    model.rowsAboutToBeInserted.connect(record, Qt.DirectConnection)
    model.rowsAboutToBeRemoved.connect(record, Qt.DirectConnection)
    return threads, record


class TestAddNodeFromWorkerThread:
    def test_node_is_added_and_signals_fire_on_the_model_thread(self, qtbot):
        model = ProductsModel.instance()
        name = f"worker_add_{uuid.uuid4().hex[:8]}"
        threads, record = _record_signal_threads(model)
        try:
            n0 = model.rowCount()
            _run_in_worker(lambda: model.add_node([], ProductsModelNode(name)))
            qtbot.waitUntil(lambda: (_flush(2), model.rowCount() == n0 + 1)[1], timeout=5000)
            assert threads and all(t == model.thread() for t in threads)
        finally:
            model.rowsAboutToBeInserted.disconnect(record)
            model.rowsAboutToBeRemoved.disconnect(record)

    def test_replacing_a_node_from_a_worker_thread_keeps_row_accounting(self, qtbot):
        model = ProductsModel.instance()
        name = f"worker_replace_{uuid.uuid4().hex[:8]}"
        model.add_node([], ProductsModelNode(name))
        threads, record = _record_signal_threads(model)
        try:
            n0 = model.rowCount()
            _run_in_worker(lambda: model.add_node([], ProductsModelNode(name)))
            qtbot.waitUntil(lambda: (_flush(2), len(threads) >= 2)[1], timeout=5000)
            assert model.rowCount() == n0
            assert all(t == model.thread() for t in threads)
        finally:
            model.rowsAboutToBeInserted.disconnect(record)
            model.rowsAboutToBeRemoved.disconnect(record)

    def test_node_is_inserted_and_owned_by_the_model_thread(self, qtbot):
        model = ProductsModel.instance()
        name = f"worker_owner_{uuid.uuid4().hex[:8]}"
        node = {}

        def add():
            node["n"] = ProductsModelNode(name)
            model.add_node([], node["n"])

        _run_in_worker(add)
        qtbot.waitUntil(lambda: (_flush(2), ProductsModel.node([name]) is not None)[1], timeout=5000)
        assert node["n"].thread() == model.thread()
        assert node["n"].parent() is not None
        assert node["n"].parent().thread() == model.thread()

    def test_node_that_cannot_be_moved_is_refused(self, qtbot):
        model = ProductsModel.instance()
        name = f"worker_parented_{uuid.uuid4().hex[:8]}"
        keep = []

        def add():
            parent = ProductsModelNode(f"{name}_parent")
            child = ProductsModelNode(name)
            parent.add_child(child)
            keep.extend([parent, child])
            model.add_node([], child)

        n0 = model.rowCount()
        _run_in_worker(add)
        _flush(20)
        assert model.rowCount() == n0
        assert ProductsModel.node([name]) is None


class TestReplaceWhileQueryIsActive:
    def test_worker_replace_then_remerge_does_not_touch_freed_nodes(self, qtbot):
        """#138 sequence: a filter model holds scores for a product, the product is
        re-registered from a worker thread, then the merge strategy changes and
        walks every scored node. Crash-based, so a regression may show as a
        segfault rather than an assertion."""
        token = f"tok{uuid.uuid4().hex[:8]}"
        model = ProductsModel.instance()

        def make_provider():
            provider = ProductsModelNode(f"{token}_provider")
            provider.add_child(ProductsModelNode(
                f"{token}_leaf", "prov", {"uid": token},
                ProductsModelNodeType.PARAMETER, ParameterType.Scalar))
            return provider

        model.add_node([], make_provider())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_query(QueryParser.parse(token))
        qtbot.waitUntil(lambda: (_flush(2), fm.rowCount() > 0)[1], timeout=5000)

        for _ in range(5):
            _run_in_worker(lambda: model.add_node([], make_provider()))
            _flush(20)
        qtbot.wait(300)
        assert fm.rowCount() == 1
        fm.set_score_merge_strategy(ScoreMergeStrategy.Override)
        _flush(20)
