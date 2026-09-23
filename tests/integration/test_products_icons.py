"""Changing a published node's icon must reach the views (issue #111).

SciQLop gives product nodes its own icons (virtual products, per-provider...):
10 to 50 icons over ~100k nodes. set_icon() on a node already in the model
changed the name it looks up but told nobody, so the tree and the flat search
results kept showing the old icon.
"""
import time
import uuid

from PySide6.QtCore import QCoreApplication, Qt
from PySide6.QtGui import QColor, QIcon, QPixmap
from SciQLopPlots import (
    Icons, ParameterType, ProductsFlatFilterModel, ProductsModel, ProductsModelNode,
    ProductsModelNodeType, ProductsView,
)

N_ICONS = 50


def _flush(n=10):
    for _ in range(n):
        QCoreApplication.processEvents()


def _register_icons():
    for i in range(N_ICONS):
        pm = QPixmap(16, 16)
        pm.fill(QColor.fromHsv(i * 7 % 360, 200, 200))
        Icons.add_icon(f"test_icon_{i}", QIcon(pm))


def _leaf(name, icon=""):
    return ProductsModelNode(name, "icons_test", {}, ProductsModelNodeType.PARAMETER,
                             ParameterType.Scalar, icon)


def _publish(folders, per_folder):
    """A fresh top-level folder of `folders` x `per_folder` leaves, icons cycling."""
    top = f"icons_{uuid.uuid4().hex[:8]}"
    root = ProductsModelNode(top)
    leaves = []
    for f in range(folders):
        folder = ProductsModelNode(f"f{f}")
        for p in range(per_folder):
            leaf = _leaf(f"p{f}_{p}", f"test_icon_{len(leaves) % N_ICONS}")
            folder.add_child(leaf)
            leaves.append(leaf)
        root.add_child(folder)
    assert ProductsModel.instance().add_node([], root)
    _flush()
    return top, leaves


def _changed_rows(model):
    changes = []
    model.dataChanged.connect(
        lambda first, last, roles: changes.append((first, last, list(roles))))
    return changes


class TestSetIconOnAPublishedNode:
    def setup_method(self):
        _register_icons()

    def test_the_model_announces_the_new_icon(self, qtbot):
        model = ProductsModel.instance()
        top, leaves = _publish(1, 3)
        changes = _changed_rows(model)
        leaves[1].set_icon("test_icon_7")
        _flush()
        rows = [(first.row(), last.row()) for first, last, roles in changes
                if Qt.DecorationRole in roles or not roles]
        assert (1, 1) in rows
        model.remove_node([top])

    def test_the_flat_results_announce_it_too(self, qtbot):
        model = ProductsModel.instance()
        flat = ProductsFlatFilterModel(model)
        top, leaves = _publish(1, 3)
        _flush(20)
        changes = _changed_rows(flat)
        leaves[0].set_icon("test_icon_9")
        _flush()
        assert any(Qt.DecorationRole in roles or not roles for _, _, roles in changes)
        model.remove_node([top])

    def test_the_row_follows_a_sibling_removal(self, qtbot):
        model = ProductsModel.instance()
        top, leaves = _publish(1, 5)
        model.remove_node([top, "f0", "p0_1"])
        _flush()
        changes = _changed_rows(model)
        leaves[4].set_icon("test_icon_11")
        _flush()
        assert [(first.row(), last.row()) for first, last, _ in changes] == [(3, 3)]
        model.remove_node([top])

    def test_a_node_not_yet_published_stays_quiet(self, qtbot):
        leaf = _leaf("unpublished", "test_icon_1")
        leaf.set_icon("test_icon_2")


class TestIconsAtScale:
    """~100k nodes, 50 icons, a ProductsView open: re-iconing everything must stay cheap.

    One flat folder is the worst shape: finding a node's row was a linear scan of its
    siblings, so re-iconing a whole folder cost O(width^2)."""

    def test_reiconing_100k_published_nodes_stays_under_a_second(self, qtbot):
        _register_icons()
        model = ProductsModel.instance()
        view = ProductsView()
        qtbot.addWidget(view)
        top, leaves = _publish(1, 100_000)
        _flush(20)
        start = time.perf_counter()
        for i, leaf in enumerate(leaves):
            leaf.set_icon(f"test_icon_{(i + 1) % N_ICONS}")
        _flush(20)
        elapsed = time.perf_counter() - start
        model.remove_node([top])
        _flush()
        assert elapsed < 1.0, f"re-iconing 100k nodes took {elapsed:.2f}s"
