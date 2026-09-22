"""Reproducers for SciQLop issue #138: ``ProductsModel::add_node`` called from a
non-GUI thread (kernel thread) mutated the model there. Its signals then reached
the filter models as queued calls, *after* a replaced node had been deleted, and
a later query change dereferenced the freed node.

The model must apply every mutation on its own thread, without blocking the caller.
"""
import threading
import uuid

import shiboken6
from PySide6.QtCore import QCoreApplication, QThread, Qt

from SciQLopPlots import ProductsModel, ProductsModelNode


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
        result = []

        def add():
            parent = ProductsModelNode(f"{name}_parent")
            child = ProductsModelNode(name)
            parent.add_child(child)
            keep.extend([parent, child])
            result.append(model.add_node([], child))

        n0 = model.rowCount()
        _run_in_worker(add)
        _flush(20)
        assert model.rowCount() == n0
        assert ProductsModel.node([name]) is None
        assert result == [False]
        # Refused: the child's owner must stay its ProductsModelNode parent, not the
        # model (bindings.xml only hands ownership to the model when add_node
        # returns True).
        assert keep[1].parent() is keep[0]


    def test_node_built_on_a_worker_but_added_from_the_model_thread_is_refused(self, qtbot):
        """Only the worker-thread call was checked: a node made off-thread and added on
        the model thread was appended to the tree, but Qt refused to parent it, so the
        model listed a node it did not own."""
        model = ProductsModel.instance()
        name = f"split_thread_{uuid.uuid4().hex[:8]}"
        built = []
        _run_in_worker(lambda: built.append(ProductsModelNode(name)))
        n0 = model.rowCount()
        result = model.add_node([], built[0])
        _flush(20)
        assert model.rowCount() == n0
        assert ProductsModel.node([name]) is None
        assert result is False
        assert shiboken6.Shiboken.ownedByPython(built[0])

    def test_accepted_node_is_owned_by_the_model(self, qtbot):
        model = ProductsModel.instance()
        name = f"accepted_owner_{uuid.uuid4().hex[:8]}"
        node = ProductsModelNode(name)
        result = model.add_node([], node)
        assert result is True
        assert not shiboken6.Shiboken.ownedByPython(node)
