"""Reproducers for products model contract violations
(docs/backlog-2026-06-10.md H1 + H2 + H3).

H1: re-publishing a product with an existing name silently removed the old
node inside ``ProductsModelNode::add_child`` (a structural removal with no
``beginRemoveRows``) while ``_insert_node`` had already announced an insert —
views/proxies end up believing one more row exists than the node holds.

H2: ``ProductsView`` computed the "%1 results" label synchronously after
``set_query``, but the flat filter model defers all matching to zero-interval
timer batches — the label permanently read "0 results".

H3: the flat filter model's final score-sort swapped ``m_results`` under
``layoutAboutToBeChanged``/``layoutChanged`` without remapping persistent
indexes — a selection taken while batches streamed silently retargeted to a
different product after the sort.
"""
import uuid

import pytest
from PySide6.QtCore import QCoreApplication, QPersistentModelIndex, Qt
from PySide6.QtWidgets import QLabel, QTextEdit

from SciQLopPlots import (
    ParameterType,
    ProductsFlatFilterModel,
    ProductsModel,
    ProductsModelNode,
    ProductsModelNodeType,
    ProductsView,
    QueryParser,
)


def _flush(n=10):
    for _ in range(n):
        QCoreApplication.processEvents()


def _make_leaf(name, provider):
    return ProductsModelNode(
        name, provider, {"uid": name},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)


def _drain_flat_model(fm, qtbot, timeout=5000):
    """Pump events until the zero-interval batch loop has consumed all leaves.
    No public "done" flag: pump until two consecutive rounds add no rows."""
    last = -1
    for _ in range(500):
        _flush(2)
        cur = fm.rowCount()
        if cur == last:
            # one more confirmation round (sort emits layoutChanged at the end)
            _flush(5)
            if fm.rowCount() == cur:
                return
        last = cur
    pytest.fail("flat filter model never settled")


class TestRepublishRowAccounting:
    def test_republish_same_name_keeps_row_accounting_consistent(self, qtbot):
        """H1: the net row change announced via rowsInserted/rowsRemoved must
        equal the actual rowCount change when a same-named node is re-added."""
        model = ProductsModel.instance()
        name = f"dup_provider_{uuid.uuid4().hex[:8]}"

        inserted, removed = [], []

        def on_inserted(parent, first, last):
            inserted.append(last - first + 1)

        def on_removed(parent, first, last):
            removed.append(last - first + 1)

        model.rowsInserted.connect(on_inserted)
        model.rowsRemoved.connect(on_removed)
        try:
            n0 = model.rowCount()
            model.add_node([], ProductsModelNode(name))
            model.add_node([], ProductsModelNode(name))  # re-publish, same name
            n1 = model.rowCount()
            assert n1 - n0 == sum(inserted) - sum(removed), (
                f"model announced +{sum(inserted)}/-{sum(removed)} rows but "
                f"rowCount changed by {n1 - n0}: views are now desynced"
            )
        finally:
            model.rowsInserted.disconnect(on_inserted)
            model.rowsRemoved.disconnect(on_removed)


class TestResultCountLabel:
    def test_result_count_label_reflects_async_results(self, qtbot):
        """H2: after typing a query, the results label must eventually show
        the real (non-zero) match count, not the pre-batch zero."""
        token = f"h2tok{uuid.uuid4().hex[:6]}"
        provider = ProductsModelNode(f"h2_provider_{token}")
        for i in range(3):
            provider.add_child(_make_leaf(f"{token}_product_{i}", "h2prov"))
        ProductsModel.instance().add_node([], provider)

        view = ProductsView()
        qtbot.addWidget(view)
        view.show()

        bar = view.findChild(QTextEdit)
        assert bar is not None
        bar.setPlainText(token)

        labels = view.findChildren(QLabel)

        def count_label_text():
            _flush(2)
            for lab in labels:
                if "result" in lab.text():
                    return lab.text()
            return ""

        # debounce (150 ms) + batches; the label must end up showing 3 results
        qtbot.waitUntil(lambda: count_label_text().startswith("3 "), timeout=5000)


class TestPersistentIndexAcrossSort:
    def test_persistent_index_survives_final_score_sort(self, qtbot):
        """H3: a persistent index taken while batches stream must still point
        at the same product after the final score sort."""
        token = f"h3zzz{uuid.uuid4().hex[:6]}"
        provider = ProductsModelNode(f"h3_provider_{token}")
        # 250 weak matches (> BATCH_SIZE=200, so they span two batches) ...
        for i in range(250):
            provider.add_child(_make_leaf(f"aaa_{token}_{i:03d}", "h3prov"))
        # ... and one exact match, landing in the SECOND batch with the top score
        provider.add_child(_make_leaf(token, "h3prov"))
        model = ProductsModel.instance()
        model.add_node([], provider)

        fm = ProductsFlatFilterModel(model)
        fm.set_query(QueryParser.parse(token))

        # let exactly the first batch land (rows exist, processing not finished)
        qtbot.waitUntil(lambda: (_flush(1), fm.rowCount() > 0)[1], timeout=5000)

        first = QPersistentModelIndex(fm.index(0, 0))
        first_name = fm.data(fm.index(0, 0), Qt.DisplayRole)
        assert first.isValid()

        _drain_flat_model(fm, qtbot)

        assert first.isValid(), "persistent index dropped by final sort"
        assert fm.data(fm.index(first.row(), 0), Qt.DisplayRole) == first_name, (
            "persistent index points at a different product after the score "
            "sort (selection/drag would target the wrong product)"
        )
