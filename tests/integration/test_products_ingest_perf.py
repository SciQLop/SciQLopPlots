"""Regression: bulk product ingest must not cost one full rebuild per product.

ProductsFlatFilterModel rebuilt (beginResetModel + full corpus walk) on every
rowsInserted, so loading N products cost O(N^2) tree walks. The rebuild is now
coalesced into one pass per event-loop turn.
"""
import uuid

from PySide6.QtCore import QCoreApplication
from SciQLopPlots import (
    ProductsModel, ProductsModelNode, ProductsModelNodeType, ParameterType,
    ProductsFlatFilterModel,
)


def flush_events():
    for _ in range(10):
        QCoreApplication.processEvents()


def _make_product(provider, name):
    root = ProductsModelNode(provider)
    leaf = ProductsModelNode(
        name, provider,
        {"uid": name,
         "start_date": "2020-01-01T00:00:00Z",
         "stop_date": "2024-12-31T23:59:59Z"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    root.add_child(leaf)
    return root


class TestFlatModelIngestCoalescing:

    def test_bulk_ingest_triggers_single_reset(self, qtbot):
        model = ProductsModel.instance()
        fm = ProductsFlatFilterModel(model)
        resets = []
        fm.modelReset.connect(lambda: resets.append(1))

        tag = uuid.uuid4().hex[:8]
        for i in range(20):
            model.add_node([], _make_product(f"bulk_{tag}_{i}", f"P{i}"))
        flush_events()

        assert len(resets) == 1, f"expected 1 coalesced rebuild, got {len(resets)}"
        assert fm.rowCount() >= 20
